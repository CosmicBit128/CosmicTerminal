#pragma once

#include <vector>
#include <cstdint>

class TerminalModel {
public:
    TerminalModel(int w, int h, int buf_h, int cw, int ch);

    void resize(int new_width, int new_height);
    void insertChar(uint32_t codepoint);
    uint32_t getChar(int x, int y) const;
    std::vector<uint32_t> getVisibleScreen() const;

    int width() const { return m_width; }
    int height() const { return m_height; }
    int char_width() const { return m_char_width; }
    int char_height() const { return m_char_height; }
    int cursor_x() const { return m_cursor_x; }
    int cursor_y() const { return m_cursor_y; }
    int bufferHeight() const { return m_buf_height; }

private:
    size_t physicalRow(int logicalRow) const;
    void clearRow(int physicalRow);

    int m_width;
    int m_height;
    int m_char_width;
    int m_char_height;
    int m_buf_height;

    int m_cursor_x = 0;
    int m_cursor_y = 0;
    int m_top_row = 0; // physical row index of the top visible line

    std::vector<uint32_t> m_buffer;
};