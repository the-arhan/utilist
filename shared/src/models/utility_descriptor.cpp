#include <utilist/models/utility_descriptor.hpp>
#include <utilist/protocol/json_helper.hpp>
#include <filesystem>
#include <regex>
#include <algorithm>

namespace utilist::models {

namespace fs = std::filesystem;

bool UtilityDescriptor::hasCapability(const std::string& capability) const {
    return std::find(capabilities.begin(), capabilities.end(), capability) != capabilities.end();
}

std::string UtilityDescriptor::getExecutableAbsolutePath() const {
    if (directoryPath.empty()) {
        return executable;
    }
    fs::path dir(directoryPath);
    auto p1 = (dir / executable).lexically_normal();
    if (fs::exists(p1)) return p1.string();
    auto pDebug = (dir / "Debug" / executable).lexically_normal();
    if (fs::exists(pDebug)) return pDebug.string();
    auto pRelease = (dir / "Release" / executable).lexically_normal();
    if (fs::exists(pRelease)) return pRelease.string();
    auto pSrc = (dir / "src" / executable).lexically_normal();
    if (fs::exists(pSrc)) return pSrc.string();

    // Check workspace CMake build directories for developer builds
    try {
        fs::path root = dir.parent_path().parent_path();
        if (dir.parent_path().filename() == "examples") {
            root = dir.parent_path().parent_path().parent_path();
        }
        auto pBuildDebug = (root / "build" / "windows-msvc-debug" / "utilities" / dir.filename() / "Debug" / executable).lexically_normal();
        if (fs::exists(pBuildDebug)) return pBuildDebug.string();
        auto pBuildRel = (root / "build" / "windows-msvc-debug" / "utilities" / dir.filename() / "Release" / executable).lexically_normal();
        if (fs::exists(pBuildRel)) return pBuildRel.string();
        auto pBuildExDebug = (root / "build" / "windows-msvc-debug" / "utilities" / "examples" / dir.filename() / "Debug" / executable).lexically_normal();
        if (fs::exists(pBuildExDebug)) return pBuildExDebug.string();
        auto pBuildExRel = (root / "build" / "windows-msvc-debug" / "utilities" / "examples" / dir.filename() / "Release" / executable).lexically_normal();
        if (fs::exists(pBuildExRel)) return pBuildExRel.string();
    } catch (...) {}

    return p1.string();
}

std::optional<UtilityDescriptor> UtilityDescriptor::fromJson(const std::string& json, const std::string& directoryPath) {
    UtilityDescriptor desc;
    desc.directoryPath = directoryPath;

    // Use protocol JSON helper to parse status / object members
    // We can parse top-level fields
    std::string unescaped = json;
    // Basic member extraction
    auto parseField = [&](const std::string& key) -> std::optional<std::string> {
        std::string pattern = "\"" + key + "\"";
        size_t keyPos = unescaped.find(pattern);
        if (keyPos == std::string::npos) return std::nullopt;
        size_t colonPos = unescaped.find(':', keyPos + pattern.size());
        if (colonPos == std::string::npos) return std::nullopt;
        size_t start = colonPos + 1;
        while (start < unescaped.size() && (unescaped[start] == ' ' || unescaped[start] == '\t' || unescaped[start] == '\r' || unescaped[start] == '\n')) {
            start++;
        }
        if (start >= unescaped.size()) return std::nullopt;

        if (unescaped[start] == '"') {
            size_t end = start + 1;
            while (end < unescaped.size()) {
                if (unescaped[end] == '"' && unescaped[end - 1] != '\\') {
                    break;
                }
                end++;
            }
            if (end < unescaped.size()) {
                return protocol::JsonHelper::unescapeString(unescaped.substr(start + 1, end - start - 1));
            }
        } else {
            size_t end = start;
            while (end < unescaped.size() && unescaped[end] != ',' && unescaped[end] != '}' && unescaped[end] != '\r' && unescaped[end] != '\n') {
                end++;
            }
            return unescaped.substr(start, end - start);
        }
        return std::nullopt;
    };

    if (auto sVer = parseField("schema_version")) {
        try { desc.schemaVersion = static_cast<uint32_t>(std::stoul(*sVer)); } catch (...) {}
    }
    if (auto pVer = parseField("protocol_version")) {
        try { desc.protocolVersion = static_cast<uint32_t>(std::stoul(*pVer)); } catch (...) {}
    }
    if (auto val = parseField("id")) desc.id = *val;
    if (auto val = parseField("name")) desc.name = *val;
    if (auto val = parseField("description")) desc.description = *val;
    if (auto val = parseField("version")) desc.version = *val;
    if (auto val = parseField("executable")) desc.executable = *val;
    if (auto val = parseField("default_shortcut")) desc.defaultShortcut = *val;
    if (auto val = parseField("icon")) desc.icon = *val;

    // Parse capabilities array
    size_t capPos = unescaped.find("\"capabilities\"");
    if (capPos != std::string::npos) {
        size_t arrayStart = unescaped.find('[', capPos);
        size_t arrayEnd = unescaped.find(']', arrayStart);
        if (arrayStart != std::string::npos && arrayEnd != std::string::npos) {
            std::string arr = unescaped.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
            size_t pos = 0;
            while (pos < arr.size()) {
                size_t q1 = arr.find('"', pos);
                if (q1 == std::string::npos) break;
                size_t q2 = arr.find('"', q1 + 1);
                if (q2 == std::string::npos) break;
                desc.capabilities.push_back(arr.substr(q1 + 1, q2 - q1 - 1));
                pos = q2 + 1;
            }
        }
    }

    if (desc.id.empty() || desc.name.empty() || desc.executable.empty()) {
        return std::nullopt;
    }

    return desc;
}

std::vector<std::string> UtilityDescriptor::validate(const UtilityDescriptor& descriptor) {
    std::vector<std::string> errors;

    if (descriptor.schemaVersion != 1) {
        errors.push_back("Unsupported schema version: " + std::to_string(descriptor.schemaVersion));
    }

    if (descriptor.protocolVersion != 1) {
        errors.push_back("Unsupported protocol version: " + std::to_string(descriptor.protocolVersion));
    }

    // ID format check: e.g. "vendor.name"
    static const std::regex idRegex("^[a-zA-Z0-9_-]+(\\.[a-zA-Z0-9_-]+)+$");
    if (!std::regex_match(descriptor.id, idRegex)) {
        errors.push_back("Invalid utility ID format: '" + descriptor.id + "'. Must be dot-separated identifier.");
    }

    if (descriptor.name.empty() || descriptor.name.size() > 64) {
        errors.push_back("Utility name must be between 1 and 64 characters.");
    }

    if (descriptor.executable.empty()) {
        errors.push_back("Executable name cannot be empty.");
    } else {
        fs::path p(descriptor.executable);
        if (p.extension() != ".exe") {
            errors.push_back("Executable must have a .exe extension.");
        }
        if (p.is_absolute() || descriptor.executable.find("..") != std::string::npos) {
            errors.push_back("Executable path must be relative and cannot contain path traversal ('..').");
        }
    }

    static const std::vector<std::string> validCapabilities = {
        "status", "activate", "deactivate", "toggle", "pause", "resume", "settings"
    };

    for (const auto& cap : descriptor.capabilities) {
        if (std::find(validCapabilities.begin(), validCapabilities.end(), cap) == validCapabilities.end()) {
            errors.push_back("Unknown capability: " + cap);
        }
    }

    return errors;
}

} // namespace utilist::models
