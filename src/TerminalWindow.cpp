#include "TerminalWindow.h"
#include <unistd.h>


TerminalWindow::TerminalWindow() {
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);

    m_tabBar = new TabBar(this);
    m_termWidget = new TerminalWidget(this);

    m_tabBar->setFixedHeight(32);
    layout->addWidget(m_tabBar, 0);
    layout->addWidget(m_termWidget, 1);

    connect(m_tabBar, &TabBar::tabClicked, this, &TerminalWindow::switchToTab);
    connect(m_tabBar, &TabBar::tabClosed, this, &TerminalWindow::closeTab);
    connect(m_termWidget, &TerminalWidget::tabClosed, this, [this]{closeTab(m_activeTab);});
    connect(m_tabBar, &TabBar::newTabRequested, this, [this]{ createTab(); });
    connect(m_termWidget, &TerminalWidget::requestNewTab, this, [this]{ createTab(); });

    createTab();
}

TerminalTab* TerminalWindow::createTab() {
    auto* tab = new TerminalTab();
    tab->model = new TerminalModel(m_termWidget->cols(), m_termWidget->rows(), 
                                   8192,
                                   m_termWidget->charWidth(),
                                   m_termWidget->charHeight());
    tab->parser = new TerminalParser(*tab->model);

    struct winsize ws{};
    ws.ws_col = tab->model->width();
    ws.ws_row = tab->model->height();
    tab->shellPid = forkpty(&tab->masterFd, nullptr, nullptr, &ws);
    if (tab->shellPid == 0) {
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        execl("/bin/zsh", "zsh", nullptr);
        _exit(1);
    }

    tab->notifier = new QSocketNotifier(tab->masterFd, QSocketNotifier::Read);

    int index = (int)m_tabs.size();
    m_tabs.push_back(tab);
    m_tabBar->addTab(tab->title, index);
    switchToTab(index);
    return tab;
}

void TerminalWindow::switchToTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    m_activeTab = index;
    m_termWidget->setTab(m_tabs[index]);
    m_tabBar->setActiveTab(index);
    m_termWidget->setFocus();
}

void TerminalWindow::closeTab(int index) {
    if (m_tabs.size() == 1) { qApp->quit(); return; }
    delete m_tabs[index];
    m_tabs.erase(m_tabs.begin() + index);
    m_tabBar->removeTab(index);
    switchToTab(std::min(index, (int)m_tabs.size() - 1));
}