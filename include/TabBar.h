#pragma once

#include <QWidget>
#include <QTabBar>
#include <QHBoxLayout>
#include <QPushButton>
#include <vector>

class TabBar : public QWidget {
    Q_OBJECT
public:
    explicit TabBar(QWidget* parent = nullptr);
    void addTab(const QString& title, int index);
    void removeTab(int index);
    void setNewTabButtonVisible(bool visible);

    QTabBar* bar() const { return m_bar; }
    QPushButton* newTabBtn() const { return m_newTabBtn; }

signals:
    void newTabRequested();

private:
    QTabBar* m_bar = nullptr;
    QHBoxLayout* m_layout = nullptr;
    QPushButton* m_newTabBtn = nullptr;
};