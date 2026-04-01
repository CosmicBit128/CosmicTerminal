#pragma once

#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>


class SettingsDialog : public QDialog
{
public:
    SettingsDialog(QWidget* parent);
    void load();
    void save();

private:
    QWidget* createGeneralTab();
    QWidget* createAdvancedTab();

private:
    QCheckBox* m_checkbox;
};