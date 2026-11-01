#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <memory>
#include <string>

namespace utilist::app::core {

class UtilityManager;

class TrayController : public QObject {
    Q_OBJECT

public:
    explicit TrayController(UtilityManager* utilityManager, QObject* parent = nullptr);
    ~TrayController() override;

    void initialize();
    void show();
    void showNotification(const QString& title, const QString& message);

signals:
    void openMainWindowRequested();
    void openSettingsRequested();
    void exitApplicationRequested();

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void rebuildContextMenu();

private:
    UtilityManager* m_utilityManager{nullptr};
    QSystemTrayIcon* m_trayIcon{nullptr};
    QMenu* m_trayMenu{nullptr};

    void setupTrayIcon();
};

} // namespace utilist::app::core
