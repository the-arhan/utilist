#include "tray_controller.hpp"
#include "utility_manager.hpp"
#include <QPainter>
#include <QPixmap>
#include <QAction>
#include <QApplication>

namespace utilist::app::core {

namespace {

QIcon createUtilistTrayIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Dark sleek circular badge with cyan accent
    painter.setBrush(QColor(24, 28, 36));
    painter.setPen(QPen(QColor(0, 180, 216), 2));
    painter.drawRoundedRect(2, 2, 28, 28, 6, 6);

    // Letter 'U'
    painter.setPen(QPen(QColor(240, 246, 252), 3));
    painter.drawArc(8, 10, 16, 14, 0, -180 * 16);
    painter.drawLine(8, 8, 8, 17);
    painter.drawLine(24, 8, 24, 17);

    return QIcon(pixmap);
}

} // anonymous namespace

TrayController::TrayController(UtilityManager* utilityManager, QObject* parent)
    : QObject(parent),
      m_utilityManager(utilityManager),
      m_trayIcon(new QSystemTrayIcon(this)),
      m_trayMenu(new QMenu()) {
    connect(m_utilityManager, &UtilityManager::utilityStateChanged, this, &TrayController::rebuildContextMenu);
    connect(m_utilityManager, &UtilityManager::utilitiesRefreshed, this, &TrayController::rebuildContextMenu);
}

TrayController::~TrayController() {
    delete m_trayMenu;
}

void TrayController::initialize() {
    setupTrayIcon();
    rebuildContextMenu();
}

void TrayController::setupTrayIcon() {
    m_trayIcon->setIcon(createUtilistTrayIcon());
    m_trayIcon->setToolTip("UTILIST — Desktop Utility Platform");

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayController::onTrayIconActivated);
    m_trayIcon->setContextMenu(m_trayMenu);
}

void TrayController::show() {
    m_trayIcon->show();
}

void TrayController::showNotification(const QString& title, const QString& message) {
    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
}

void TrayController::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        emit openMainWindowRequested();
    }
}

void TrayController::rebuildContextMenu() {
    m_trayMenu->clear();

    auto* openAction = m_trayMenu->addAction("Open UTILIST");
    QFont boldFont = openAction->font();
    boldFont.setBold(true);
    openAction->setFont(boldFont);
    connect(openAction, &QAction::triggered, this, &TrayController::openMainWindowRequested);

    m_trayMenu->addSeparator();

    auto utilities = m_utilityManager->getManagedUtilities();
    if (utilities.empty()) {
        auto* emptyAction = m_trayMenu->addAction("No utilities installed");
        emptyAction->setEnabled(false);
    } else {
        for (const auto& u : utilities) {
            QString statusText = u.enabled ? (u.processState == ProcessState::Running ? (u.featureActive ? "Active" : "Running") : "Stopped") : "Disabled";
            QString label = QString::fromStdString(u.descriptor.name) + " (" + statusText + ")";

            auto* uAction = m_trayMenu->addAction(label);
            uAction->setEnabled(u.enabled);

            std::string uId = u.descriptor.id;
            connect(uAction, &QAction::triggered, this, [this, uId]() {
                m_utilityManager->toggleUtilityFeature(uId);
            });
        }
    }

    m_trayMenu->addSeparator();

    auto* settingsAction = m_trayMenu->addAction("Settings...");
    connect(settingsAction, &QAction::triggered, this, &TrayController::openSettingsRequested);

    m_trayMenu->addSeparator();

    auto* exitAction = m_trayMenu->addAction("Exit UTILIST");
    connect(exitAction, &QAction::triggered, this, &TrayController::exitApplicationRequested);
}

} // namespace utilist::app::core
