#pragma once

#include <QVector>
#include <QColor>
#include <QSettings>

struct SettingsData {
    int terminalWidth = 128;
    int terminalHeight = 32;
    int terminalBufferHeight = 4096;
    bool enableBell = true;
    QVector<QColor> colors = {
        "#2E3436", "#CC0000", "#4E9A06", "#C4A000", "#3465A4", "#75507B", "#06989A", "#D3D7CF",
        "#555753", "#EF2929", "#8AE234", "#FCE94F", "#729FCF", "#AD7FA8", "#34E2E2", "#EEEEEC"
    };
    QColor backgroundColor = QColor("#000000");
    QColor cursorColor = QColor("#dcdcdc");
    QColor selectionColor = QColor("#5980ff");
    int backgroundOpacity = 80;
    int cursorStyle = 0;
    bool showTabBar = true;
    bool showTabCloseButton = true;
    bool showNewTabButton = true;
    int addNewTabs = 0;
    int tabsAlignment = 0;
};

class Settings {
public:
    static SettingsData& data();

    static void load();
    static void save();
};