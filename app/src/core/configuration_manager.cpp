#include "configuration_manager.hpp"
#include <utilist/protocol/json_helper.hpp>
#include <fstream>
#include <sstream>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace utilist::app::core {

namespace fs = std::filesystem;

ConfigurationManager& ConfigurationManager::instance() {
    static ConfigurationManager s_instance;
    return s_instance;
}

fs::path ConfigurationManager::getConfigFilePath() const {
    return m_configFilePath;
}

void ConfigurationManager::backupCorruptFile() {
    if (!fs::exists(m_configFilePath)) return;

    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    fs::path backup = m_configFilePath.string() + ".corrupt." + std::to_string(now);
    std::error_code ec;
    fs::rename(m_configFilePath, backup, ec);
}

bool ConfigurationManager::load(const fs::path& configFilePath) {
    m_configFilePath = configFilePath;

    if (!fs::exists(m_configFilePath)) {
        // Create initial default configuration file
        return save();
    }

    std::ifstream inFile(m_configFilePath);
    if (!inFile.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(inFile)),
                         std::istreambuf_iterator<char>());
    inFile.close();

    // Basic validation & extraction
    if (content.find("\"schema_version\"") == std::string::npos) {
        backupCorruptFile();
        save();
        return false;
    }

    // Parse app settings
    if (content.find("\"minimize_to_tray_on_close\": false") != std::string::npos) {
        m_appSettings.minimizeToTrayOnClose = false;
    } else {
        m_appSettings.minimizeToTrayOnClose = true;
    }

    if (content.find("\"start_with_windows\": true") != std::string::npos) {
        m_appSettings.startWithWindows = true;
    } else {
        m_appSettings.startWithWindows = false;
    }

    return true;
}

bool ConfigurationManager::save() {
    std::error_code ec;
    fs::create_directories(m_configFilePath.parent_path(), ec);

    std::ostringstream ss;
    ss << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"app\": {\n"
       << "    \"minimize_to_tray_on_close\": " << (m_appSettings.minimizeToTrayOnClose ? "true" : "false") << ",\n"
       << "    \"start_with_windows\": " << (m_appSettings.startWithWindows ? "true" : "false") << ",\n"
       << "    \"theme\": \"" << protocol::JsonHelper::escapeString(m_appSettings.theme) << "\",\n"
       << "    \"log_level\": \"" << protocol::JsonHelper::escapeString(m_appSettings.logLevel) << "\"\n"
       << "  },\n"
       << "  \"shortcuts\": {\n";

    size_t sIdx = 0;
    for (const auto& [id, sc] : m_shortcutConfigs) {
        if (sIdx > 0) ss << ",\n";
        ss << "    \"" << protocol::JsonHelper::escapeString(id) << "\": {\n"
           << "      \"key_sequence\": \"" << protocol::JsonHelper::escapeString(sc.keySequence) << "\",\n"
           << "      \"action\": \"" << protocol::JsonHelper::escapeString(sc.action) << "\",\n"
           << "      \"enabled\": " << (sc.enabled ? "true" : "false") << "\n"
           << "    }";
        sIdx++;
    }

    ss << "\n  },\n"
       << "  \"utilities\": {\n";

    size_t uIdx = 0;
    for (const auto& [id, uc] : m_utilityConfigs) {
        if (uIdx > 0) ss << ",\n";
        ss << "    \"" << protocol::JsonHelper::escapeString(id) << "\": {\n"
           << "      \"enabled\": " << (uc.enabled ? "true" : "false") << ",\n"
           << "      \"auto_start\": " << (uc.autoStart ? "true" : "false") << "\n"
           << "    }";
        uIdx++;
    }

    ss << "\n  }\n"
       << "}\n";

    std::string jsonStr = ss.str();

    // Atomic write via temp file
    fs::path tempFile = m_configFilePath.string() + ".tmp";
    std::ofstream outFile(tempFile, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        return false;
    }
    outFile << jsonStr;
    outFile.flush();
    outFile.close();

    // Atomically replace config file
    if (!MoveFileExW(tempFile.wstring().c_str(), m_configFilePath.wstring().c_str(), MOVEFILE_REPLACE_EXISTING)) {
        // Fallback standard rename
        fs::rename(tempFile, m_configFilePath, ec);
    }

    return true;
}

UtilityConfig ConfigurationManager::getUtilityConfig(const std::string& utilityId) const {
    auto it = m_utilityConfigs.find(utilityId);
    if (it != m_utilityConfigs.end()) {
        return it->second;
    }
    return UtilityConfig{true, true};
}

void ConfigurationManager::setUtilityConfig(const std::string& utilityId, const UtilityConfig& config) {
    m_utilityConfigs[utilityId] = config;
    save();
}

ShortcutConfig ConfigurationManager::getShortcutConfig(const std::string& utilityId) const {
    auto it = m_shortcutConfigs.find(utilityId);
    if (it != m_shortcutConfigs.end()) {
        return it->second;
    }
    return ShortcutConfig{};
}

void ConfigurationManager::setShortcutConfig(const std::string& utilityId, const ShortcutConfig& config) {
    m_shortcutConfigs[utilityId] = config;
    save();
}

} // namespace utilist::app::core
