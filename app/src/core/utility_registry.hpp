#pragma once

#include <utilist/models/utility_descriptor.hpp>
#include <vector>
#include <map>
#include <filesystem>
#include <string>

namespace utilist::app::core {

class UtilityRegistry {
public:
    static UtilityRegistry& instance();

    void addSearchPath(const std::filesystem::path& path);
    void rescan();

    [[nodiscard]] std::vector<models::UtilityDescriptor> getAllUtilities() const;
    [[nodiscard]] std::optional<models::UtilityDescriptor> getUtility(const std::string& utilityId) const;

private:
    UtilityRegistry() = default;

    std::vector<std::filesystem::path> m_searchPaths;
    std::map<std::string, models::UtilityDescriptor> m_utilities;

    void scanDirectory(const std::filesystem::path& dir);
};

} // namespace utilist::app::core
