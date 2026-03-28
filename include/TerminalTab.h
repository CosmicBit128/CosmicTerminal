#pragma once

#include "TerminalModel.h"
#include "TerminalParser.h"

#include <QSocketNotifier>
#include <QStringDecoder>


struct TerminalTab {
    TerminalModel* model = nullptr;
    TerminalParser* parser = nullptr;

    QStringDecoder decoder{QStringDecoder::Utf8};

    int masterFd = -1;
    pid_t shellPid = -1;
    QSocketNotifier* notifier = nullptr;
    QString title = "Terminal";

    ~TerminalTab() {
        delete notifier;
        delete parser;
        delete model;
        // if (masterFd >= 0) ::close(masterFd);
    }
};