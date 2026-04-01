#include "SettingsDialog.h"

#include <QSettings>
#include <QTabWidget>
#include <QDialogButtonBox>


SettingsDialog::SettingsDialog(QWidget* parent) {
    auto *layout = new QVBoxLayout(this);

    auto *tabs = new QTabWidget(this);

    tabs->addTab(createGeneralTab(), "General");
    tabs->addTab(createAdvancedTab(), "Advanced");

    layout->addWidget(tabs);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

QWidget* SettingsDialog::createAdvancedTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);

    QLabel* advanced = new QLabel("Advanced tab");
    layout->addWidget(advanced);

    return widget;
}

QWidget* SettingsDialog::createGeneralTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);

    m_checkbox = new QCheckBox("Cursor blinking");
    layout->addWidget(m_checkbox);

    return widget;
}

void SettingsDialog::load() {
    QSettings s;

    m_checkbox->setChecked(s.value("general/cursor_blink", true).toBool());
}

void SettingsDialog::save() {
    QSettings s;

    s.setValue("general/cursor_blink", m_checkbox->isChecked());
}