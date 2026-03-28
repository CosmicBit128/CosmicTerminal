#include "TerminalModel.h"
#include <cstring>
#include <algorithm>

TerminalModel::TerminalModel(int w, int h, int buf_h, int cw, int ch)
: m_width(w),
  m_height(h),
  m_buf_height(buf_h),
  m_char_width(cw),
  m_char_height(ch),
  m_buffer(static_cast<size_t>(w) * buf_h, 0) {}

void TerminalModel::resize(int new_width, int new_height) {
    m_width = new_width;
    m_height = new_height;
    m_cursor_x = 0;
    m_cursor_y = 0;
    m_top_row = 0;
    m_buffer.assign(static_cast<size_t>(m_width) * m_buf_height, 0);
}

size_t TerminalModel::physicalRow(int logicalRow) const {
    return static_cast<size_t>((m_top_row + logicalRow) % m_buf_height);
}

void TerminalModel::clearRow(int physicalRow) {
    std::fill_n(&m_buffer[static_cast<size_t>(physicalRow) * m_width], m_width, 0);
}

void TerminalModel::insertChar(uint32_t codepoint) {
    if (codepoint == '\r') {
        m_cursor_x = 0;
        return;
    }

    if (codepoint == 0x7f) {
        if (m_cursor_x > 0) {
            --m_cursor_x;

            size_t row = physicalRow(m_cursor_y);
            m_buffer[row * m_width + m_cursor_x] = 0;
        } else if (m_cursor_y > 0) {
            --m_cursor_y;
            size_t row = physicalRow(m_cursor_y);
            int x = m_width - 1;
            while (x > 0 && m_buffer[row * m_width + x] == 0)
                --x; // Find last non-empty char
            m_cursor_x = x+1;
        } else {
            return;
        }

        return;
    }

    if (codepoint == '\n') {
        m_cursor_x = 0;
        ++m_cursor_y;
    } else {
        size_t row = physicalRow(m_cursor_y);
        m_buffer[row * m_width + m_cursor_x] = codepoint;

        ++m_cursor_x;
        if (m_cursor_x >= m_width) {
            m_cursor_x = 0;
            ++m_cursor_y;
        }
    }

    if (m_cursor_y >= m_height) {
        m_cursor_y = m_height - 1;
        m_top_row = (m_top_row + 1) % m_buf_height;
        clearRow(static_cast<int>(physicalRow(m_height - 1)));
    }
}

uint32_t TerminalModel::getChar(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        return 0;
    }

    size_t row = physicalRow(y);
    return m_buffer[row * m_width + x];
}

std::vector<uint32_t> TerminalModel::getVisibleScreen() const {
    std::vector<uint32_t> visible(static_cast<size_t>(m_width) * m_height, 0);

    for (int row = 0; row < m_height; ++row) {
        size_t src_y = physicalRow(row);
        const uint32_t* src_ptr = &m_buffer[src_y * m_width];
        uint32_t* dst_ptr = &visible[static_cast<size_t>(row) * m_width];
        std::memcpy(dst_ptr, src_ptr, static_cast<size_t>(m_width) * sizeof(uint32_t));
    }

    return visible;
}