#pragma once

#include <models/utility_state.hpp>
#include <core/utility_registry.hpp>
#include <core/process_manager.hpp>
#include <core/configuration_manager.hpp>
#include <core/shortcut_manager.hpp>
#include <core/ipc_client.hpp>
#include <QObject>
#include <map>
#include <memory>
#include <string>

namespace utilist::app::core {

class UtilityManager : public QObject {
    Q_OBJECT

public:
    explicit UtilityManager(ProcessManager* processManager,
                            ShortcutManager* shortcutManager,
                            QObject* parent = nullptr);
    ~UtilityManager() override;

    void initialize();
    void refreshUtilities();

    [[nodiscard]] std::vector<ManagedUtility> getManagedUtilities() const;
    [[nodiscard]] std::optional<ManagedUtility> getManagedUtility(const std::string& utilityId) const;

    bool startUtility(const std::string& utilityId);
    bool stopUtility(const std::string& utilityId);
    bool toggleUtilityFeature(const std::string& utilityId);
    bool sendUtilityCommand(const std::string& utilityId, const std::string& command, const std::string& payloadJson = "{}");

    void setUtilityEnabled(const std::string& utilityId, bool enabled);
    void setUtilityShortcut(const std::string& utilityId, const std::string& shortcutText);

    void handleShortcutTriggered(const std::string& utilityId, const std::string& action);
    void shutdownAll();

signals:
    void utilityStateChanged(const std::string& utilityId);
    void utilitiesRefreshed();

private:
    ProcessManager* m_processManager{nullptr};
    ShortcutManager* m_shortcutManager{nullptr};
    std::map<std::string, ManagedUtility> m_utilities;
    std::map<std::string, std::unique_ptr<IpcClient>> m_ipcClients;

    void setupIpcForUtility(const std::string& utilityId, const std::wstring& pipeName);
    std::wstring generatePipeName(const std::string& utilityId);
};

} // namespace utilist::app::core
