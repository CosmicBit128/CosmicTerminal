#include "TerminalModel.h"
#include "Settings.h"

#include <QApplication>
#include <cstring>


static ColorSpec ansiIndexToColor(int idx) {
    ColorSpec c;
    c.type = ColorType::Indexed;
    c.index = static_cast<uint8_t>(idx);
    return c;
}

static ColorSpec rgbColor(int r, int g, int b) {
    ColorSpec c;
    c.type = ColorType::RGB;
    c.r = static_cast<uint8_t>(r);
    c.g = static_cast<uint8_t>(g);
    c.b = static_cast<uint8_t>(b);
    return c;
}


TerminalModel::TerminalModel(int w, int h, int buf_h, int cw, int ch)
: m_width(w),
  m_height(h),
  m_char_width(cw),
  m_char_height(ch),
  m_buf_height(buf_h)
{
    m_main_buffer.resize(w * buf_h);
    m_alt_buffer.resize(w * buf_h);
    m_active_buffer = &m_main_buffer;

    m_cursor_x = 0;
    m_cursor_y = 0;

    auto& s = Settings::data();
    m_default_fg = rgbColor(220, 220, 220);
    m_default_bg = rgbColor(s.backgroundColor.red(), s.backgroundColor.green(), s.backgroundColor.blue());
    m_current_fg = m_default_fg;
    m_current_bg = m_default_bg;
}

void TerminalModel::resize(int new_width, int new_height) {
    std::vector<TerminalCell> new_buffer(new_width * m_buf_height);

    int rows_to_copy = std::min(m_height, new_height);
    int cols_to_copy = std::min(m_width, new_width);

    for (int row = 0; row < rows_to_copy; ++row) {
        size_t src = physicalRow(row) * m_width;
        size_t dst = row * new_width; // new buffer uses simple layout
        for (int col = 0; col < cols_to_copy; ++col)
            new_buffer[dst + col] = (*m_active_buffer)[src + col];
    }

    *m_active_buffer = std::move(new_buffer);
    m_cursor_x = std::min(m_cursor_x, new_width - 1);
    m_cursor_y = std::min(m_cursor_y, new_height - 1);
    m_width = new_width;
    m_height = new_height;
}

size_t TerminalModel::physicalRow(int logicalRow) const {
    return static_cast<size_t>((logicalRow) % m_buf_height);
}

size_t TerminalModel::physicalRowWithOffset(int logicalRow) const {
    int base = (m_buf_height + m_scroll_offset) % m_buf_height;
    return static_cast<size_t>((base + logicalRow) % m_buf_height);
}

void TerminalModel::clearRow(int physicalRow) {
    std::fill_n(&(*m_active_buffer)[static_cast<size_t>(physicalRow) * m_width], m_width, TerminalCell{});
}

int TerminalModel::charOffset(int x, int y) const {
    return physicalRow(y) * m_width + x;
}
TerminalCell& TerminalModel::cellAt(int x, int y) {
    return (*m_active_buffer)[charOffset(x, y)];
}
const TerminalCell& TerminalModel::cellAt(int x, int y) const {
    return (*m_active_buffer)[charOffset(x, y)];
}

void TerminalModel::putWrappedCodepoint(uint32_t cp) {
    // pre-write wrap
    if (m_cursor_x >= m_width) {
        m_cursor_x = 0;
        ++m_cursor_y;
    }

    TerminalCell& cell = cellAt(m_cursor_x, m_cursor_y);
    cell.codepoint = cp;
    cell.fg = m_current_fg;
    cell.bg = m_current_bg;
    cell.flags = m_current_flags;

    ++m_cursor_x;
}

void TerminalModel::putCodepoint(uint32_t codepoint) {
    if (codepoint == '\b' or codepoint == '\x7f') { backspace(); return; }
    if (codepoint == '\r') { carriageReturn(); return; }
    if (codepoint == '\n') { lineFeed(); return; }
    if (codepoint == '\t') { tab(); return; }
    if (codepoint == 0x07) { bell(); return; }
    
    putWrappedCodepoint(codepoint);
}

void TerminalModel::backspace() {
    if (m_cursor_x > 0) {
        --m_cursor_x;
    }
}

void TerminalModel::carriageReturn() {
    m_cursor_x = 0;
}

void TerminalModel::lineFeed() {
    int bot = scrollBottom();
    if (m_cursor_y == bot) {
        scrollRegionUp(1);
    } else {
        ++m_cursor_y;
    }
}

void TerminalModel::tab() {
    int nextTab = ((m_cursor_x / 8) + 1) * 8;
    while (m_cursor_x < nextTab) {
        putCodepoint(' ');
    }
}

void TerminalModel::bell() {
    QApplication::beep();
}

void TerminalModel::cursorUp(int n) {
    m_cursor_y = std::max(0, m_cursor_y - std::max(1, n));
}

void TerminalModel::cursorDown(int n) {
    m_cursor_y = std::min(scrollBottom(), m_cursor_y + std::max(1, n));
}

void TerminalModel::cursorLeft(int n) {
    m_cursor_x = std::max(0, m_cursor_x - std::max(1, n));
}

void TerminalModel::cursorRight(int n) {
    m_cursor_x = std::min(m_width - 1, m_cursor_x + std::max(1, n));
}

void TerminalModel::cursorTo(int row1, int col1) {
    m_cursor_y = std::clamp(row1 - 1, 0, m_height - 1);
    m_cursor_x = std::clamp(col1 - 1, 0, m_width - 1);
}

void TerminalModel::cursorHome() { m_cursor_x = 0; m_cursor_y = 0; }
void TerminalModel::cursorHomeX() { m_cursor_x = 0; }
void TerminalModel::cursorHomeY() { m_cursor_y = 0; }

void TerminalModel::clearLine(int mode) {
    // 0 = cursor to end | 1 = start to cursor | 2 = entire line
    auto clearCell = [&](int x) { cellAt(x, m_cursor_y) = TerminalCell{}; };

    if (mode == 2) {
        for (int x = 0; x < m_width; ++x) clearCell(x);
    } else if (mode == 1) {
        for (int x = 0; x <= m_cursor_x; ++x) clearCell(x);
    } else {
        for (int x = m_cursor_x; x < m_width; ++x) clearCell(x);
    }
}

void TerminalModel::clearScreen(int mode) {
    // 0 = cursor to end | 1 = start to cursor | 2 = entire screen | 3 = scrollback
    if (mode == 2) {
        for (int y = 0; y < m_height; ++y) {
            clearRow(static_cast<int>(physicalRow(y)));
        }
        m_scroll_top    = 0;
        m_scroll_bottom = -1;
        return;
    }

    if (mode == 1) {
        for (int y = 0; y < m_cursor_y; ++y) {
            clearRow(static_cast<int>(physicalRow(y)));
        }
        for (int x = 0; x <= m_cursor_x; ++x) cellAt(x, m_cursor_y) = TerminalCell{};
        return;
    }

    for (int x = m_cursor_x; x < m_width; ++x) cellAt(x, m_cursor_y) = TerminalCell{};
    for (int y = m_cursor_y + 1; y < m_height; ++y) {
        clearRow(static_cast<int>(physicalRow(y)));
    }
}

void TerminalModel::scrollRegionUp(int n) {
    int top = m_scroll_top;
    int bot = scrollBottom();
    n = std::clamp(n, 1, bot - top + 1);

    for (int i = 0; i < n; ++i) {
        // save top row, shift rows up, clear bottom
        for (int y = top; y < bot; ++y) {
            size_t dst = physicalRow(y);
            size_t src = physicalRow(y + 1);
            for (int x = 0; x < m_width; ++x)
                (*m_active_buffer)[dst * m_width + x] =
                    (*m_active_buffer)[src * m_width + x];
        }
        clearRow(static_cast<int>(physicalRow(bot)));
    }
}

void TerminalModel::scrollRegionDown(int n) {
    int top = m_scroll_top;
    int bot = scrollBottom();
    n = std::clamp(n, 1, bot - top + 1);

    for (int i = 0; i < n; ++i) {
        for (int y = bot; y > top; --y) {
            size_t dst = physicalRow(y);
            size_t src = physicalRow(y - 1);
            for (int x = 0; x < m_width; ++x)
                (*m_active_buffer)[dst * m_width + x] =
                    (*m_active_buffer)[src * m_width + x];
        }
        clearRow(static_cast<int>(physicalRow(top)));
    }
}

void TerminalModel::adjustScrollOffset(int delta) {
    m_scroll_offset = std::clamp(m_scroll_offset + delta, 0, m_buf_height - m_height);
}

void TerminalModel::saveCursor() {
    m_saved_cursor_x = m_cursor_x;
    m_saved_cursor_y = m_cursor_y;
}

void TerminalModel::setScrollRegion(int top1, int bottom1) {
    m_scroll_top    = std::clamp(top1 - 1, 0, m_height - 1);
    m_scroll_bottom = std::clamp(bottom1 - 1, 0, m_height - 1);
    m_cursor_x = 0;
    m_cursor_y = 0;
}

void TerminalModel::restoreCursor() {
    m_cursor_x = m_saved_cursor_x;
    m_cursor_y = m_saved_cursor_y;
}

void TerminalModel::setPrivateMode(int mode, bool enabled) {
    switch (mode) {
        case 1:
            m_application_cursor = enabled;
            break;

        case 25:
            m_cursor_blink = enabled;
            break;

        case 1049:
            if (enabled == m_alt_screen) return;

            if (enabled) {
                m_active_buffer = &m_alt_buffer;

                m_cursor_x = 0;
                m_cursor_y = 0;

                std::fill(m_active_buffer->begin(), m_active_buffer->end(), TerminalCell{});
                m_alt_screen = true;
            } else {
                m_active_buffer = &m_main_buffer;
                m_alt_screen = false;
                m_scroll_top    = 0;
                m_scroll_bottom = -1;
            }
            break;
        
        case 2004:
            m_bracketed_paste = enabled;
            break;
        
        default:
            break;
    }
}

std::vector<TerminalCell> TerminalModel::getVisibleScreen() const {
    std::vector<TerminalCell> visible(static_cast<size_t>(m_width) * m_height);

    for (int row = 0; row < m_height; ++row) {
        size_t src_y = physicalRowWithOffset(row);
        for (int x = 0; x < m_width; ++x) {
            visible[static_cast<size_t>(row) * m_width + x] =
                (*m_active_buffer)[src_y * m_width + x];
        }
    }

    return visible;
}

ColorSpec TerminalModel::resolveColor(const ColorSpec& c, bool isFg) {
    if (c.type == ColorType::RGB) return c;
    if (c.type == ColorType::Indexed) {
        ColorSpec out;
        out.type = ColorType::RGB;
        auto& s = Settings::data();
        if (c.index < 16) {
            QColor color = s.colors[c.index];
            out.r = color.red();
            out.g = color.green();
            out.b = color.blue();
        } else {
            out.r = kAnsi256[c.index-16][0];
            out.g = kAnsi256[c.index-16][1];
            out.b = kAnsi256[c.index-16][2];
        }
        return out;
    }
    return isFg ? m_default_fg : m_default_bg;
}




void TerminalModel::setSGR(const std::vector<int>& args) {
    if (args.empty()) {
        m_current_fg = m_default_fg;
        m_current_bg = m_default_bg;
        m_current_flags = 0;
        return;
    }

    size_t i = 0;
    while (i < args.size()) {
        int a = args[i++];

        switch (a) {
            case 0:
                m_current_fg = m_default_fg;
                m_current_bg = m_default_bg;
                m_current_flags = 0;
                break;
            case 1: m_current_flags |= CellBold; break;
            case 3: m_current_flags |= CellItalic; break;
            case 4: m_current_flags |= CellUnderline; break;
            case 5: m_current_flags |= CellBlink; break;
            case 7: m_current_flags |= CellReverse; break;
            case 8: m_current_flags |= CellHidden; break;
            case 9: m_current_flags |= CellStrike; break;

            case 22: m_current_flags &= ~CellBold; break;
            case 23: m_current_flags &= ~CellItalic; break;
            case 24: m_current_flags &= ~CellUnderline; break;
            case 25: m_current_flags &= ~CellBlink; break;
            case 27: m_current_flags &= ~CellReverse; break;
            case 28: m_current_flags &= ~CellHidden; break;
            case 29: m_current_flags &= ~CellStrike; break;

            case 39: m_current_fg = m_default_fg; break;
            case 49: m_current_bg = m_default_bg; break;

            case 30 ... 37: m_current_fg = ansiIndexToColor(a - 30); break;
            case 40 ... 47: m_current_bg = ansiIndexToColor(a - 40); break;
            case 90 ... 97: m_current_fg = ansiIndexToColor(a - 90 + 8); break;
            case 100 ... 107: m_current_bg = ansiIndexToColor(a - 100 + 8); break;

            case 38:
                if (i < args.size()) {
                    int mode = args[i++];
                    if (mode == 5 && i < args.size()) {
                        m_current_fg = ansiIndexToColor(args[i++]);
                    } else if (mode == 2 && i + 2 < args.size()) {
                        m_current_fg = rgbColor(args[i], args[i + 1], args[i + 2]);
                        i += 3;
                    }
                }
                break;

            case 48:
                if (i < args.size()) {
                    int mode = args[i++];
                    if (mode == 5 && i < args.size()) {
                        m_current_bg = ansiIndexToColor(args[i++]);
                    } else if (mode == 2 && i + 2 < args.size()) {
                        m_current_bg = rgbColor(args[i], args[i + 1], args[i + 2]);
                        i += 3;
                    }
                }
                break;

            default:
                break;
        }
    }
}