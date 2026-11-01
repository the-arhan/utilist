#include "application_controller.hpp"
#include "logging_service.hpp"
#include "configuration_manager.hpp"
#include "utility_registry.hpp"
#include "process_manager.hpp"
#include "shortcut_manager.hpp"
#include "utility_manager.hpp"
#include "tray_controller.hpp"
#include <platform/windows_paths.hpp>
#include <ui/main_window.hpp>
#include <QCoreApplication>
#include <filesystem>

namespace utilist::app::core {

namespace fs = std::filesystem;

ApplicationController::ApplicationController(QObject* parent)
    : QObject(parent) {}

ApplicationController::~ApplicationController() {
    shutdown();
}

bool ApplicationController::initialize() {
    // 1. Storage & Logging
    platform::WindowsPaths::ensureDirectoriesExist();
    LoggingService::instance().initialize(platform::WindowsPaths::getLogsDir());
    LoggingService::instance().info("=== Starting UTILIST Desktop Platform v1.0.0 ===");

    // 2. Configuration
    ConfigurationManager::instance().load(platform::WindowsPaths::getConfigFilePath());

    // 3. Search Paths
    UtilityRegistry::instance().addSearchPath(platform::WindowsPaths::getUtilitiesDir());

    // Also add build directory search path for development, bundled utilities, and examples
    auto appDir = fs::current_path();
    UtilityRegistry::instance().addSearchPath(appDir / "utilities");
    UtilityRegistry::instance().addSearchPath(appDir / "utilities" / "examples");
    UtilityRegistry::instance().addSearchPath(appDir / "build" / "windows-msvc-debug" / "utilities");
    UtilityRegistry::instance().addSearchPath(appDir / "build" / "windows-msvc-debug" / "utilities" / "examples");
    UtilityRegistry::instance().addSearchPath(appDir / "build" / "windows-msvc-release" / "utilities");
    UtilityRegistry::instance().addSearchPath(appDir / "build" / "windows-msvc-release" / "utilities" / "examples");

    // Also search relative to the executable directory
    auto exeDir = platform::WindowsPaths::getExecutableDir();
    UtilityRegistry::instance().addSearchPath(exeDir / "utilities");
    try {
        auto root = exeDir.parent_path().parent_path().parent_path().parent_path();
        UtilityRegistry::instance().addSearchPath(root / "utilities");
        UtilityRegistry::instance().addSearchPath(root / "utilities" / "examples");
        UtilityRegistry::instance().addSearchPath(root / "build" / "windows-msvc-debug" / "utilities");
        UtilityRegistry::instance().addSearchPath(root / "build" / "windows-msvc-debug" / "utilities" / "examples");
        UtilityRegistry::instance().addSearchPath(root / "build" / "windows-msvc-release" / "utilities");
        UtilityRegistry::instance().addSearchPath(root / "build" / "windows-msvc-release" / "utilities" / "examples");
    } catch (...) {}

    // 4. Subsystems
    m_processManager = std::make_unique<ProcessManager>(this);
    m_shortcutManager = std::make_unique<ShortcutManager>(this);
    m_utilityManager = std::make_unique<UtilityManager>(m_processManager.get(), m_shortcutManager.get(), this);

    connect(m_shortcutManager.get(), &ShortcutManager::shortcutTriggered,
            m_utilityManager.get(), &UtilityManager::handleShortcutTriggered);

    // 5. Tray Controller
    m_trayController = std::make_unique<TrayController>(m_utilityManager.get(), this);
    m_trayController->initialize();

    connect(m_trayController.get(), &TrayController::openMainWindowRequested, this, [this]() {
        if (m_mainWindow) {
            m_mainWindow->showNormal();
            m_mainWindow->activateWindow();
        }
    });

    connect(m_trayController.get(), &TrayController::openSettingsRequested, this, [this]() {
        if (m_mainWindow) {
            m_mainWindow->showNormal();
            m_mainWindow->navigateToPage(3); // Settings page
            m_mainWindow->activateWindow();
        }
    });

    connect(m_trayController.get(), &TrayController::exitApplicationRequested, this, [this]() {
        shutdown();
        QCoreApplication::quit();
    });

    // 6. UI
    m_mainWindow = std::make_unique<ui::MainWindow>(m_utilityManager.get(), m_shortcutManager.get());

    // 7. Load utilities & start configured auto-starts
    m_utilityManager->initialize();
    m_trayController->show();

    return true;
}

void ApplicationController::run() {
    if (m_mainWindow) {
        m_mainWindow->show();
    }
}

void ApplicationController::shutdown() {
    LoggingService::instance().info("Shutting down UTILIST and all managed utility processes...");
    if (m_utilityManager) {
        m_utilityManager->shutdownAll();
    }
    if (m_shortcutManager) {
        m_shortcutManager->unregisterAll();
    }
    ConfigurationManager::instance().save();
    LoggingService::instance().info("UTILIST shutdown complete.");
}

} // namespace utilist::app::core
