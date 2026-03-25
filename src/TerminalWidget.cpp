#include "TerminalWidget.h"
#include "TerminalModel.h"
#include "GlyphCache.h"
#include <QSocketNotifier>
#include <QByteArray>
#include <QStringDecoder>
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


TerminalWidget::TerminalWidget(TerminalModel* model, QWidget* parent)
:QOpenGLWidget(parent), m_model(model), parser(*model),
m_glyphCache(model->char_width(), model->char_height(), 4096, 4096) {
    setFocusPolicy(Qt::StrongFocus);

    cursorTimer = new QTimer(this);
    connect(cursorTimer, &QTimer::timeout, this, [this]() {
        cursorVisible = !cursorVisible;
        update();
    });
    cursorTimer->start(500);

    int glyph_w = 8;
    int glyph_h = 16;
    int cols = 16;
    int rows = 16;

    int atlas_w = cols * m_model->char_width();
    int atlas_h = rows * m_model->char_height();

    std::vector<unsigned char> atlas(atlas_w * atlas_h, 0);

    struct winsize ws{};
    ws.ws_col = m_model->width();
    ws.ws_row = m_model->height();
    ws.ws_xpixel = m_model->width()  * m_model->char_width();
    ws.ws_ypixel = m_model->height() * m_model->char_height();

    m_shellPid = forkpty(&m_masterFd, nullptr, nullptr, &ws);
    if (m_shellPid == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        execl("/bin/zsh", "zsh", nullptr);
        _exit(1);
    }

    m_ptyNotifier = new QSocketNotifier(m_masterFd, QSocketNotifier::Read, this);
    connect(m_ptyNotifier, &QSocketNotifier::activated, this, [this]() {
        char buf[4096];
        ssize_t n = ::read(m_masterFd, buf, sizeof(buf));
        if (n <= 0) {
            m_ptyNotifier->setEnabled(false);
            QMetaObject::invokeMethod(qApp, "quit", Qt::QueuedConnection);
            return;
        }
        QString text = m_decoder.decode(QByteArray(buf, static_cast<int>(n)));
        parser.feed(text);
        update();
    });
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

uniform sampler2D GlyphAtlas;
uniform sampler2D GlyphUVs;

uniform ivec2 CellSize;
uniform ivec2 CursorPos;
uniform bool CursorVisible;

void main() {
    vec2 uv = UV;
    uv.y = 1.0 - uv.y;

    
    ivec2 screenSize = textureSize(TextBuffer, 0);
    
    vec2 grid_uv = uv * vec2(screenSize);
    ivec2 cell = ivec2(grid_uv);
    vec2 cell_uv = fract(grid_uv);
    uint codepoint = texelFetch(TextBuffer, cell, 0).r;

    uvec4 fgPack = texelFetch(AttrFGBuffer, cell, 0);
    uvec4 bgPack = texelFetch(AttrBGBuffer, cell, 0);

    uint flags = fgPack.a;

    vec4 fg = vec4(fgPack.rgb, 255.0) / 255.0;
    vec4 bg = vec4(bgPack.rgb, 255.0) / 255.0;

    if ((flags & 16u) != 0u) {
        vec4 t = fg; fg = bg; bg = t;
    }

    vec4 uvRect = texelFetch(GlyphUVs, cell, 0);
    vec2 atlasCoord = mix(uvRect.xy, uvRect.zw, cell_uv);
    float alpha = texture(GlyphAtlas, atlasCoord).r;

    vec4 color = mix(bg, fg, alpha);
    if (cell == CursorPos && CursorVisible) { color.rgb = vec3(1.0) - color.rgb; }

    fragColor = color;
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

    glGenTextures(1, &m_glyphUVTexture);
    glBindTexture(GL_TEXTURE_2D, m_glyphUVTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                m_model->width(), m_model->height(),
                0, GL_RGBA, GL_FLOAT, nullptr);


    // --- Build glyph atlas ---
    if (FT_Init_FreeType(&ft)) {
        throw std::runtime_error("FT_Init_FreeType failed");
    }
    
    FT_New_Face(ft,
        find_font(preferred_fonts, FC_WEIGHT_REGULAR).c_str(),
        0, &face
    );
    FT_Set_Pixel_Sizes(face, 0, glyphH);
    FT_New_Face(ft,
        find_font(preferred_fonts, FC_WEIGHT_BOLD).c_str(),
        0, &bface
    );
    FT_Set_Pixel_Sizes(bface, 0, glyphH);

    int glyphCols = 16;
    int glyphRows = 6;

    int cellW = m_model->char_width();
    int cellH = m_model->char_height();

    glGenTextures(1, &textTexture);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI,
                m_model->width(), m_model->height(),
                0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

    glGenTextures(1, &attrFgTexture);
    glBindTexture(GL_TEXTURE_2D, attrFgTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI,
                m_model->width(), m_model->height(),
                0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);

    glGenTextures(1, &attrBgTexture);
    glBindTexture(GL_TEXTURE_2D, attrBgTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI,
                m_model->width(), m_model->height(),
                0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);

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

    int cols = w / m_model->char_width();
    int rows = h / m_model->char_height();
    m_model->resize(cols, rows);

    if (m_masterFd >= 0) {
        struct winsize ws{};
        ws.ws_col = cols;
        ws.ws_row = rows;
        ws.ws_xpixel = w;
        ws.ws_ypixel = h;
        ioctl(m_masterFd, TIOCSWINSZ, &ws);
    }
}

void TerminalWidget::paintGL() {
    auto visible = m_model->getVisibleScreen();

    const size_t cellCount = static_cast<size_t>(m_model->width()) * m_model->height();
    screenCodepoints.resize(cellCount);
    screenFg.resize(cellCount * 4);
    screenBg.resize(cellCount * 4);

    m_glyphUVs.resize(cellCount * 4);
    for (size_t i = 0; i < cellCount; ++i) {
        const TerminalCell& c = visible[i];
        screenCodepoints[i] = c.codepoint;

        // Resolve colors
        ColorSpec fg_r = m_model->resolveColor(c.fg, true);
        ColorSpec bg_r = m_model->resolveColor(c.bg, false);

        screenFg[i * 4 + 0] = fg_r.r;
        screenFg[i * 4 + 1] = fg_r.g;
        screenFg[i * 4 + 2] = fg_r.b;
        screenFg[i * 4 + 3] = c.flags; // flags live here

        screenBg[i * 4 + 0] = bg_r.r;
        screenBg[i * 4 + 1] = bg_r.g;
        screenBg[i * 4 + 2] = bg_r.b;
        screenBg[i * 4 + 3] = 0;

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

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    program.bind();

    program.setUniformValue("TextBuffer", 0);
    program.setUniformValue("AttrFGBuffer", 1);
    program.setUniformValue("AttrBGBuffer", 2);
    program.setUniformValue("GlyphAtlas", 3);
    program.setUniformValue("GlyphUVs", 4);
    program.setUniformValue("CursorVisible", cursorVisible && m_model->cursorVisible() && m_model->cursorBlinkEnabled() );
    GLint cursorPosLoc = program.uniformLocation("CursorPos");
    GLint cellSizeLoc = program.uniformLocation("CellSize");
    if (cursorPosLoc >= 0) glUniform2i(cursorPosLoc, m_model->cursor_x(), m_model->cursor_y());
    if (cellSizeLoc >= 0) glUniform2i(cellSizeLoc, m_model->char_width(), m_model->char_height());


    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
        m_model->width(), m_model->height(),
        GL_RED_INTEGER, GL_UNSIGNED_INT,
        screenCodepoints.data() );

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, attrFgTexture);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
        m_model->width(), m_model->height(),
        GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, screenFg.data() );

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, attrBgTexture);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
        m_model->width(), m_model->height(),
        GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, screenBg.data() );

    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m_glyphCache.texture());
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, m_glyphUVTexture);
    glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0,
        m_model->width(), m_model->height(),
        GL_RGBA, GL_FLOAT, m_glyphUVs.data() );

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Draw
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    program.release();
}

void TerminalWidget::keyPressEvent(QKeyEvent* event) {
    if (m_masterFd < 0) return;

    QByteArray bytes;

    // Map keys to escape sequences
    switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:     bytes = "\r";       break;
        case Qt::Key_Backspace: bytes = "\x7f";     break;
        case Qt::Key_Tab:       bytes = "\t";       break;
        case Qt::Key_Up:    bytes = m_model->applicationCursor() ? "\x1bOA" : "\x1b[A"; break;
        case Qt::Key_Down:  bytes = m_model->applicationCursor() ? "\x1bOB" : "\x1b[B"; break;
        case Qt::Key_Right: bytes = m_model->applicationCursor() ? "\x1bOC" : "\x1b[C"; break;
        case Qt::Key_Left:  bytes = m_model->applicationCursor() ? "\x1bOD" : "\x1b[D"; break;
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
        ::write(m_masterFd, bytes.constData(), bytes.size());
}

void TerminalWidget::keyReleaseEvent(QKeyEvent *event) {
    Q_UNUSED(event);
}