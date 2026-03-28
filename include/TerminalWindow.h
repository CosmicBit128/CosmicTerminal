#pragma once

#include "TerminalTab.h"
#include "TerminalWidget.h"
#include "TabBar.h"

#include <QWidget>
#include <QVBoxLayout>
#include <vector>


class TerminalWindow : public QWidget {
    Q_OBJECT
public:
    TerminalWindow();
    TerminalTab* createTab();
    void switchToTab(int index);
    void closeTab(int index);

private:
    std::vector<TerminalTab*> m_tabs;
    int m_activeTab = -1;
    TerminalWidget* m_termWidget = nullptr;
    TabBar* m_tabBar = nullptr;
};