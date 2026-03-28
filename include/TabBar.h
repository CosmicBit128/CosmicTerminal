#pragma once

#include <QWidget>
#include <QHBoxLayout>
#include <QPushButton>
#include <vector>

class TabBar : public QWidget {
    Q_OBJECT
public:
    explicit TabBar(QWidget* parent = nullptr);
    void addTab(const QString& title, int index);
    void removeTab(int index);
    void setActiveTab(int index);

signals:
    void tabClicked(int index);
    void tabClosed(int index);
    void newTabRequested();

private:
    QHBoxLayout* m_layout = nullptr;
    QPushButton* m_newTabBtn = nullptr;
    std::vector<QWidget*> m_tabs;
};