#pragma once

#include <QString>
#include <QStringView>
#include <vector>

class TerminalModel;

class TerminalParser {
public:
    explicit TerminalParser(TerminalModel& model);

    void feed(QStringView text);
    void reset();

private:
    enum class State {
        Ground,
        Escape,
        Charset,
        CSI,
        OSC
    };

    void handleCSI(QChar finalChar);
    std::vector<int> parseParams(QStringView s) const;
    static bool isFinalByte(QChar ch);

private:
    TerminalModel& m_model;
    State m_state = State::Ground;
    QString m_paramBuffer;
    QString m_oscBuffer;
    bool m_privateMode = false;
};