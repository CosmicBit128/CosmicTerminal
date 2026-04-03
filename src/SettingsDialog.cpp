#include "SettingsDialog.h"
#include "Settings.h"

#include <QSettings>
#include <QTabWidget>
#include <QFormLayout>
#include <QGridLayout>
#include <QDialogButtonBox>


SettingsDialog::SettingsDialog(QWidget* parent) {
    auto *layout = new QVBoxLayout(this);

    auto *tabs = new QTabWidget(this);

    tabs->addTab(createGeneralTab(), "General");
    tabs->addTab(createAppearanceTab(), "Apperance");
    tabs->addTab(createTabsTab(), "Tabs");
    // tabs->addTab(createApperanceTab(), "Keybindings");

    Settings::load();
    load();
    update();

    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

QWidget* SettingsDialog::createGeneralTab() {
    auto *widget = new QWidget();
    auto *layout = new QFormLayout(widget);

    m_terminalWidth = new QSpinBox();
    m_terminalHeight = new QSpinBox();
    m_terminalBufferHeight = new QSpinBox();
    m_enableBell = new QCheckBox("Enable Bell");

    m_terminalWidth->setRange(16, 512);
    m_terminalHeight->setRange(3, 512);
    m_terminalBufferHeight->setRange(3, 10000);

    layout->addRow("Terminal Width", m_terminalWidth);
    layout->addRow("Terminal Height", m_terminalHeight);
    layout->addRow("Buffer Height", m_terminalBufferHeight);
    layout->addRow(m_enableBell);

    return widget;
}

QWidget* SettingsDialog::createAppearanceTab() {
    auto *widget = new QWidget();
    auto *layout = new QFormLayout(widget);

    m_colorButtons.resize(16);
    m_colors.resize(16);
    
    QLabel* font = new QLabel("Font");
    m_fontButton = new QPushButton("JetBrains Mono Nerd Font 10pt");
    QLabel* colors = new QLabel("Colors");
    QWidget* color_widget = new QWidget();
    QGridLayout* color_grid = new QGridLayout(color_widget);
    for (int i = 0; i < 16; ++i) {
        auto* button = new QPushButton();
        button->setFixedSize(48, 32);

        color_grid->addWidget(button, (int)(i/8), i%8);
        m_colorButtons[i] = button;

        connect(button, &QPushButton::clicked, this, [this, i](){ m_colors[i] = chooseColor(m_colors[i]); update(); });
    }
    m_backgroundColorButton = new QPushButton();
    m_cursorColorButton = new QPushButton();
    m_selectionColorButton = new QPushButton();
    connect(m_backgroundColorButton, &QPushButton::clicked, this, [this](){ m_backgroundColor = chooseColor(m_backgroundColor); update(); });
    connect(m_cursorColorButton, &QPushButton::clicked, this, [this](){ m_cursorColor = chooseColor(m_cursorColor); update(); });
    connect(m_selectionColorButton, &QPushButton::clicked, this, [this](){ m_selectionColor = chooseColor(m_selectionColor); update(); });

    m_backgroundOpacity = new QSpinBox();
    m_backgroundOpacity->setRange(0, 100);
    m_backgroundOpacity->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);

    m_cursorStyle = new QComboBox();
    m_cursorStyle->addItem("Block", 0);
    m_cursorStyle->addItem("Vertical Line", 1);
    m_cursorStyle->addItem("Horizontal Line", 2);
    
    layout->addWidget(font);
    layout->addWidget(m_fontButton);
    layout->addWidget(colors);
    layout->addWidget(color_widget);
    layout->addRow("Background Color", m_backgroundColorButton);
    layout->addRow("Cursor Color", m_cursorColorButton);
    layout->addRow("Selection Color", m_selectionColorButton);
    layout->addRow("Background Opacity [%]", m_backgroundOpacity);
    layout->addRow("Cursor Style", m_cursorStyle);
    
    return widget;
}

QWidget* SettingsDialog::createTabsTab() {
    auto *widget = new QWidget();
    auto *layout = new QFormLayout(widget);

    m_showTabBar = new QCheckBox("Show Tab Bar");
    m_showTabCloseButton = new QCheckBox("Show Tab Close Button");
    m_showNewTabButton = new QCheckBox("Show New Tab Button");
    m_addNewTabs = new QComboBox();
    m_addNewTabs->addItem("At The End", 0);
    m_addNewTabs->addItem("At The Start", 1);
    m_addNewTabs->addItem("Afer The Current", 2);
    m_tabsAlignment = new QComboBox();
    m_tabsAlignment->addItem("Stretch", 0);
    m_tabsAlignment->addItem("To The Left", 1);

    layout->addWidget(m_showTabBar);
    layout->addWidget(m_showTabCloseButton);
    layout->addWidget(m_showNewTabButton);
    layout->addRow("Add New Tabs", m_addNewTabs);
    layout->addRow("Tabs Alignment", m_tabsAlignment);

    return widget;
}

QColor SettingsDialog::chooseColor(QColor init) {
    QColor color = QColorDialog::getColor(init, this);

    if (!color.isValid()) return QColor(Qt::black);

    return color;
}

void SettingsDialog::update() {
    for (int i = 0; i < 16; ++i) m_colorButtons[i]->setStyleSheet(QString("background-color: %1; border: 1px solid #444;").arg(m_colors[i].name()));
    QString background_button_style = QString("background-color: %1; border: 1px solid #444;").arg(m_backgroundColor.name());
    m_backgroundColorButton->setStyleSheet(background_button_style);
    QString cursor_button_style = QString("background-color: %1; border: 1px solid #444;").arg(m_cursorColor.name());
    m_cursorColorButton->setStyleSheet(cursor_button_style);
    QString selection_button_style = QString("background-color: %1; border: 1px solid #444;").arg(m_selectionColor.name());
    m_selectionColorButton->setStyleSheet(selection_button_style);
}

void SettingsDialog::load() {
    auto& d = Settings::data();

    m_terminalWidth->setValue(d.terminalWidth);
    m_terminalHeight->setValue(d.terminalHeight);
    m_terminalBufferHeight->setValue(d.terminalBufferHeight);
    m_enableBell->setChecked(d.enableBell);
    for (int i = 0; i < 16; ++i) m_colors[i] = d.colors[i];
    m_backgroundColor = d.backgroundColor;
    m_cursorColor = d.cursorColor;
    m_selectionColor = d.selectionColor;
    m_backgroundOpacity->setValue(d.backgroundOpacity);
    m_cursorStyle->setCurrentIndex(d.cursorStyle);
    m_showTabBar->setChecked(d.showTabBar);
    m_showTabCloseButton->setChecked(d.showTabCloseButton);
    m_showNewTabButton->setChecked(d.showNewTabButton);
    m_addNewTabs->setCurrentIndex(d.addNewTabs);
    m_tabsAlignment->setCurrentIndex(d.tabsAlignment);
}

void SettingsDialog::save() {
    auto& d = Settings::data();

    d.terminalWidth = m_terminalWidth->value();
    d.terminalHeight = m_terminalHeight->value();
    d.terminalBufferHeight = m_terminalBufferHeight->value();
    d.enableBell = m_enableBell->isChecked();
    for (int i = 0; i < 16; ++i) d.colors[i] = m_colors[i];
    d.backgroundColor = m_backgroundColor;
    d.cursorColor = m_cursorColor;
    d.selectionColor = m_selectionColor;
    d.backgroundOpacity = m_backgroundOpacity->value();
    d.cursorStyle = m_cursorStyle->currentData().toInt();
    d.showTabBar = m_showTabBar->isChecked();
    d.showTabCloseButton = m_showTabCloseButton->isChecked();
    d.showNewTabButton = m_showNewTabButton->isChecked();
    d.addNewTabs = m_addNewTabs->currentData().toInt();
    d.tabsAlignment = m_tabsAlignment->currentData().toInt();

    Settings::save();
}