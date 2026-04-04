#include "TerminalWindow.h"
#include "Settings.h"
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <unistd.h>


TerminalWindow::TerminalWindow() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);

    m_dlg = new SettingsDialog(this);
    m_dlg->setWindowTitle("Preferences");

    setAttribute(Qt::WA_TranslucentBackground);

    m_tabBar = new TabBar(container);
    m_termWidget = new TerminalWidget(container);

    m_tabBar->setFixedHeight(32);
    layout->addWidget(m_tabBar, 0);
    layout->addWidget(m_termWidget, 1);

    setCentralWidget(container);

    connect(m_tabBar->bar(), &QTabBar::currentChanged, this, [this](int index){ switchToTab(index); });
    connect(m_tabBar->bar(), &QTabBar::tabCloseRequested, this, [this](int index){ closeTab(index); });
    connect(m_tabBar->newTabBtn(), &QPushButton::clicked, this, [this]{ createTab(); });
    connect(m_termWidget, &TerminalWidget::tabClosed, this, [this]{ closeTab(m_tabBar->bar()->currentIndex()); });
    connect(m_termWidget, &TerminalWidget::requestNewTab, this, [this]{ createTab(); });

    // --- Menu Bar ---
    m_menu = new QMenuBar(this);

    // File Menu
    QMenu* fileMenu = m_menu->addMenu("File");
    QAction* a_new_tab = fileMenu->addAction("New Tab");
    QAction* a_new_win = fileMenu->addAction("New Window");
    fileMenu->addSeparator();
    QAction* a_quit = fileMenu->addAction("Quit");

    // Edit Menu
    QMenu* editMenu = m_menu->addMenu("Edit");
    QAction* a_copy = editMenu->addAction("Copy");
    QAction* a_paste = editMenu->addAction("Paste");
    editMenu->addSeparator();
    QAction* a_pref = editMenu->addAction("Preferences");

    // Connections
    connect(a_new_tab, &QAction::triggered, this, &TerminalWindow::createTab);
    connect(a_new_win, &QAction::triggered, this, &TerminalWindow::newWindow);
    connect(a_quit, &QAction::triggered, this, [this]{ qApp->quit(); });
    connect(a_copy, &QAction::triggered, this, [this]{ m_termWidget->copySelection(); });
    connect(a_paste, &QAction::triggered, this, [this]{ m_termWidget->paste(); });
    connect(a_pref, &QAction::triggered, this, &TerminalWindow::openPreferences);

    setMenuBar(m_menu);

    // Create initial tab
    createTab();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    setFixedSize(sizeHint());
}

TerminalTab* TerminalWindow::createTab() {
    auto& s = Settings::data();

    auto* tab = new TerminalTab();
    tab->model = new TerminalModel(m_termWidget->cols(), m_termWidget->rows(), s.terminalBufferHeight, m_termWidget->charWidth(), m_termWidget->charHeight());
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

    int index = 0;
    switch (s.addNewTabs) {
        case 0: index = (int)m_tabs.size(); break;
        case 1: index = 0; break;
        case 2: index = m_tabBar->bar()->currentIndex() + 1; break;
    }
    m_tabs.insert(m_tabs.begin() + index, tab);
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
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if(len == -1) { perror("readlink"); return; }
    exe_path[len] = '\0';

    char *argv[] = { exe_path, NULL };
    pid_t pid = fork();
    if(pid == 0) {
        execv(argv[0], argv);
        perror("exec failed");
        _exit(1);
    }
}

void TerminalWindow::openPreferences() {
    m_dlg->load();
    if (m_dlg->exec() == QDialog::Accepted) {
        m_dlg->save();

        auto& s = Settings::data();
        m_tabBar->bar()->setExpanding(!s.tabsAlignment);
        m_tabBar->bar()->setTabsClosable(s.showTabCloseButton);
        m_tabBar->setNewTabButtonVisible(s.showNewTabButton);
        if (s.showTabBar) m_tabBar->show();
        else m_tabBar->hide();
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        setFixedSize(sizeHint());
    }
}

QSize TerminalWindow::sizeHint() const {
    int tabH = (m_tabBar && !m_tabBar->isHidden()) ? m_tabBar->sizeHint().height() : 0;
    auto& s = Settings::data();
    return QSize(
        s.terminalWidth * m_termWidget->charWidth(),
        s.terminalHeight * m_termWidget->charHeight() + tabH
 
    );
}