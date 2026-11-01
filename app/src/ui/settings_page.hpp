#pragma once

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>

namespace utilist::app::ui {

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

private:
    QCheckBox* m_minimizeTrayCheck{nullptr};
    QCheckBox* m_startupCheck{nullptr};

    void setupUi();
    void loadSettings();
    void saveSettings();
};

} // namespace utilist::app::ui
