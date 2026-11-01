#pragma once

#include <string>
#include <map>
#include <filesystem>

namespace utilist::app::core {

struct AppSettings {
    bool minimizeToTrayOnClose{true};
    bool startWithWindows{false};
    std::string theme{"dark"};
    std::string logLevel{"INFO"};
};

struct UtilityConfig {
    bool enabled{true};
    bool autoStart{true};
};

struct ShortcutConfig {
    std::string keySequence;
    std::string action{"toggle"};
    bool enabled{true};
};

class ConfigurationManager {
public:
    static ConfigurationManager& instance();

    bool load(const std::filesystem::path& configFilePath);
    bool save();

    AppSettings& appSettings() noexcept { return m_appSettings; }
    const AppSettings& appSettings() const noexcept { return m_appSettings; }

    UtilityConfig getUtilityConfig(const std::string& utilityId) const;
    void setUtilityConfig(const std::string& utilityId, const UtilityConfig& config);

    ShortcutConfig getShortcutConfig(const std::string& utilityId) const;
    void setShortcutConfig(const std::string& utilityId, const ShortcutConfig& config);

    [[nodiscard]] std::filesystem::path getConfigFilePath() const;

private:
    ConfigurationManager() = default;

    std::filesystem::path m_configFilePath;
    AppSettings m_appSettings;
    std::map<std::string, UtilityConfig> m_utilityConfigs;
    std::map<std::string, ShortcutConfig> m_shortcutConfigs;

    void backupCorruptFile();
};

} // namespace utilist::app::core
