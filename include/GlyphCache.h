#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <unordered_map>
#include <ft2build.h>
#include FT_FREETYPE_H

struct GlyphRect { float u0, v0, u1, v1; bool valid; };

enum class GlyphStyle { Regular, Bold, Italic, BoldItalic };

class GlyphCache : protected QOpenGLFunctions_3_3_Core {
public:
    GlyphCache(int cellW, int cellH, int atlasW = 4096, int atlasH = 4096);
    ~GlyphCache();

    void init(FT_Face regular, FT_Face bold);
    GlyphRect get(uint32_t codepoint, GlyphStyle style);

    GLuint texture() const { return m_tex; }
    int atlasW()    const { return m_atlasW; }
    int atlasH()    const { return m_atlasH; }
    bool isWide(uint32_t cp);

private:
    GlyphRect upload(uint32_t codepoint, GlyphStyle style);
    FT_Face faceFor(GlyphStyle style);

    int m_cellW, m_cellH;
    int m_atlasW, m_atlasH;
    int m_cursorX = 0, m_cursorY = 0;
    int m_rowH = 0; // tallest glyph in current row

    GLuint m_tex = 0;

    FT_Face m_regular  = nullptr;
    FT_Face m_bold     = nullptr;

    std::unordered_map<uint32_t, GlyphRect> m_cache;
};