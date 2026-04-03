#include "SettingsDialog.h"

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
    QSettings s;

    QString color_codes[2][16] = {
        { // GNOME Terminal: Tango Theme
            "#2E3436", "#CC0000", "#4E9A06", "#C4A000", "#3465A4", "#75507B", "#06989A", "#D3D7CF",
            "#555753", "#EF2929", "#8AE234", "#FCE94F", "#729FCF", "#AD7FA8", "#34E2E2", "#EEEEEC"
        },
        { // Kitty Default
            "#32363D", "#E06B74", "#98C379", "#E5C07A", "#62AEEF", "#C778DD", "#55B6C2", "#ABB2BF",
            "#50545B", "#EA757E", "#A2CD83", "#EFCA84", "#6CB8F9", "#D282E7", "#5FC0CC", "#B5BCC9"
        }
    };

    m_terminalWidth->setValue(s.value("TerminalWidth", 128).toInt());
    m_terminalHeight->setValue(s.value("TerminalHeight", 32).toInt());
    m_terminalBufferHeight->setValue(s.value("TerminalBufferHeight", 4096).toInt());
    m_enableBell->setChecked(s.value("EnableBell", true).toBool());
    s.beginReadArray("Appearance/Colors");
    for (int i = 0; i < 16; ++i) {
        s.setArrayIndex(i);
        m_colors[i] = s.value("Color", QColor(color_codes[1][i])).value<QColor>();
    }
    s.endArray();
    m_backgroundColor = s.value("Appearance/BackgroundColor", "#000000").value<QColor>();
    m_cursorColor = s.value("Appearance/CursorColor", "#ffffff").value<QColor>();
    m_selectionColor = s.value("Appearance/SelectionColor", "#5980ff").value<QColor>();
    m_backgroundOpacity->setValue(s.value("Appearance/BackgroundOpacity", 80).toInt());
    m_cursorStyle->setCurrentIndex(s.value("Appearance/CursorStyle", 0).toInt());
    m_showTabBar->setChecked(s.value("Tabs/ShowTabBar", true).toBool());
    m_showTabCloseButton->setChecked(s.value("Tabs/ShowTabCloseButton", true).toBool());
    m_showNewTabButton->setChecked(s.value("Tabs/ShowNewTabButton", true).toBool());
    m_addNewTabs->setCurrentIndex(s.value("Tabs/AddNewTabs", 0).toInt());
    m_tabsAlignment->setCurrentIndex(s.value("Tabs/TabsAlignment", 0).toInt());
}

void SettingsDialog::save() {
    QSettings s;

    s.setValue("TerminalWidth", m_terminalWidth->value());
    s.setValue("TerminalHeight", m_terminalHeight->value());
    s.setValue("TerminalBufferHeight", m_terminalBufferHeight->value());
    s.setValue("EnableBell", m_enableBell->isChecked());
    s.beginWriteArray("Appearance/Colors");
    for (int i = 0; i < 16; ++i) {
        s.setArrayIndex(i);
        s.setValue("Color", m_colors[i]);
    }
    s.endArray();
    s.setValue("Appearance/BackgroundColor", m_backgroundColor);
    s.setValue("Appearance/CursorColor", m_cursorColor);
    s.setValue("Appearance/SelectionColor", m_selectionColor);
    s.setValue("Appearance/BackgroundOpacity", m_backgroundOpacity->value());
    s.setValue("Appearance/CursorStyle", m_cursorStyle->currentData().toInt());
    s.setValue("Tabs/ShowTabBar", m_showTabBar->isChecked());
    s.setValue("Tabs/ShowTabCloseButton", m_showTabCloseButton->isChecked());
    s.setValue("Tabs/ShowNewTabButton", m_showNewTabButton->isChecked());
    s.setValue("Tabs/AddNewTabs", m_addNewTabs->currentData().toInt());
    s.setValue("Tabs/TabsAlignment", m_tabsAlignment->currentData().toInt());
}