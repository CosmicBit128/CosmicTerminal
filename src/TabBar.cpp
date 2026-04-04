#include "TabBar.h"
#include "Settings.h"

#include <QStyle>


TabBar::TabBar(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::NoFocus);

    auto& s = Settings::data();
    m_bar = new QTabBar(this);
    m_bar->setMovable(true);
    m_bar->setTabsClosable(true);
    m_bar->setExpanding(!s.tabsAlignment);
    if (s.showTabBar) show();
    else hide();
    
    setAutoFillBackground(true);

    m_layout = new QHBoxLayout(this);
    m_layout->setSpacing(2);
    m_layout->setContentsMargins(0,0,0,0);

    m_newTabBtn = new QPushButton("+", this);
    m_newTabBtn->setFixedSize(32, 32);
    m_newTabBtn->setFocusPolicy(Qt::NoFocus);

    m_layout->addWidget(m_bar);
    m_layout->addWidget(m_newTabBtn);
}

void TabBar::addTab(const QString& title, int index) {
    m_bar->insertTab(index, title);
}

void TabBar::removeTab(int index) {
    if (index < 0 || index >= m_bar->count()) return;
    m_bar->removeTab(index);
}

void TabBar::setNewTabButtonVisible(bool visible) {
    if (visible) m_newTabBtn->show();
    else m_newTabBtn->hide();
}