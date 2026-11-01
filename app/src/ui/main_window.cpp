#include "main_window.hpp"
#include "dashboard_page.hpp"
#include "utilities_page.hpp"
#include "shortcuts_page.hpp"
#include "settings_page.hpp"
#include "about_page.hpp"
#include <core/configuration_manager.hpp>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QApplication>

namespace utilist::app::ui {

MainWindow::MainWindow(core::UtilityManager* utilityManager,
                       core::ShortcutManager* shortcutManager,
                       QWidget* parent)
    : QMainWindow(parent),
      m_utilityManager(utilityManager),
      m_shortcutManager(shortcutManager) {
    setupUi();
}

void MainWindow::setupUi() {
    setWindowTitle("UTILIST — Windows Desktop Utility Platform");
    setMinimumSize(960, 640);
    resize(1080, 720);

    // Modern dark theme stylesheet
    setStyleSheet(
        "QMainWindow { background-color: #0d1117; color: #f0f6fc; font-family: 'Segoe UI', sans-serif; }"
        "QWidget { font-family: 'Segoe UI', sans-serif; }"
        "QScrollBar:vertical { background: #0d1117; width: 8px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #30363d; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: #484f58; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    );

    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Left Navigation Sidebar
    auto* sidebar = new QWidget(this);
    sidebar->setFixedWidth(220);
    sidebar->setStyleSheet("background-color: #161b22; border-right: 1px solid #30363d;");
    setupSidebar(sidebar);
    mainLayout->addWidget(sidebar);

    // Right Page Stack
    m_pageStack = new QStackedWidget(this);
    m_pageStack->setStyleSheet("background-color: #0d1117;");

    m_dashboardPage = new DashboardPage(m_utilityManager, this);
    m_utilitiesPage = new UtilitiesPage(m_utilityManager, this);
    m_shortcutsPage = new ShortcutsPage(m_utilityManager, m_shortcutManager, this);
    m_settingsPage = new SettingsPage(this);
    m_aboutPage = new AboutPage(this);

    m_pageStack->addWidget(m_dashboardPage);
    m_pageStack->addWidget(m_utilitiesPage);
    m_pageStack->addWidget(m_shortcutsPage);
    m_pageStack->addWidget(m_settingsPage);
    m_pageStack->addWidget(m_aboutPage);

    mainLayout->addWidget(m_pageStack, 1);
    setCentralWidget(centralWidget);

    navigateToPage(0);
}

void MainWindow::setupSidebar(QWidget* container) {
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(16, 20, 16, 20);
    layout->setSpacing(8);

    // Logo / Title Header
    auto* header = new QHBoxLayout();
    header->setSpacing(10);

    auto* badge = new QLabel("U", container);
    badge->setFixedSize(32, 32);
    badge->setStyleSheet(
        "background-color: #00b4d8;"
        "color: #0d1117;"
        "border-radius: 6px;"
        "font-weight: bold;"
        "font-size: 16px;"
    );
    badge->setAlignment(Qt::AlignCenter);
    header->addWidget(badge);

    auto* title = new QLabel("UTILIST", container);
    title->setStyleSheet("color: #f0f6fc; font-size: 18px; font-weight: bold; letter-spacing: 1px;");
    header->addWidget(title);
    header->addStretch();

    layout->addLayout(header);
    layout->addSpacing(20);

    // Navigation buttons
    QStringList navLabels = {"Dashboard", "Utilities", "Shortcuts", "Settings", "About"};

    for (int i = 0; i < navLabels.size(); ++i) {
        auto* btn = new QPushButton(navLabels[i], container);
        btn->setFixedHeight(38);
        btn->setCursor(Qt::PointingHandCursor);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            navigateToPage(i);
        });
        m_navButtons.push_back(btn);
        layout->addWidget(btn);
    }

    layout->addStretch();

    // Footer status
    auto* footer = new QLabel("Ready • IPC Active", container);
    footer->setStyleSheet("color: #8b949e; font-size: 11px;");
    footer->setAlignment(Qt::AlignCenter);
    layout->addWidget(footer);
}

void MainWindow::updateNavStyles(int activeIndex) {
    for (size_t i = 0; i < m_navButtons.size(); ++i) {
        if (static_cast<int>(i) == activeIndex) {
            m_navButtons[i]->setStyleSheet(
                "QPushButton {"
                "  background-color: #21262d;"
                "  color: #00b4d8;"
                "  border: 1px solid #30363d;"
                "  border-left: 3px solid #00b4d8;"
                "  border-radius: 6px;"
                "  text-align: left;"
                "  padding-left: 14px;"
                "  font-size: 13px;"
                "  font-weight: bold;"
                "}"
            );
        } else {
            m_navButtons[i]->setStyleSheet(
                "QPushButton {"
                "  background-color: transparent;"
                "  color: #c9d1d9;"
                "  border: none;"
                "  border-radius: 6px;"
                "  text-align: left;"
                "  padding-left: 16px;"
                "  font-size: 13px;"
                "}"
                "QPushButton:hover {"
                "  background-color: #1f242c;"
                "  color: #f0f6fc;"
                "}"
            );
        }
    }
}

void MainWindow::navigateToPage(int pageIndex) {
    if (pageIndex >= 0 && pageIndex < m_pageStack->count()) {
        m_pageStack->setCurrentIndex(pageIndex);
        updateNavStyles(pageIndex);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (core::ConfigurationManager::instance().appSettings().minimizeToTrayOnClose) {
        event->ignore();
        hide();
    } else {
        event->accept();
        QApplication::quit();
    }
}

} // namespace utilist::app::ui
