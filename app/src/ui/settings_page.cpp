#include "settings_page.hpp"
#include <core/configuration_manager.hpp>
#include <platform/windows_paths.hpp>
#include <platform/windows_startup.hpp>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QDesktopServices>
#include <QUrl>

namespace utilist::app::ui {

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent) {
    setupUi();
    loadSettings();
}

void SettingsPage::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(20);

    // Header
    auto* titleLabel = new QLabel("Settings", this);
    titleLabel->setStyleSheet("color: #f0f6fc; font-size: 22px; font-weight: bold;");
    rootLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel("Configure behavior, startup preferences, and diagnostics.", this);
    subtitleLabel->setStyleSheet("color: #8b949e; font-size: 13px;");
    rootLayout->addWidget(subtitleLabel);

    // Preferences Card
    auto* prefCard = new QFrame(this);
    prefCard->setStyleSheet(
        "background-color: #161b22;"
        "border: 1px solid #30363d;"
        "border-radius: 8px;"
        "padding: 18px;"
    );
    auto* prefLayout = new QVBoxLayout(prefCard);
    prefLayout->setSpacing(14);

    auto* prefTitle = new QLabel("Window & Startup Behavior", prefCard);
    prefTitle->setStyleSheet("color: #f0f6fc; font-size: 15px; font-weight: bold;");
    prefLayout->addWidget(prefTitle);

    m_minimizeTrayCheck = new QCheckBox("Minimize to system tray when closing the window", prefCard);
    m_minimizeTrayCheck->setStyleSheet(
        "QCheckBox { color: #c9d1d9; font-size: 13px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1px solid #30363d; background: #0d1117; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
    );
    connect(m_minimizeTrayCheck, &QCheckBox::toggled, this, &SettingsPage::saveSettings);
    prefLayout->addWidget(m_minimizeTrayCheck);

    m_startupCheck = new QCheckBox("Start UTILIST automatically when signing in to Windows", prefCard);
    m_startupCheck->setStyleSheet(
        "QCheckBox { color: #c9d1d9; font-size: 13px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1px solid #30363d; background: #0d1117; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
    );
    connect(m_startupCheck, &QCheckBox::toggled, this, &SettingsPage::saveSettings);
    prefLayout->addWidget(m_startupCheck);

    rootLayout->addWidget(prefCard);

    // Diagnostics & Paths Card
    auto* diagCard = new QFrame(this);
    diagCard->setStyleSheet(
        "background-color: #161b22;"
        "border: 1px solid #30363d;"
        "border-radius: 8px;"
        "padding: 18px;"
    );
    auto* diagLayout = new QVBoxLayout(diagCard);
    diagLayout->setSpacing(12);

    auto* diagTitle = new QLabel("Storage & Diagnostics", diagCard);
    diagTitle->setStyleSheet("color: #f0f6fc; font-size: 15px; font-weight: bold;");
    diagLayout->addWidget(diagTitle);

    auto createPathBtn = [this](const QString& text, const QString& pathStr, bool isFolder) -> QHBoxLayout* {
        auto* row = new QHBoxLayout();
        auto* label = new QLabel(text, this);
        label->setStyleSheet("color: #c9d1d9; font-size: 13px;");
        row->addWidget(label);

        row->addStretch();

        auto* btn = new QPushButton("Open", this);
        btn->setFixedWidth(80);
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  background-color: #21262d;"
            "  color: #c9d1d9;"
            "  border: 1px solid #30363d;"
            "  border-radius: 4px;"
            "  font-size: 12px;"
            "}"
            "QPushButton:hover { background-color: #30363d; color: #f0f6fc; }"
        );
        connect(btn, &QPushButton::clicked, this, [pathStr, isFolder]() {
            if (isFolder) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(pathStr));
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(pathStr));
            }
        });
        row->addWidget(btn);
        return row;
    };

    diagLayout->addLayout(createPathBtn(
        "Configuration file: %LOCALAPPDATA%\\Utilist\\config.json",
        QString::fromStdWString(platform::WindowsPaths::getConfigFilePath().wstring()),
        false
    ));

    diagLayout->addLayout(createPathBtn(
        "Log files directory: %LOCALAPPDATA%\\Utilist\\logs",
        QString::fromStdWString(platform::WindowsPaths::getLogsDir().wstring()),
        true
    ));

    diagLayout->addLayout(createPathBtn(
        "Utilities directory: %LOCALAPPDATA%\\Utilist\\utilities",
        QString::fromStdWString(platform::WindowsPaths::getUtilitiesDir().wstring()),
        true
    ));

    rootLayout->addWidget(diagCard);
    rootLayout->addStretch();
}

void SettingsPage::loadSettings() {
    auto& cfg = core::ConfigurationManager::instance();
    m_minimizeTrayCheck->setChecked(cfg.appSettings().minimizeToTrayOnClose);
    m_startupCheck->setChecked(platform::WindowsStartup::isStartupEnabled());
}

void SettingsPage::saveSettings() {
    auto& cfg = core::ConfigurationManager::instance();
    cfg.appSettings().minimizeToTrayOnClose = m_minimizeTrayCheck->isChecked();
    cfg.appSettings().startWithWindows = m_startupCheck->isChecked();
    cfg.save();

    platform::WindowsStartup::setStartupEnabled(m_startupCheck->isChecked());
}

} // namespace utilist::app::ui
