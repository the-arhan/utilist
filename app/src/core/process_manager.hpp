#pragma once

#include <platform/windows_process.hpp>
#include <QObject>
#include <QTimer>
#include <map>
#include <memory>
#include <string>

namespace utilist::app::core {

class ProcessManager : public QObject {
    Q_OBJECT

public:
    explicit ProcessManager(QObject* parent = nullptr);
    ~ProcessManager() override;

    bool startUtilityProcess(const std::string& utilityId,
                             const std::wstring& executablePath,
                             const std::wstring& arguments,
                             const std::wstring& workingDir);

    bool isRunning(const std::string& utilityId) const;
    uint32_t getProcessId(const std::string& utilityId) const;

    void terminateGracefully(const std::string& utilityId, uint32_t timeoutMs = 3000);
    void terminateAll();

signals:
    void processStarted(const std::string& utilityId, uint32_t pid);
    void processExited(const std::string& utilityId, int exitCode);

private slots:
    void monitorProcesses();

private:
    std::map<std::string, std::unique_ptr<platform::WindowsProcess>> m_processes;
    QTimer* m_monitorTimer{nullptr};
};

} // namespace utilist::app::core
