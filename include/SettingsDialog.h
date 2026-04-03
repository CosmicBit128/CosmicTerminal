#pragma once

#include <QWidget>
#include <QDialog>
#include <QVector>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QFontDialog>
#include <QColorDialog>
#include <QCheckBox>


class SettingsDialog : public QDialog
{
public:
    SettingsDialog(QWidget* parent);
    void load();
    void save();

private:
    QWidget* createGeneralTab();
    QWidget* createAppearanceTab();
    QWidget* createTabsTab();
    // TODO: Add custom keybindings
    // QWidget* createKeybindingsTab();
    void update();
    QColor chooseColor(QColor init);

private:
    // General
    QSpinBox* m_terminalWidth;
    QSpinBox* m_terminalHeight;
    QSpinBox* m_terminalBufferHeight;
    QCheckBox* m_enableBell;

    // Apperance
    QPushButton* m_fontButton;
    QVector<QPushButton*> m_colorButtons;
    QVector<QColor> m_colors;
    QPushButton* m_backgroundColorButton;
    QPushButton* m_cursorColorButton;
    QPushButton* m_selectionColorButton;
    QSpinBox* m_backgroundOpacity;
    QComboBox* m_cursorStyle;
    QColor m_backgroundColor;
    QColor m_cursorColor;
    QColor m_selectionColor;

    // Tabs
    QCheckBox* m_showTabBar;
    QCheckBox* m_showTabCloseButton;
    QCheckBox* m_showNewTabButton;
    QComboBox* m_addNewTabs;
    QComboBox* m_tabsAlignment;
};