#include "utility_manager.hpp"
#include "logging_service.hpp"
#include <filesystem>
#include <random>

namespace utilist::app::core {

namespace fs = std::filesystem;

UtilityManager::UtilityManager(ProcessManager* processManager,
                               ShortcutManager* shortcutManager,
                               QObject* parent)
    : QObject(parent),
      m_processManager(processManager),
      m_shortcutManager(shortcutManager) {
    connect(m_processManager, &ProcessManager::processStarted, this, [this](const std::string& id, uint32_t pid) {
        auto it = m_utilities.find(id);
        if (it != m_utilities.end()) {
            it->second.processId = pid;
            it->second.processState = ProcessState::Running;
            emit utilityStateChanged(id);
        }
    });

    connect(m_processManager, &ProcessManager::processExited, this, [this](const std::string& id, int exitCode) {
        auto it = m_utilities.find(id);
        if (it != m_utilities.end()) {
            it->second.processId = 0;
            it->second.processState = (exitCode == 0) ? ProcessState::Stopped : ProcessState::Error;
            it->second.featureActive = false;
            it->second.stateDescription = (exitCode == 0) ? "Stopped" : "Process exited unexpectedly";
            if (m_ipcClients.count(id)) {
                m_ipcClients[id]->close();
            }
            emit utilityStateChanged(id);
        }
    });
}

UtilityManager::~UtilityManager() {
    shutdownAll();
}

std::wstring UtilityManager::generatePipeName(const std::string& utilityId) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist(100000, 999999);

    std::string safeId = utilityId;
    std::replace(safeId.begin(), safeId.end(), '/', '.');
    std::replace(safeId.begin(), safeId.end(), '\\', '.');

    std::string pipeStr = "\\\\.\\pipe\\utilist." + safeId + "." + std::to_string(dist(gen));
    return std::wstring(pipeStr.begin(), pipeStr.end());
}

void UtilityManager::initialize() {
    refreshUtilities();
}

void UtilityManager::refreshUtilities() {
    UtilityRegistry::instance().rescan();
    auto descriptors = UtilityRegistry::instance().getAllUtilities();

    for (const auto& desc : descriptors) {
        auto uCfg = ConfigurationManager::instance().getUtilityConfig(desc.id);
        auto sCfg = ConfigurationManager::instance().getShortcutConfig(desc.id);

        ManagedUtility managed;
        managed.descriptor = desc;
        managed.enabled = uCfg.enabled;
        managed.processState = ProcessState::Stopped;
        managed.featureActive = false;
        managed.stateDescription = "Stopped";

        std::string shortcut = sCfg.keySequence.empty() ? desc.defaultShortcut : sCfg.keySequence;
        managed.configuredShortcut = shortcut;

        m_utilities[desc.id] = managed;

        if (managed.enabled && !shortcut.empty()) {
            m_shortcutManager->registerShortcut(desc.id, shortcut, "toggle");
        }

        if (managed.enabled && uCfg.autoStart) {
            startUtility(desc.id);
        }
    }

    emit utilitiesRefreshed();
}

std::vector<ManagedUtility> UtilityManager::getManagedUtilities() const {
    std::vector<ManagedUtility> list;
    list.reserve(m_utilities.size());
    for (const auto& [id, util] : m_utilities) {
        list.push_back(util);
    }
    return list;
}

std::optional<ManagedUtility> UtilityManager::getManagedUtility(const std::string& utilityId) const {
    auto it = m_utilities.find(utilityId);
    if (it != m_utilities.end()) {
        return it->second;
    }
    return std::nullopt;
}

void UtilityManager::setupIpcForUtility(const std::string& utilityId, const std::wstring& pipeName) {
    auto client = std::make_unique<IpcClient>(this);
    if (!client->createPipeServer(pipeName)) {
        LoggingService::instance().error("Failed to create named pipe server for utility: " + utilityId);
        return;
    }

    connect(client.get(), &IpcClient::connected, this, [this, utilityId]() {
        LoggingService::instance().info("Handshake with utility: " + utilityId);
        if (m_ipcClients.count(utilityId)) {
            m_ipcClients[utilityId]->sendCommand("handshake");
            m_ipcClients[utilityId]->sendCommand("get_status");
        }
    });

    connect(client.get(), &IpcClient::statusUpdated, this, [this, utilityId](const protocol::StatusPayload& status) {
        auto it = m_utilities.find(utilityId);
        if (it != m_utilities.end()) {
            it->second.featureActive = status.featureActive;
            it->second.stateDescription = status.stateDescription;
            it->second.uptimeSeconds = status.uptimeSeconds;
            emit utilityStateChanged(utilityId);
        }
    });

    m_ipcClients[utilityId] = std::move(client);
}

bool UtilityManager::startUtility(const std::string& utilityId) {
    auto it = m_utilities.find(utilityId);
    if (it == m_utilities.end()) return false;

    if (!it->second.enabled) {
        LoggingService::instance().warn("Cannot start disabled utility: " + utilityId);
        return false;
    }

    if (m_processManager->isRunning(utilityId)) {
        return true;
    }

    std::wstring pipeName = generatePipeName(utilityId);
    setupIpcForUtility(utilityId, pipeName);

    std::string exePath = it->second.descriptor.getExecutableAbsolutePath();
    std::wstring wExePath(exePath.begin(), exePath.end());
    std::wstring wArgs = L"--utilist-pipe=" + pipeName;
    std::wstring wWorkDir(it->second.descriptor.directoryPath.begin(), it->second.descriptor.directoryPath.end());

    it->second.processState = ProcessState::Starting;
    it->second.stateDescription = "Starting...";
    emit utilityStateChanged(utilityId);

    bool ok = m_processManager->startUtilityProcess(utilityId, wExePath, wArgs, wWorkDir);
    if (!ok) {
        it->second.processState = ProcessState::Error;
        it->second.lastError = "Executable not found or launch failed";
        it->second.stateDescription = "Launch Failed";
        emit utilityStateChanged(utilityId);
    }
    return ok;
}

bool UtilityManager::stopUtility(const std::string& utilityId) {
    auto it = m_utilities.find(utilityId);
    if (it == m_utilities.end()) return false;

    it->second.processState = ProcessState::Stopping;
    it->second.stateDescription = "Stopping...";
    emit utilityStateChanged(utilityId);

    if (m_ipcClients.count(utilityId)) {
        m_ipcClients[utilityId]->sendCommand("shutdown");
    }

    m_processManager->terminateGracefully(utilityId, 2000);
    return true;
}

bool UtilityManager::toggleUtilityFeature(const std::string& utilityId) {
    auto it = m_utilities.find(utilityId);
    if (it == m_utilities.end()) return false;

    if (it->second.processState != ProcessState::Running) {
        if (startUtility(utilityId)) {
            // After start, send activate
            return true;
        }
        return false;
    }

    if (m_ipcClients.count(utilityId)) {
        return m_ipcClients[utilityId]->sendCommand("toggle");
    }
    return false;
}

bool UtilityManager::sendUtilityCommand(const std::string& utilityId, const std::string& command, const std::string& payloadJson) {
    if (m_ipcClients.count(utilityId)) {
        return m_ipcClients[utilityId]->sendCommand(command, payloadJson);
    }
    return false;
}

void UtilityManager::setUtilityEnabled(const std::string& utilityId, bool enabled) {
    auto it = m_utilities.find(utilityId);
    if (it == m_utilities.end()) return;

    it->second.enabled = enabled;
    auto uCfg = ConfigurationManager::instance().getUtilityConfig(utilityId);
    uCfg.enabled = enabled;
    ConfigurationManager::instance().setUtilityConfig(utilityId, uCfg);

    if (!enabled) {
        // Disabling must shut down the running utility
        m_shortcutManager->unregisterShortcut(utilityId);
        stopUtility(utilityId);
    } else {
        if (!it->second.configuredShortcut.empty()) {
            m_shortcutManager->registerShortcut(utilityId, it->second.configuredShortcut, "toggle");
        }
        if (uCfg.autoStart) {
            startUtility(utilityId);
        }
    }

    emit utilityStateChanged(utilityId);
}

void UtilityManager::setUtilityShortcut(const std::string& utilityId, const std::string& shortcutText) {
    auto it = m_utilities.find(utilityId);
    if (it == m_utilities.end()) return;

    it->second.configuredShortcut = shortcutText;
    auto sCfg = ConfigurationManager::instance().getShortcutConfig(utilityId);
    sCfg.keySequence = shortcutText;
    ConfigurationManager::instance().setShortcutConfig(utilityId, sCfg);

    if (it->second.enabled) {
        m_shortcutManager->registerShortcut(utilityId, shortcutText, "toggle");
    }

    emit utilityStateChanged(utilityId);
}

void UtilityManager::handleShortcutTriggered(const std::string& utilityId, const std::string& action) {
    auto it = m_utilities.find(utilityId);
    if (it == m_utilities.end() || !it->second.enabled) return;

    if (action == "toggle") {
        toggleUtilityFeature(utilityId);
    } else if (action == "activate") {
        if (m_ipcClients.count(utilityId)) {
            m_ipcClients[utilityId]->sendCommand("activate");
        }
    } else if (action == "deactivate") {
        if (m_ipcClients.count(utilityId)) {
            m_ipcClients[utilityId]->sendCommand("deactivate");
        }
    } else if (action == "update_settings") {
        if (m_ipcClients.count(utilityId)) {
            m_ipcClients[utilityId]->sendCommand("update_settings");
        }
    }
}

void UtilityManager::shutdownAll() {
    for (auto& [id, util] : m_utilities) {
        if (util.processState == ProcessState::Running || util.processState == ProcessState::Starting) {
            if (m_ipcClients.count(id)) {
                m_ipcClients[id]->sendCommand("shutdown");
            }
        }
    }
    m_processManager->terminateAll();
    m_ipcClients.clear();
}

} // namespace utilist::app::core
