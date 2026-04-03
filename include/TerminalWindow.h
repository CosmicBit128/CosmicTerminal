#pragma once

#include "TerminalTab.h"
#include "TerminalWidget.h"
#include "SettingsDialog.h"
#include "TabBar.h"

#include <QMainWindow>
#include <QMenuBar>
#include <QVBoxLayout>
#include <vector>


class TerminalWindow : public QMainWindow {
    Q_OBJECT
public:
    TerminalWindow();
    TerminalTab* createTab();
    void switchToTab(int index);
    void closeTab(int index);
    void newWindow();
    void openPreferences();
    void resizeWidget(int width, int height);
    QSize sizeHint() const;

private:
    std::vector<TerminalTab*> m_tabs;
    TerminalWidget* m_termWidget = nullptr;
    TabBar* m_tabBar = nullptr;
    SettingsDialog* m_dlg = nullptr;
    QMenuBar* m_menu = nullptr;
};