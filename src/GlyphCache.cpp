#include "GlyphCache.h"
#include <stdexcept>
#include <cstring>

GlyphCache::GlyphCache(int cellW, int cellH, int atlasW, int atlasH)
    : m_cellW(cellW), m_cellH(cellH), m_atlasW(atlasW), m_atlasH(atlasH) {}

GlyphCache::~GlyphCache() {
    if (m_tex) glDeleteTextures(1, &m_tex);
}

void GlyphCache::init(FT_Face regular, FT_Face bold) {
    initializeOpenGLFunctions();
    m_regular = regular;
    m_bold    = bold;

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // allocate empty atlas
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                 m_atlasW, m_atlasH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, nullptr);
}

FT_Face GlyphCache::faceFor(GlyphStyle style) {
    return (style == GlyphStyle::Bold || style == GlyphStyle::BoldItalic)
           ? m_bold : m_regular;
}

GlyphRect GlyphCache::get(uint32_t cp, GlyphStyle style) {
    uint32_t key = cp | (static_cast<uint32_t>(style) << 21);
    auto it = m_cache.find(key);
    if (it != m_cache.end()) return it->second;
    return m_cache[key] = upload(cp, style);
}

GlyphRect GlyphCache::upload(uint32_t cp, GlyphStyle style) {
    if (!m_tex) {
        qDebug() << "upload called before init!";
        return {0,0,0,0,false};
    }

    FT_Face face = faceFor(style);

    bool italic = (style == GlyphStyle::Italic || style == GlyphStyle::BoldItalic);
    if (italic) {
        FT_Matrix m = { 1<<16, (int)(0.25*(1<<16)), 0, 1<<16 };
        FT_Set_Transform(face, &m, nullptr);
    } else {
        FT_Set_Transform(face, nullptr, nullptr);
    }

    if (FT_Load_Char(face, cp, FT_LOAD_RENDER))
        return {0,0,0,0,false};

    FT_GlyphSlot g = face->glyph;
    int bw = (int)g->bitmap.width;
    int bh = (int)g->bitmap.rows;
    
    bool wide = (g->bitmap_left + bw) > m_cellW;
    int slotW = wide ? m_cellW * 2 : m_cellW;
    int slotH = m_cellH;
    
    // advance to next row if needed
    if (m_cursorX + slotW > m_atlasW) {
        m_cursorX  = 0;
        m_cursorY += m_rowH;
        m_rowH     = 0;
    }

    if (m_cursorY + slotH > m_atlasH) {
        return {0,0,0,0,false};
    }

    m_rowH = std::max(m_rowH, slotH);

    int baseline = face->size->metrics.ascender >> 6;
    int ox = m_cursorX + 1 + g->bitmap_left;
    int oy = m_cursorY + baseline - g->bitmap_top;

    // upload row by row clipping to atlas bounds
    for (int y = 0; y < bh; ++y) {
        int ay = oy + y;
        if (ay < m_cursorY || ay >= m_cursorY + slotH) continue;
        if (ay < 0 || ay >= m_atlasH) continue;

        int x0 = std::max(0, -ox);
        int x1 = std::min(bw, m_atlasW - ox);

        if (x0 >= x1) continue;

        glBindTexture(GL_TEXTURE_2D, m_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0,
            ox + x0, ay,
            x1 - x0, 1,
            GL_RED, GL_UNSIGNED_BYTE,
            g->bitmap.buffer + y * g->bitmap.pitch + x0);
    }

    GlyphRect r;
    r.u0 = (float) m_cursorX          / m_atlasW;
    r.v0 = (float) m_cursorY          / m_atlasH;
    r.u1 = (float)(m_cursorX + slotW) / m_atlasW;
    r.v1 = (float)(m_cursorY + slotH) / m_atlasH;
    r.valid = true;

    m_cursorX += slotW;
    return r;
}

bool GlyphCache::isWide(uint32_t cp) {
    if (FT_Load_Char(m_regular, cp, FT_LOAD_RENDER))
        return false;
    FT_GlyphSlot g = m_regular->glyph;
    int left = g->bitmap_left;
    int width = (int)g->bitmap.width;
    return (left + width) > m_cellW;
}