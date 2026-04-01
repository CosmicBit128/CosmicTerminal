#include "TerminalWindow.h"
#include <QMenu>
#include <QAction>
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

    connect(m_tabBar->bar(), &QTabBar::currentChanged, this, [this](int index){ switchToTab(index); });
    connect(m_tabBar->bar(), &QTabBar::tabCloseRequested, this, [this](int index){ closeTab(index); });
    connect(m_tabBar->newTabBtn(), &QPushButton::clicked, this, [this]{ createTab(); });
    connect(m_termWidget, &TerminalWidget::tabClosed, this, [this]{ closeTab(m_tabBar->bar()->currentIndex()); });
    connect(m_termWidget, &TerminalWidget::requestNewTab, this, [this]{ createTab(); });

    m_dlg = new SettingsDialog(this);

    // --- Menu Bar ---
    m_menu = new QMenuBar(this);
    QMenu* fileMenu = m_menu->addMenu("File");
    QAction* a_new_tab = fileMenu->addAction("New Tab");
    QAction* a_new_win = fileMenu->addAction("New Window");
    fileMenu->addSeparator();
    QAction* a_quit = fileMenu->addAction("Quit");
    QMenu* editMenu = m_menu->addMenu("Edit");
    QAction* a_copy = editMenu->addAction("Copy");
    QAction* a_paste = editMenu->addAction("Paste");
    editMenu->addSeparator();
    QAction* a_pref = editMenu->addAction("Preferences");

    connect(a_new_tab, &QAction::triggered, this, &TerminalWindow::createTab);
    connect(a_new_win, &QAction::triggered, this, &TerminalWindow::newWindow);
    connect(a_quit, &QAction::triggered, this, [this]{ qApp->quit(); });
    connect(a_copy, &QAction::triggered, this, [this]{ m_termWidget->copySelection(); });
    connect(a_paste, &QAction::triggered, this, [this]{ m_termWidget->paste(); });
    connect(a_pref, &QAction::triggered, this, &TerminalWindow::openPreferences);

    auto *menu_layout = new QVBoxLayout(this);
    menu_layout->setMenuBar(m_menu);

    createTab();
}

TerminalTab* TerminalWindow::createTab() {
    auto* tab = new TerminalTab();
    tab->model = new TerminalModel(m_termWidget->cols(), m_termWidget->rows(), 8192, m_termWidget->charWidth(), m_termWidget->charHeight());
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

    tab->model->glyphIsWide = [this](uint32_t cp) {
        return m_termWidget->glyphCache().isWide(cp);
    };

    tab->notifier = new QSocketNotifier(tab->masterFd, QSocketNotifier::Read);

    int index = (int)m_tabs.size();
    m_tabs.push_back(tab);
    m_tabBar->addTab(tab->title, index);
    switchToTab(index);

    return tab;
}

void TerminalWindow::switchToTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    m_tabBar->bar()->setCurrentIndex(index);
    m_termWidget->setTab(m_tabs[index]);
    m_termWidget->setFocus();
}

void TerminalWindow::closeTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    if (m_tabs.size() == 1) { qApp->quit(); return; }

    m_termWidget->setTab(nullptr);

    delete m_tabs[index];
    m_tabs.erase(m_tabs.begin() + index);
    m_tabBar->removeTab(index);

    int newIndex = std::min(index, (int)m_tabs.size() - 1);
    switchToTab(newIndex);
}

void TerminalWindow::newWindow() {
    return;
}

void TerminalWindow::openPreferences() {
    m_dlg->load();
    if (m_dlg->exec() == QDialog::Accepted) {
        m_dlg->save();
    }
}