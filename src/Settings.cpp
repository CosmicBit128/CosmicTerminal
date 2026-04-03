#include "Settings.h"


QColor loadColor(QSettings &settings, const QString &key, const QColor &defaultColor) {
    QString value = settings.value(key).toString();
    if (value.isEmpty())
        return defaultColor;

    QStringList parts = value.split(',');
    if (parts.size() != 3)
        return defaultColor;

    bool okR, okG, okB;
    int r = parts[0].toInt(&okR);
    int g = parts[1].toInt(&okG);
    int b = parts[2].toInt(&okB);

    if (!okR || !okG || !okB)
        return defaultColor;

    return QColor(r, g, b);
}


static SettingsData settings_data;

SettingsData& Settings::data() {
    return settings_data;
}

void Settings::save() {
    QSettings s;

    auto& d = data();
    s.setValue("TerminalWidth", d.terminalWidth);
    s.setValue("TerminalHeight", d.terminalHeight);
    s.setValue("TerminalBufferHeight", d.terminalBufferHeight);
    s.setValue("EnableBell", d.enableBell);
    s.beginWriteArray("Appearance/Colors");
    for (int i = 0; i < 16; ++i) {
        s.setArrayIndex(i);
        s.setValue("Color", QString("%1,%2,%3").arg(d.colors[i].red()).arg(d.colors[i].green()).arg(d.colors[i].blue()));
    }
    s.endArray();
    s.setValue("Appearance/BackgroundColor", QString("%1,%2,%3").arg(d.backgroundColor.red()).arg(d.backgroundColor.green()).arg(d.backgroundColor.blue()));
    s.setValue("Appearance/CursorColor", QString("%1,%2,%3").arg(d.cursorColor.red()).arg(d.cursorColor.green()).arg(d.cursorColor.blue()));
    s.setValue("Appearance/SelectionColor", QString("%1,%2,%3").arg(d.selectionColor.red()).arg(d.selectionColor.green()).arg(d.selectionColor.blue()));
    s.setValue("Appearance/BackgroundOpacity", d.backgroundOpacity);
    s.setValue("Appearance/CursorStyle", d.cursorStyle);
    s.setValue("Tabs/ShowTabBar", d.showTabBar);
    s.setValue("Tabs/ShowTabCloseButton", d.showTabCloseButton);
    s.setValue("Tabs/ShowNewTabButton", d.showNewTabButton);
    s.setValue("Tabs/AddNewTabs", d.addNewTabs);
    s.setValue("Tabs/TabsAlignment", d.tabsAlignment);
}

// GNOME Terminal: Tango Theme
// "#2E3436", "#CC0000", "#4E9A06", "#C4A000", "#3465A4", "#75507B", "#06989A", "#D3D7CF",
// "#555753", "#EF2929", "#8AE234", "#FCE94F", "#729FCF", "#AD7FA8", "#34E2E2", "#EEEEEC"

// Kitty Default Theme
// "#32363D", "#E06B74", "#98C379", "#E5C07A", "#62AEEF", "#C778DD", "#55B6C2", "#ABB2BF",
// "#50545B", "#EA757E", "#A2CD83", "#EFCA84", "#6CB8F9", "#D282E7", "#5FC0CC", "#B5BCC9"

void Settings::load() {
    QSettings s;

    auto& d = data();
    QString default_colors[16] = {
        "#32363D", "#E06B74", "#98C379", "#E5C07A", "#62AEEF", "#C778DD", "#55B6C2", "#ABB2BF",
        "#50545B", "#EA757E", "#A2CD83", "#EFCA84", "#6CB8F9", "#D282E7", "#5FC0CC", "#B5BCC9"
    };

    d.terminalWidth = s.value("TerminalWidth", 128).toInt();
    d.terminalHeight = s.value("TerminalHeight", 32).toInt();
    d.terminalBufferHeight = s.value("TerminalBufferHeight", 4096).toInt();
    d.enableBell = s.value("EnableBell", true).toBool();
    s.beginReadArray("Appearance/Colors");
    for (int i = 0; i < 16; ++i) {
        s.setArrayIndex(i);
        d.colors[i] = loadColor(s, "Color", QColor(default_colors[i]));
    }
    s.endArray();
    d.backgroundColor = loadColor(s, "Appearance/BackgroundColor", QColor(0,0,0));
    d.cursorColor = loadColor(s, "Appearance/CursorColor", QColor(220,220,220));
    d.selectionColor = loadColor(s, "Appearance/SelectionColor", QColor(89,128,255));
    d.backgroundOpacity = s.value("Appearance/BackgroundOpacity", 80).toInt();
    d.cursorStyle = s.value("Appearance/CursorStyle", 0).toInt();
    d.showTabBar = s.value("Tabs/ShowTabBar", true).toBool();
    d.showTabCloseButton = s.value("Tabs/ShowTabCloseButton", true).toBool();
    d.showNewTabButton = s.value("Tabs/ShowNewTabButton", true).toBool();
    d.addNewTabs = s.value("Tabs/AddNewTabs", 0).toInt();
    d.tabsAlignment = s.value("Tabs/TabsAlignment", 0).toInt();
}