#include "TerminalWidget.h"
#include "TerminalModel.h"
#include "Settings.h"

#include <QSocketNotifier>
#include <QByteArray>
#include <QStringDecoder>
#include <QClipboard>
#include <unistd.h>



std::vector<std::string> preferred_fonts = {
    "JetBrainsMono Nerd Font",
    "SpaceMono Nerd Font",
    "Hack Nerd Font",
    "Fira Mono",
    "Fira Code",
    "Monospace",
};

struct Atlas {
    std::vector<unsigned char> data;
    int width;
    int height;
};

Atlas buildAtlas(FT_Face face, bool italic,
                 int cellW, int cellH,
                 int glyphCols, int glyphRows)
{
    const int firstChar = 32;
    const int lastChar  = 126;

    int atlasW = glyphCols * cellW;
    int atlasH = glyphRows * cellH;

    std::vector<unsigned char> atlas(atlasW * atlasH, 0);

    FT_Matrix matrix = {
        1 << 16,
        (int)(0.3 * (1 << 16)), // shear X
        0,
        1 << 16
    };

    if (italic) {
        FT_Set_Transform(face, &matrix, nullptr);
    } else {
        FT_Set_Transform(face, nullptr, nullptr);
    }

    int baseline = face->size->metrics.ascender >> 6;

    for (int c = firstChar; c <= lastChar; ++c) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
            continue;

        FT_GlyphSlot g = face->glyph;

        int idx = c - firstChar;
        int gx = (idx % glyphCols) * cellW;
        int gy = (idx / glyphCols) * cellH;

        int dest_x = gx + 1 + g->bitmap_left;
        int dest_y = gy + baseline - g->bitmap_top;

        for (int y = 0; y < g->bitmap.rows; ++y) {
            int ay = dest_y + y;
            if (ay < 0 || ay >= atlasH) continue;

            for (int x = 0; x < g->bitmap.width; ++x) {
                int ax = dest_x + x;
                if (ax < 0 || ax >= atlasW) continue;

                unsigned char src =
                    g->bitmap.buffer[y * g->bitmap.pitch + x];

                atlas[ay * atlasW + ax] = std::max(
                    atlas[ay * atlasW + ax],
                    src
                );
            }
        }
    }

    return { std::move(atlas), atlasW, atlasH };
}

auto uploadAtlas = [](GLuint& tex, const Atlas& a) {
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
        a.width, a.height, 0,
        GL_RED, GL_UNSIGNED_BYTE, a.data.data());
};


TerminalWidget::TerminalWidget(QWidget* parent)
    : QOpenGLWidget(parent),
      m_glyphCache(m_charWidth, m_charHeight, 4096, 4096)
{
    setFocusPolicy(Qt::StrongFocus);

    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    cursorTimer = new QTimer(this);
    connect(cursorTimer, &QTimer::timeout, this, [this]() {
        cursorVisible = !cursorVisible;
        update();
    });
    cursorTimer->start(500);

    auto& s = Settings::data();
    m_cols = s.terminalWidth;
    m_rows = s.terminalHeight;
}

TerminalWidget::~TerminalWidget() {
    if (m_glyphUVTexture) glDeleteTextures(1, &m_glyphUVTexture);
    if (attrFgTexture) glDeleteTextures(1, &attrFgTexture);
    if (attrBgTexture) glDeleteTextures(1, &attrBgTexture);
    if (textTexture) glDeleteTextures(1, &textTexture);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

void TerminalWidget::initializeGL() {
    initializeOpenGLFunctions();

    // --- Shaders ---
    program.addShaderFromSourceCode(QOpenGLShader::Vertex, R"(
#version 330 core
layout (location = 0) in vec2 Position;

out vec2 UV;

void main()
{
    UV = (Position + 1.0) * 0.5;
    gl_Position = vec4(Position, 0.0, 1.0);
}
    )");

    program.addShaderFromSourceCode(QOpenGLShader::Fragment, R"(
#version 330 core

in vec2 UV;
out vec4 fragColor;

uniform usampler2D TextBuffer;
uniform usampler2D AttrFGBuffer;
uniform usampler2D AttrBGBuffer;
uniform float BackgroundOpacity;

uniform sampler2D GlyphAtlas;
uniform sampler2D GlyphUVs;

uniform ivec2 CellSize;
uniform ivec2 CursorPos;
uniform bool CursorVisible;
uniform vec3 CursorColor;
uniform int CursorStyle;

uniform ivec2 SelStart;
uniform ivec2 SelEnd;
uniform bool HasSelection;
uniform vec3 SelColor;


bool inSelection(ivec2 cell, ivec2 screen_size) {
    if (!HasSelection) return false;
    int cellIdx = cell.y * screen_size.x + cell.x;
    int selStartIdx = SelStart.y * screen_size.x + SelStart.x;
    int selEndIdx   = SelEnd.y   * screen_size.x + SelEnd.x;
    return cellIdx >= selStartIdx && cellIdx <= selEndIdx;
}

void main() {
    vec2 uv = UV;
    uv.y = 1.0 - uv.y;

    ivec2 screenSize = textureSize(TextBuffer, 0);
    
    vec2 grid_uv = uv * vec2(screenSize);
    ivec2 cell = ivec2(grid_uv);
    vec2 cell_uv = fract(grid_uv);

    
    uvec4 fgPack = texelFetch(AttrFGBuffer, cell, 0);
    uvec4 bgPack = texelFetch(AttrBGBuffer, cell, 0);
    
    uint flags = fgPack.a | (bgPack.a << 8u);
    
    vec4 fg = vec4(fgPack.rgb, 255.0) / 255.0;
    vec4 bg = vec4(bgPack.rgb, 255.0) / 255.0;
    
    if ((flags & 16u) != 0u) {
        vec4 t = fg; fg = bg; bg = t;
    }
        
    // Normal char
    vec2 sample_uv = cell_uv;
    vec4 uvRect = texelFetch(GlyphUVs, cell, 0);
    bool is_wide = (uvRect.z-uvRect.x)*150.0>0.5;
    if (is_wide) sample_uv.x = cell_uv.x * 0.5;

    vec2 atlasCoord = mix(uvRect.xy, uvRect.zw, sample_uv);
    float alpha = texture(GlyphAtlas, atlasCoord).r;

    // Neighbor char
    if (cell.x > 0) {
        vec2 n_sample_uv = cell_uv;
        vec4 n_uvRect = texelFetch(GlyphUVs, cell-ivec2(1,0), 0);
        bool n_is_wide = (n_uvRect.z-n_uvRect.x)*150.0>0.5;
        if (n_is_wide) {
            n_sample_uv.x = cell_uv.x * 0.5 + 0.5;
            vec2 n_atlasCoord = mix(n_uvRect.xy, n_uvRect.zw, n_sample_uv);
            alpha += texture(GlyphAtlas, n_atlasCoord).r;
        }
    }
    alpha = clamp(alpha, 0.0, 1.0);

    bg.a = BackgroundOpacity;
    vec4 color;
    if (cell == CursorPos && CursorVisible) {
        ivec2 local_pos = ivec2(cell_uv * CellSize);
        vec4 cursor_color = mix(vec4(CursorColor, BackgroundOpacity), vec4(bg.rgb, 1.0), alpha);
        vec4 normal_color = mix(bg, fg, alpha);
        if (CursorStyle == 0) color = cursor_color;
        else if (CursorStyle == 1) color = local_pos.x==0 ? cursor_color : normal_color;
        else if (CursorStyle == 2) color = local_pos.y==CellSize.y-1 ? cursor_color : normal_color;
    } else { color = mix(bg, fg, alpha); }
    if (inSelection(cell, screenSize)) color.rgb += SelColor * 0.2;

    fragColor = vec4(color.rgba);
}
    )");

    if (!program.link()) {
        qFatal("Shader link failed: %s", qPrintable(program.log()));
    }

    float vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,

        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenTextures(1, &textTexture);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI,
                m_cols, m_rows,
                0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    glGenTextures(1, &attrFgTexture);
    glBindTexture(GL_TEXTURE_2D, attrFgTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI,
                m_cols, m_rows,
                0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glGenTextures(1, &attrBgTexture);
    glBindTexture(GL_TEXTURE_2D, attrBgTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI,
                m_cols, m_rows,
                0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glGenTextures(1, &m_glyphUVTexture);
    glBindTexture(GL_TEXTURE_2D, m_glyphUVTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                m_cols, m_rows,
                0, GL_RGBA, GL_FLOAT, nullptr);

    // --- Build glyph atlas ---
    if (FT_Init_FreeType(&ft))
        throw std::runtime_error("FT_Init_FreeType failed");
    
    FT_New_Face(ft, find_font(preferred_fonts, FC_WEIGHT_REGULAR).c_str(), 0, &face);
    FT_Set_Pixel_Sizes(face, 0, glyphH);
    FT_New_Face(ft, find_font(preferred_fonts, FC_WEIGHT_BOLD).c_str(), 0, &bface);
    FT_Set_Pixel_Sizes(bface, 0, glyphH);
    
    m_glyphCache.init(face, bface);

    // --- Render ---
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void TerminalWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    if (!m_tab || !m_tab->model) return;
    m_cols = w / m_charWidth;
    m_rows = h / m_charHeight;
    if (m_cols == m_tab->model->width() && m_rows == m_tab->model->height()) return;

    m_tab->model->resize(m_cols, m_rows);

    glBindTexture(GL_TEXTURE_2D, textTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, m_cols, m_rows,
                 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    glBindTexture(GL_TEXTURE_2D, attrFgTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, m_cols, m_rows,
                 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, attrBgTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, m_cols, m_rows,
                 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glBindTexture(GL_TEXTURE_2D, m_glyphUVTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_cols, m_rows,
                 0, GL_RGBA, GL_FLOAT, nullptr);

    if (m_tab->masterFd >= 0) {
        struct winsize ws{};
        ws.ws_col = m_cols;
        ws.ws_row = m_rows;
        ws.ws_xpixel = w;
        ws.ws_ypixel = h;
        ioctl(m_tab->masterFd, TIOCSWINSZ, &ws);
    }
}

void TerminalWidget::paintGL() {
    if (!m_tab) return;
    
    auto visible = m_tab->model->getVisibleScreen();

    const size_t cellCount = static_cast<size_t>(m_cols) * m_rows;
    screenCodepoints.resize(cellCount);
    screenFg.resize(cellCount * 4);
    screenBg.resize(cellCount * 4);

    m_glyphUVs.resize(cellCount * 4);
    for (size_t i = 0; i < cellCount; ++i) {
        const TerminalCell& c = visible[i];
        screenCodepoints[i] = c.codepoint;

        // Resolve colors
        ColorSpec fg_r = m_tab->model->resolveColor(c.fg, true);
        ColorSpec bg_r = m_tab->model->resolveColor(c.bg, false);

        screenFg[i * 4 + 0] = fg_r.r;
        screenFg[i * 4 + 1] = fg_r.g;
        screenFg[i * 4 + 2] = fg_r.b;
        screenFg[i * 4 + 3] = static_cast<uint8_t>(c.flags & 0xFF);

        screenBg[i * 4 + 0] = bg_r.r;
        screenBg[i * 4 + 1] = bg_r.g;
        screenBg[i * 4 + 2] = bg_r.b;
        screenBg[i * 4 + 3] = static_cast<uint8_t>((c.flags >> 8) & 0xFF);

        // Resolve UVs
        GlyphStyle style = GlyphStyle::Regular;
        if ((c.flags & CellBold) && (c.flags & CellItalic)) style = GlyphStyle::BoldItalic;
        else if (c.flags & CellBold)   style = GlyphStyle::Bold;
        else if (c.flags & CellItalic) style = GlyphStyle::Italic;

        GlyphRect r{0, 0, 0, 0, false};
        if (c.codepoint != 0)
            r = m_glyphCache.get(c.codepoint, style);
        m_glyphUVs[i*4+0] = r.u0;
        m_glyphUVs[i*4+1] = r.v0;
        m_glyphUVs[i*4+2] = r.u1;
        m_glyphUVs[i*4+3] = r.v1;
    }
    auto& s = Settings::data();

    float alpha = s.backgroundOpacity / 100.0f;
    QColor bg = s.backgroundColor;
    glClearColor(bg.redF() * alpha, bg.greenF() * alpha, bg.blueF() * alpha, alpha);
    glClear(GL_COLOR_BUFFER_BIT);

    program.bind();


    program.setUniformValue("TextBuffer", 0);
    program.setUniformValue("AttrFGBuffer", 1);
    program.setUniformValue("AttrBGBuffer", 2);
    program.setUniformValue("GlyphAtlas", 3);
    program.setUniformValue("GlyphUVs", 4);
    program.setUniformValue("BackgroundOpacity", s.backgroundOpacity / 100.0f);
    program.setUniformValue("CursorStyle", s.cursorStyle);
    program.setUniformValue("CursorVisible", cursorVisible && m_tab->model->cursorVisible() && m_tab->model->cursorBlinkEnabled());
    program.setUniformValue("HasSelection", m_hasSelection);
    GLint cursorPosLoc = program.uniformLocation("CursorPos");
    GLint cellSizeLoc = program.uniformLocation("CellSize");
    GLint selStartLoc = program.uniformLocation("SelStart");
    GLint selEndLoc = program.uniformLocation("SelEnd");
    GLint selColorLoc = program.uniformLocation("SelColor");
    GLint cursorColorLoc = program.uniformLocation("CursorColor");
    if (cursorPosLoc >= 0) glUniform2i(cursorPosLoc, m_tab->model->cursor_x(), m_tab->model->cursor_y() - m_tab->model->scroll_y());
    if (cellSizeLoc >= 0) glUniform2i(cellSizeLoc, m_charWidth, m_charHeight);
    if (selStartLoc >= 0) glUniform2i(selStartLoc, m_selStart.x(), m_selStart.y() - m_tab->model->scroll_y());
    if (selEndLoc >= 0) glUniform2i(selEndLoc, m_selEnd.x(), m_selEnd.y() - m_tab->model->scroll_y());
    if (selColorLoc >= 0) glUniform3f(selColorLoc, s.selectionColor.redF(), s.selectionColor.greenF(), s.selectionColor.blueF());
    if (cursorColorLoc >= 0) glUniform3f(cursorColorLoc, s.cursorColor.redF(), s.cursorColor.greenF(), s.cursorColor.blueF());


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
        m_cols, m_rows,
        GL_RED_INTEGER, GL_UNSIGNED_INT,
        screenCodepoints.data() );

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, attrFgTexture);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
        m_cols, m_rows,
        GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, screenFg.data() );

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, attrBgTexture);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
        m_cols, m_rows,
        GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, screenBg.data() );

    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m_glyphCache.texture());
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, m_glyphUVTexture);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
        m_cols, m_rows,
        GL_RGBA, GL_FLOAT, m_glyphUVs.data() );

    // Draw
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    program.release();
}

QPoint TerminalWidget::getCellPos(QPointF position) {
    QPoint p;
    p.setX((int)(position.x() / m_charWidth));
    p.setY((int)(position.y() / m_charHeight));
    return p;
}

// --- Events ---
void TerminalWidget::keyPressEvent(QKeyEvent* event) {
    Qt::KeyboardModifiers mods = event->modifiers();

    if (mods == (Qt::ControlModifier | Qt::ShiftModifier)) {
        switch (event->key()) {
            case Qt::Key_C: copySelection(); return;
            case Qt::Key_V: paste(); return;
            case Qt::Key_T: emit requestNewTab(); return;
            case Qt::Key_N: emit requestNewWindow(); return;
            default: break;
        }
    }

    if (m_tab->masterFd < 0) return;

    QByteArray bytes;

    // Map keys to escape sequences
    switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:     bytes = "\r";       break;
        case Qt::Key_Backspace: bytes = "\x7f";     break;
        case Qt::Key_Tab:       bytes = "\t";       break;
        case Qt::Key_Up:    bytes = m_tab->model->applicationCursor() ? "\x1bOA" : "\x1b[A"; break;
        case Qt::Key_Down:  bytes = m_tab->model->applicationCursor() ? "\x1bOB" : "\x1b[B"; break;
        case Qt::Key_Right: bytes = m_tab->model->applicationCursor() ? "\x1bOC" : "\x1b[C"; break;
        case Qt::Key_Left:  bytes = m_tab->model->applicationCursor() ? "\x1bOD" : "\x1b[D"; break;
        case Qt::Key_Home:      bytes = "\x1b[H";   break;
        case Qt::Key_End:       bytes = "\x1b[F";   break;
        case Qt::Key_Delete:    bytes = "\x1b[3~";  break;
        case Qt::Key_PageUp:    bytes = "\x1b[5~";  break;
        case Qt::Key_PageDown:  bytes = "\x1b[6~";  break;
        default:
            bytes = event->text().toUtf8();
            break;
    }

    if (!bytes.isEmpty())
        ::write(m_tab->masterFd, bytes.constData(), bytes.size());
}

void TerminalWidget::keyReleaseEvent(QKeyEvent *event) {
    Q_UNUSED(event);
}

void TerminalWidget::wheelEvent(QWheelEvent* event) {
    if (m_tab->model->isAltScreen()) {
        // send arrow keys to shell
        QByteArray seq = (event->angleDelta().y() > 0)
            ? "\x1b[A\x1b[A\x1b[A" : "\x1b[B\x1b[B\x1b[B";
        ::write(m_tab->masterFd, seq.constData(), seq.size());
        return;
    }
    int delta = event->angleDelta().y() > 0 ? 3 : -3;
    m_tab->model->adjustScrollOffset(-delta);
    update();
}

void TerminalWidget::mousePressEvent(QMouseEvent *event) {
    QPoint mouse_cell = getCellPos(event->pos());
    m_selStart = mouse_cell;
    m_hasSelection = true;
}

void TerminalWidget::mouseMoveEvent(QMouseEvent *event) {
    QPoint mouse_cell = getCellPos(event->pos());
    m_selEnd = mouse_cell;
    update();
}

void TerminalWidget::mouseReleaseEvent(QMouseEvent *event) {
    QPoint mouse_cell = getCellPos(event->pos());
    if (mouse_cell == m_selStart) m_hasSelection = false;
    else m_selEnd = mouse_cell;
    update();
}


// --- Copy & Paste ---
void TerminalWidget::copySelection() {
    auto visible = m_tab->model->getVisibleScreen();
    QString text;

    int width = m_cols;
    int sel_end = m_tab->model->charOffset(m_selEnd.x(), m_selEnd.y());
    int sel_start = m_tab->model->charOffset(m_selStart.x(), m_selStart.y());

    for (int i = 0; i < sel_end - sel_start; i++) {
        int offset = sel_start + i;
        
        if (i > 0 && (sel_start + i) % width == 0)
            text += '\n';
        
        uint32_t cp = visible.at(offset).codepoint;
        if (cp != 0) text += QString::fromUcs4(&cp, 1);
    }
    QGuiApplication::clipboard()->setText(text);
}

void TerminalWidget::paste() {
    QString text = QGuiApplication::clipboard()->text();
    QByteArray bytes = text.toUtf8();
    if (m_tab->model->bracketedPaste()) {
        ::write(m_tab->masterFd, "\x1b[200~", 6);
        ::write(m_tab->masterFd, bytes.constData(), bytes.size());
        ::write(m_tab->masterFd, "\x1b[201~", 6);
    } else {
        ::write(m_tab->masterFd, bytes.constData(), bytes.size());
    }
}

void TerminalWidget::onPtyReadable() {
    char buf[4096];
    ssize_t n = ::read(m_tab->masterFd, buf, sizeof(buf));
    if (n <= 0) {
        m_tab->notifier->setEnabled(false);
        emit tabClosed();
        return;
    }
    QString text = m_tab->decoder.decode(QByteArray(buf, static_cast<int>(n)));
    m_tab->parser->feed(text);
    update();
}

void TerminalWidget::setTab(TerminalTab* tab) {
    if (m_tab && m_tab->notifier) disconnect(m_tab->notifier, &QSocketNotifier::activated, this, &TerminalWidget::onPtyReadable);

    m_tab = tab;

    if (m_tab && m_tab->notifier) connect(m_tab->notifier, &QSocketNotifier::activated, this, &TerminalWidget::onPtyReadable);

    update();
}