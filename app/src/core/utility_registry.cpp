#include "utility_registry.hpp"
#include "logging_service.hpp"
#include <fstream>
#include <sstream>

namespace utilist::app::core {

namespace fs = std::filesystem;

UtilityRegistry& UtilityRegistry::instance() {
    static UtilityRegistry s_instance;
    return s_instance;
}

void UtilityRegistry::addSearchPath(const fs::path& path) {
    if (std::find(m_searchPaths.begin(), m_searchPaths.end(), path) == m_searchPaths.end()) {
        m_searchPaths.push_back(path);
    }
}

void UtilityRegistry::rescan() {
    m_utilities.clear();
    for (const auto& path : m_searchPaths) {
        if (fs::exists(path) && fs::is_directory(path)) {
            scanDirectory(path);
        }
    }
    LoggingService::instance().info("Utility discovery completed. Discovered " +
                                     std::to_string(m_utilities.size()) + " utilities.");
}

void UtilityRegistry::scanDirectory(const fs::path& dir) {
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_directory()) {
            fs::path manifestPath = entry.path() / "manifest.json";
            fs::path manifestDir = entry.path();
            if (!fs::exists(manifestPath)) {
                if (fs::exists(entry.path() / "Debug" / "manifest.json")) {
                    manifestPath = entry.path() / "Debug" / "manifest.json";
                    manifestDir = entry.path() / "Debug";
                } else if (fs::exists(entry.path() / "Release" / "manifest.json")) {
                    manifestPath = entry.path() / "Release" / "manifest.json";
                    manifestDir = entry.path() / "Release";
                }
            }

            if (fs::exists(manifestPath)) {
                std::ifstream f(manifestPath);
                if (f.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>());
                    f.close();

                    auto descOpt = models::UtilityDescriptor::fromJson(content, manifestDir.string());
                    if (!descOpt) {
                        LoggingService::instance().warn("Failed to parse manifest at: " + manifestPath.string());
                        continue;
                    }

                    auto errors = models::UtilityDescriptor::validate(*descOpt);
                    if (!errors.empty()) {
                        std::string errList;
                        for (const auto& err : errors) {
                            if (!errList.empty()) errList += "; ";
                            errList += err;
                        }
                        LoggingService::instance().warn("Manifest validation failed for " + manifestPath.string() + ": " + errList);
                        continue;
                    }

                    // Check if executable exists
                    fs::path exePath = descOpt->getExecutableAbsolutePath();
                    if (!fs::exists(exePath)) {
                        LoggingService::instance().warn("Utility executable does not exist: " + exePath.string());
                    }

                    if (m_utilities.count(descOpt->id)) {
                        fs::path existingExe = m_utilities[descOpt->id].getExecutableAbsolutePath();
                        if (fs::exists(existingExe) && !fs::exists(exePath)) {
                            continue;
                        }
                    }

                    m_utilities[descOpt->id] = *descOpt;
                    LoggingService::instance().info("Registered utility: " + descOpt->id + " (" + descOpt->name + ")");
                }
            }
        }
    }
}

std::vector<models::UtilityDescriptor> UtilityRegistry::getAllUtilities() const {
    std::vector<models::UtilityDescriptor> list;
    list.reserve(m_utilities.size());
    for (const auto& [id, desc] : m_utilities) {
        list.push_back(desc);
    }
    return list;
}

std::optional<models::UtilityDescriptor> UtilityRegistry::getUtility(const std::string& utilityId) const {
    auto it = m_utilities.find(utilityId);
    if (it != m_utilities.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace utilist::app::core
