#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace utilist::models {

struct UtilityDescriptor {
    uint32_t schemaVersion{1};
    std::string id;
    std::string name;
    std::string description;
    std::string version;
    std::string executable;
    uint32_t protocolVersion{1};
    std::vector<std::string> capabilities;
    std::string defaultShortcut;
    std::string icon;
    std::string directoryPath;

    [[nodiscard]] bool hasCapability(const std::string& capability) const;
    [[nodiscard]] std::string getExecutableAbsolutePath() const;

    static std::optional<UtilityDescriptor> fromJson(const std::string& json, const std::string& directoryPath = "");
    static std::vector<std::string> validate(const UtilityDescriptor& descriptor);
};

} // namespace utilist::models
