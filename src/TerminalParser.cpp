#include "TerminalParser.h"
#include "TerminalModel.h"

TerminalParser::TerminalParser(TerminalModel& model)
: m_model(model) {}

void TerminalParser::reset() {
    m_state = State::Ground;
    m_paramBuffer.clear();
    m_oscBuffer.clear();
    m_privateMode = false;
}

std::vector<int> TerminalParser::parseParams(QStringView s) const {
    std::vector<int> out;
    if (s.isEmpty()) return out;

    QString token;
    for (QChar ch : s) {
        if (ch == ';') {
            out.push_back(token.isEmpty() ? 0 : token.toInt());
            token.clear();
        } else {
            token.append(ch);
        }
    }
    out.push_back(token.isEmpty() ? 0 : token.toInt());
    return out;
}

bool TerminalParser::isFinalByte(QChar ch) {
    ushort u = ch.unicode();
    return u >= 0x40 && u <= 0x7E;
}

void TerminalParser::handleCSI(QChar finalChar) {
    const auto params = parseParams(m_paramBuffer);

    auto p = [&](int idx, int def) {
        return (idx < static_cast<int>(params.size()) && params[idx] != 0) ? params[idx] : def;
    };

    switch (finalChar.unicode()) {
        case 'A': m_model.cursorUp(p(0, 1)); break;
        case 'B': m_model.cursorDown(p(0, 1)); break;
        case 'C': m_model.cursorRight(p(0, 1)); break;
        case 'D': m_model.cursorLeft(p(0, 1)); break;
        case 'E':
            m_model.cursorDown(p(0, 1));
            m_model.cursorHomeX();
            break;
        case 'F':
            m_model.cursorUp(p(0, 1));
            m_model.cursorHomeX();
            break;
        case 'G':
            m_model.cursorTo(m_model.cursor_y() + 1, p(0, 1));
            break;
        case 'H':
        case 'f':
            m_model.cursorTo(p(0, 1), p(1, 1));
            break;
        case 'J':
            m_model.clearScreen(p(0, 0));
            break;
        case 'K':
            m_model.clearLine(p(0, 0));
            break;
        case 'L':
            m_model.scrollRegionDown(1);
            break;
        case 'M':
            m_model.scrollRegionUp(1);
            break;
        case 'S':
            m_model.scrollRegionUp(p(0, 1));
            break;
        case 'T':
            m_model.scrollRegionDown(p(0, 1));
            break;
        case 'd':
            m_model.cursorTo(p(0, 1), m_model.cursor_x() + 1);
            break;
        case 'h':
        case 'l':
            if (m_privateMode) {
                bool enabled = (finalChar == 'h');
                for (int v : params) {
                    m_model.setPrivateMode(v, enabled);
                }
            }
            break;
        case 'm':
            m_model.setSGR(params);
            break;
        case 'r':
            m_model.setScrollRegion(p(0, 1), p(1, m_model.height()));
            break;
        case 's':
            m_model.saveCursor();
            break;
        case 'u':
            m_model.restoreCursor();
            break;
        default:
            break;
    }
}

void TerminalParser::feed(QStringView text) {
    if (text.isEmpty()) return;
    for (QChar ch : text) {
        switch (m_state) {
            case State::Ground:
                if (ch == QChar(0x1B)) {
                    m_state = State::Escape;
                } else {
                    m_model.putCodepoint(ch.unicode());
                }
                break;

            case State::Escape:
                if (ch == '(') {
                    m_state = State::Charset;
                } else if (ch == '[') {
                    m_state = State::CSI;
                    m_paramBuffer.clear();
                    m_privateMode = false;
                } else if (ch == ']') {
                    m_state = State::OSC;
                    m_oscBuffer.clear();
                } else if (ch == '7') {
                    m_model.saveCursor();
                    m_state = State::Ground;
                } else if (ch == '8') {
                    m_model.restoreCursor();
                    m_state = State::Ground;
                } else {
                    m_state = State::Ground;
                }
                break;

            case State::CSI:
                if (ch == '?') {
                    m_privateMode = true;
                } else if (isFinalByte(ch)) {
                    handleCSI(ch);
                    m_state = State::Ground;
                } else {
                    m_paramBuffer.append(ch);
                }
                break;

            case State::OSC:
                // Ignore OSC for now, but consume until BEL or ST
                if (ch == QChar(0x07)) {
                    m_state = State::Ground;
                } else if (ch == QChar(0x1B)) {
                    m_state = State::Escape;
                } else {
                    m_oscBuffer.append(ch);
                }
                break;
            
            case State::Charset:
                // TODO: c = 'B', '0'
                m_state = State::Ground;
        }
    }
}