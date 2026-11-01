#include "process_manager.hpp"
#include "logging_service.hpp"

namespace utilist::app::core {

ProcessManager::ProcessManager(QObject* parent)
    : QObject(parent),
      m_monitorTimer(new QTimer(this)) {
    connect(m_monitorTimer, &QTimer::timeout, this, &ProcessManager::monitorProcesses);
    m_monitorTimer->start(200); // 200ms health check interval
}

ProcessManager::~ProcessManager() {
    terminateAll();
}

bool ProcessManager::startUtilityProcess(const std::string& utilityId,
                                        const std::wstring& executablePath,
                                        const std::wstring& arguments,
                                        const std::wstring& workingDir) {
    if (isRunning(utilityId)) {
        LoggingService::instance().warn("Attempted to start utility '" + utilityId + "' which is already running.");
        return false;
    }

    auto proc = std::make_unique<platform::WindowsProcess>();
    if (!proc->launch(executablePath, arguments, workingDir)) {
        LoggingService::instance().error("Failed to launch utility '" + utilityId + "' executable: " +
                                         std::string(executablePath.begin(), executablePath.end()));
        return false;
    }

    uint32_t pid = proc->getProcessId();
    m_processes[utilityId] = std::move(proc);

    LoggingService::instance().info("Launched utility process '" + utilityId + "' [PID: " + std::to_string(pid) + "]");
    emit processStarted(utilityId, pid);
    return true;
}

bool ProcessManager::isRunning(const std::string& utilityId) const {
    auto it = m_processes.find(utilityId);
    if (it != m_processes.end() && it->second) {
        return it->second->isAlive();
    }
    return false;
}

uint32_t ProcessManager::getProcessId(const std::string& utilityId) const {
    auto it = m_processes.find(utilityId);
    if (it != m_processes.end() && it->second) {
        return it->second->getProcessId();
    }
    return 0;
}

void ProcessManager::terminateGracefully(const std::string& utilityId, uint32_t timeoutMs) {
    auto it = m_processes.find(utilityId);
    if (it == m_processes.end() || !it->second) return;

    if (it->second->isAlive()) {
        LoggingService::instance().info("Waiting for utility '" + utilityId + "' process to exit...");
        if (!it->second->waitForExit(timeoutMs)) {
            LoggingService::instance().warn("Utility '" + utilityId + "' did not exit gracefully within timeout. Terminating.");
            it->second->forceTerminate();
        }
    }

    it->second->close();
    m_processes.erase(it);
    emit processExited(utilityId, 0);
}

void ProcessManager::terminateAll() {
    m_monitorTimer->stop();
    for (auto& [id, proc] : m_processes) {
        if (proc && proc->isAlive()) {
            LoggingService::instance().info("Terminating process for utility '" + id + "' during shutdown.");
            if (!proc->waitForExit(1500)) {
                proc->forceTerminate();
            }
            proc->close();
        }
    }
    m_processes.clear();
}

void ProcessManager::monitorProcesses() {
    std::vector<std::string> deadProcesses;

    for (const auto& [id, proc] : m_processes) {
        if (proc && !proc->isAlive()) {
            deadProcesses.push_back(id);
        }
    }

    for (const auto& id : deadProcesses) {
        LoggingService::instance().warn("Detected process exit for utility: " + id);
        m_processes[id]->close();
        m_processes.erase(id);
        emit processExited(id, -1);
    }
}

} // namespace utilist::app::core
