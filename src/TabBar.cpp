#include "TabBar.h"

#include <QStyle>


TabBar::TabBar(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::NoFocus);

    m_layout = new QHBoxLayout(this);
    m_layout->setSpacing(2);
    m_layout->setContentsMargins(4,2,4,2);
    m_layout->addStretch();

    m_newTabBtn = new QPushButton("+", this);
    m_newTabBtn->setFixedSize(24, 24);
    m_newTabBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_newTabBtn, &QPushButton::clicked, this, &TabBar::newTabRequested);
    m_layout->addWidget(m_newTabBtn);
}

void TabBar::addTab(const QString& title, int index) {
    auto* w = new QWidget(this);
    auto* row = new QHBoxLayout(w);
    row->setContentsMargins(8,2,4,2);
    row->setSpacing(4);

    auto* label = new QPushButton(title, w);
    label->setFlat(true);
    connect(label, &QPushButton::clicked, this, [this, index]{ emit tabClicked(index); });

    auto* close = new QPushButton("×", w);
    close->setFixedSize(16,16);
    close->setFlat(true);
    connect(close, &QPushButton::clicked, this, [this, index]{ emit tabClosed(index); });

    label->setFocusPolicy(Qt::NoFocus);
    close->setFocusPolicy(Qt::NoFocus);

    row->addWidget(label);
    row->addWidget(close);

    m_layout->insertWidget(m_layout->count() - 2, w);
    m_tabs.insert(m_tabs.begin() + index, w);
}

void TabBar::removeTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    m_layout->removeWidget(m_tabs[index]);
    delete m_tabs[index];
    m_tabs.erase(m_tabs.begin() + index);
}

void TabBar::setActiveTab(int index) {
    for (int i = 0; i < (int)m_tabs.size(); ++i) {
        m_tabs[i]->setProperty("active", i == index);
        QStyle *style = m_tabs[i]->style();
        style->unpolish(m_tabs[i]);
        style->polish(m_tabs[i]);
    }
}