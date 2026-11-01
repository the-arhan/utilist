#pragma once

#include <QObject>
#include <memory>

namespace utilist::app::ui {
class MainWindow;
}

namespace utilist::app::core {

class ProcessManager;
class ShortcutManager;
class UtilityManager;
class TrayController;

class ApplicationController : public QObject {
    Q_OBJECT

public:
    explicit ApplicationController(QObject* parent = nullptr);
    ~ApplicationController() override;

    bool initialize();
    void run();
    void shutdown();

    [[nodiscard]] UtilityManager* utilityManager() const noexcept { return m_utilityManager.get(); }
    [[nodiscard]] ShortcutManager* shortcutManager() const noexcept { return m_shortcutManager.get(); }
    [[nodiscard]] TrayController* trayController() const noexcept { return m_trayController.get(); }
    [[nodiscard]] ui::MainWindow* mainWindow() const noexcept { return m_mainWindow.get(); }

private:
    std::unique_ptr<ProcessManager> m_processManager;
    std::unique_ptr<ShortcutManager> m_shortcutManager;
    std::unique_ptr<UtilityManager> m_utilityManager;
    std::unique_ptr<TrayController> m_trayController;
    std::unique_ptr<ui::MainWindow> m_mainWindow;
};

} // namespace utilist::app::core
