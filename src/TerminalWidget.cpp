#include "TerminalWidget.h"
#include "TerminalModel.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <iostream>

TerminalWidget::TerminalWidget(TerminalModel* model, QWidget* parent)
:QOpenGLWidget(parent), m_model(model) {
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
}

TerminalWidget::~TerminalWidget() {
    if (glyphTexture) glDeleteTextures(1, &glyphTexture);
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
uniform sampler2D Glyphs;

uniform ivec2 CharSize;   // 7x14 (glyph size)
uniform ivec2 CellSize;   // 9x18 (grid spacing)
uniform int GlyphCols;
uniform ivec2 CursorPos;
uniform bool CursorVisible;

uniform vec4 FGColor;
uniform vec4 BGColor;

void main() {
    vec2 uv = UV;
    uv.y = 1.0 - uv.y;

    ivec2 screenSize = textureSize(TextBuffer, 0);

    // which cell
    vec2 grid_uv = uv * vec2(screenSize);
    ivec2 cell = ivec2(grid_uv);

    // inside cell (0–1)
    vec2 cell_uv = fract(grid_uv);
    ivec2 glyph_uv = ivec2(cell*CellSize + cell_uv*CellSize);

    // fragColor = vec4(cell_uv, 0.0, 1.0);
    // return;
    // fragColor = vec4(vec2(cell)/vec2(screenSize), 0.0, 1.0);
    // return;
    // fragColor = vec4(vec2(glyph_uv), 0.0, 1.0);
    // return;
    // fragColor = mix(vec4(cell_uv, 0.0, 1.0), vec4(1.0), texelFetch(Glyphs, glyph_uv, 0).r);
    // return;

    uint codepoint = texelFetch(TextBuffer, cell, 0).r;

    int glyph_index = int(codepoint - 32u);
    ivec2 glyph_pos = ivec2(
        glyph_index % GlyphCols,
        glyph_index / GlyphCols
    );

    ivec2 atlas_uv = ivec2(glyph_pos*CellSize + cell_uv*CellSize);

    float a = texelFetch(Glyphs, atlas_uv, 0).r;
    if (codepoint < 32u || codepoint > 126u) a = 0.0;
    vec4 color = mix(BGColor, FGColor, a);
    if (CursorVisible && cell == CursorPos) color.rgb = vec3(1.0) - color.rgb;
    

    fragColor = color;
}
    )");

    if (!program.link()) {
        qFatal("Shader link failed: %s", qPrintable(program.log()));
    }

    qDebug() << "CursorPos loc:" << program.uniformLocation("CursorPos");

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
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_R32UI,
        m_model->width(), m_model->height(),
        0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr
    );


    // --- Build glyph atlas ---
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        throw std::runtime_error("FT_Init_FreeType failed");
    }

    FT_Face face;
    // /usr/share/fonts/TTF/FiraCodeNerdFont-Bold.ttf
    // /usr/share/fonts/archcraft/nerd-fonts/FiraCode/FiraCodeNerdFont-Bold.ttf
    if (FT_New_Face(ft, "/usr/share/fonts/archcraft/nerd-fonts/FiraCode/FiraCodeNerdFont-Bold.ttf", 0, &face)) {
        FT_Done_FreeType(ft);
        throw std::runtime_error("FT_New_Face failed");
    }

    FT_Set_Pixel_Sizes(face, 0, glyphH);

    const int firstChar = 32;
    const int lastChar  = 126;
    const int glyphCount = lastChar - firstChar + 1;

    glyphCols = 16;
    glyphRows = (glyphCount + glyphCols - 1) / glyphCols;

    int cellW = 9;
    int cellH = 18;

    const int atlasW = glyphCols * cellW;
    const int atlasH = glyphRows * cellH;
    int baseline  = face->size->metrics.ascender >> 6;

    std::vector<unsigned char> atlas(static_cast<size_t>(atlasW) * atlasH, 0);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

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

                unsigned char src = g->bitmap.buffer[y * g->bitmap.pitch + x];
                atlas[ay * atlasW + ax] = src;
            }
        }
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glGenTextures(1, &glyphTexture);
    glBindTexture(GL_TEXTURE_2D, glyphTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_R8,
        atlasW,
        atlasH,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        atlas.data()
    );


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
}

void TerminalWidget::paintGL() {
    auto visible = m_model->getVisibleScreen();
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glTexSubImage2D(
        GL_TEXTURE_2D,
        0,
        0, 0,
        m_model->width(),
        m_model->height(),
        GL_RED_INTEGER,
        GL_UNSIGNED_INT,
        visible.data()
    );

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    program.bind();

    program.setUniformValue("TextBuffer", 0);
    program.setUniformValue("Glyphs", 1);
    program.setUniformValue("CellSize", 9, 18);
    program.setUniformValue("GlyphCols", glyphCols);
    GLint cellSizeLoc  = program.uniformLocation("CellSize");
    GLint glyphColsLoc = program.uniformLocation("GlyphCols");
    GLint cursorPosLoc = program.uniformLocation("CursorPos");
    if (cellSizeLoc >= 0) glUniform2i(cellSizeLoc, m_model->char_width(), m_model->char_height());
    if (glyphColsLoc >= 0) glUniform1i(glyphColsLoc, glyphCols);
    if (cursorPosLoc >= 0) glUniform2i(cursorPosLoc, m_model->cursor_x(), m_model->cursor_y());
    program.setUniformValue("CursorVisible", cursorVisible);
    program.setUniformValue("FGColor", QVector4D(0.9f, 0.9f, 0.9f, 1.0f));
    program.setUniformValue("BGColor", QVector4D(0.05f, 0.05f, 0.05f, 1.0f));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, glyphTexture);

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    program.release();
}

void TerminalWidget::keyPressEvent(QKeyEvent *event) {
    if (!m_model) return;

    switch (event->key()) {
        case Qt::Key_Backspace:
            m_model->insertChar(0x7f);
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            m_model->insertChar('\n');
            break;
        case Qt::Key_Space:
            m_model->insertChar(' ');
            break;
        default:
        {
            const QString text = event->text();
            if (text.isEmpty()) return;
            for (const QChar& c : text) {
                m_model->insertChar(c.unicode());
            }
            break;
        }
    }

    update();
}

void TerminalWidget::keyReleaseEvent(QKeyEvent *event) {
    Q_UNUSED(event);
}