#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <algorithm>


static const uint8_t kAnsi256[240][3] = {
    {0,0,0}, {0,0,95}, {0,0,135}, {0,0,175}, {0,0,215}, {0,0,255}, {0,95,0}, {0,95,95},
    {0,95,135}, {0,95,175}, {0,95,215}, {0,95,255}, {0,135,0}, {0,135,95}, {0,135,135}, {0,135,175},
    {0,135,215}, {0,135,255}, {0,175,0}, {0,175,95}, {0,175,135}, {0,175,175}, {0,175,215}, {0,175,255},
    {0,215,0}, {0,215,95}, {0,215,135}, {0,215,175}, {0,215,215}, {0,215,255}, {0,255,0}, {0,255,95},
    {0,255,135}, {0,255,175}, {0,255,215}, {0,255,255}, {95,0,0}, {95,0,95}, {95,0,135}, {95,0,175},
    {95,0,215}, {95,0,255}, {95,95,0}, {95,95,95}, {95,95,135}, {95,95,175}, {95,95,215}, {95,95,255},
    {95,135,0}, {95,135,95}, {95,135,135}, {95,135,175}, {95,135,215}, {95,135,255}, {95,175,0}, {95,175,95},
    {95,175,135}, {95,175,175}, {95,175,215}, {95,175,255}, {95,215,0}, {95,215,95}, {95,215,135}, {95,215,175},
    {95,215,215}, {95,215,255}, {95,255,0}, {95,255,95}, {95,255,135}, {95,255,175}, {95,255,215}, {95,255,255},
    {135,0,0}, {135,0,95}, {135,0,135}, {135,0,175}, {135,0,215}, {135,0,255}, {135,95,0}, {135,95,95},
    {135,95,135}, {135,95,175}, {135,95,215}, {135,95,255}, {135,135,0}, {135,135,95}, {135,135,135}, {135,135,175},
    {135,135,215}, {135,135,255}, {135,175,0}, {135,175,95}, {135,175,135}, {135,175,175}, {135,175,215}, {135,175,255},
    {135,215,0}, {135,215,95}, {135,215,135}, {135,215,175}, {135,215,215}, {135,215,255}, {135,255,0}, {135,255,95},
    {135,255,135}, {135,255,175}, {135,255,215}, {135,255,255}, {175,0,0}, {175,0,95}, {175,0,135}, {175,0,175},
    {175,0,215}, {175,0,255}, {175,95,0}, {175,95,95}, {175,95,135}, {175,95,175}, {175,95,215}, {175,95,255},
    {175,135,0}, {175,135,95}, {175,135,135}, {175,135,175}, {175,135,215}, {175,135,255}, {175,175,0}, {175,175,95},
    {175,175,135}, {175,175,175}, {175,175,215}, {175,175,255}, {175,215,0}, {175,215,95}, {175,215,135}, {175,215,175},
    {175,215,215}, {175,215,255}, {175,255,0}, {175,255,95}, {175,255,135}, {175,255,175}, {175,255,215}, {175,255,255},
    {215,0,0}, {215,0,95}, {215,0,135}, {215,0,175}, {215,0,215}, {215,0,255}, {215,95,0}, {215,95,95},
    {215,95,135}, {215,95,175}, {215,95,215}, {215,95,255}, {215,135,0}, {215,135,95}, {215,135,135}, {215,135,175},
    {215,135,215}, {215,135,255}, {215,175,0}, {215,175,95}, {215,175,135}, {215,175,175}, {215,175,215}, {215,175,255},
    {215,215,0}, {215,215,95}, {215,215,135}, {215,215,175}, {215,215,215}, {215,215,255}, {215,255,0}, {215,255,95},
    {215,255,135}, {215,255,175}, {215,255,215}, {215,255,255}, {255,0,0}, {255,0,95}, {255,0,135}, {255,0,175},
    {255,0,215}, {255,0,255}, {255,95,0}, {255,95,95}, {255,95,135}, {255,95,175}, {255,95,215}, {255,95,255},
    {255,135,0}, {255,135,95}, {255,135,135}, {255,135,175}, {255,135,215}, {255,135,255}, {255,175,0}, {255,175,95},
    {255,175,135}, {255,175,175}, {255,175,215}, {255,175,255}, {255,215,0}, {255,215,95}, {255,215,135}, {255,215,175},
    {255,215,215}, {255,215,255}, {255,255,0}, {255,255,95}, {255,255,135}, {255,255,175}, {255,255,215}, {255,255,255},
    {8,8,8}, {18,18,18}, {28,28,28}, {38,38,38}, {48,48,48}, {58,58,58}, {68,68,68}, {78,78,78},
    {88,88,88}, {98,98,98}, {108,108,108}, {118,118,118}, {128,128,128}, {138,138,138}, {148,148,148}, {158,158,158},
    {168,168,168}, {178,178,178}, {188,188,188}, {198,198,198}, {208,208,208}, {218,218,218}, {228,228,228}, {238,238,238}
};

enum class ColorType : uint8_t {
    Default = 0,
    Indexed = 1,
    RGB = 2
};

struct ColorSpec {
    ColorType type = ColorType::Default;
    uint8_t index = 0;
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

enum CellFlags : uint16_t {
    CellBold      = 1 << 0,
    CellItalic    = 1 << 1,
    CellUnderline = 1 << 2,
    CellBlink     = 1 << 3,
    CellReverse   = 1 << 4,
    CellHidden    = 1 << 5,
    CellStrike    = 1 << 6,
    CellWide      = 1 << 7
};

struct TerminalCell {
    uint32_t codepoint = 0;
    ColorSpec fg{};
    ColorSpec bg{};
    uint16_t flags = 0;
};



class TerminalModel {
public:
    TerminalModel(int w, int h, int buf_h, int cw, int ch);

    void resize(int new_width, int new_height);

    void putCodepoint(uint32_t codepoint);
    void backspace();
    void carriageReturn();
    void lineFeed();
    void tab();
    void bell();

    void cursorUp(int n = 1);
    void cursorDown(int n = 1);
    void cursorLeft(int n = 1);
    void cursorRight(int n = 1);
    void cursorTo(int row1, int col1);
    void cursorHome();
    void cursorHomeX();
    void cursorHomeY();

    void clearScreen(int mode); // 0,1,2,3
    void clearLine(int mode); // 0,1,2
    void scrollRegionUp(int n = 1);
    void scrollRegionDown(int n = 1);
    void setScrollRegion(int top1, int bottom1);

    void saveCursor();
    void restoreCursor();
    void setCursorVisible(bool visible) { m_cursor_visible = visible; }
    
    void setSGR(const std::vector<int>& args);
    void setPrivateMode(int mode, bool enabled);
    void adjustScrollOffset(int delta);

    bool cursorVisible() const { return m_cursor_visible; }
    bool cursorBlinkEnabled() const { return m_cursor_blink; }
    bool applicationCursor() const { return m_application_cursor; }
    bool isAltScreen() const { return m_alt_screen; }
    bool bracketedPaste() const { return m_bracketed_paste; }
    
    int width() const { return m_width; }
    int height() const { return m_height; }
    int char_width() const { return m_char_width; }
    int char_height() const { return m_char_height; }
    int cursor_x() const { return m_cursor_x; }
    int cursor_y() const { return m_cursor_y; }
    int scroll_y() const { return m_scroll_offset; }
    int charOffset(int x, int y) const;
    
    ColorSpec resolveColor(const ColorSpec& c, bool isFg);
    std::vector<TerminalCell> getVisibleScreen() const;

private:
    size_t physicalRow(int logicalRow) const;
    size_t physicalRowWithOffset(int logicalRow) const;
    void clearRow(int physicalRow);
    TerminalCell& cellAt(int x, int y);
    const TerminalCell& cellAt(int x, int y) const;
    void putWrappedCodepoint(uint32_t cp);

    int scrollBottom() const { return m_scroll_bottom < 0 ? m_height - 1 : m_scroll_bottom; }

    int m_width;
    int m_height;
    int m_char_width;
    int m_char_height;
    int m_buf_height;

    int m_cursor_x = 0;
    int m_cursor_y = 0;
    int m_saved_cursor_x = 0;
    int m_saved_cursor_y = 0;
    int m_scroll_offset = 0;
    int m_scroll_top = 0;
    int m_scroll_bottom = -1;

    bool m_cursor_visible = true;
    bool m_cursor_blink = true;
    bool m_alt_screen = false;
    bool m_application_cursor = false;
    bool m_bracketed_paste = false;

    ColorSpec m_default_fg{};
    ColorSpec m_default_bg{};
    ColorSpec m_current_fg{};
    ColorSpec m_current_bg{};
    uint16_t m_current_flags = 0;

    int m_saved_top_row = 0;
    ColorSpec m_saved_fg{};
    ColorSpec m_saved_bg{};
    uint16_t m_saved_flags = 0;

    std::vector<TerminalCell> m_main_buffer;
    std::vector<TerminalCell> m_alt_buffer;
    std::vector<TerminalCell>* m_active_buffer;
};