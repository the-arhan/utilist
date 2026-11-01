#pragma once

#include <filesystem>
#include <string>

namespace utilist::app::platform {

class WindowsPaths {
public:
    static std::filesystem::path getAppDataDir();
    static std::filesystem::path getConfigFilePath();
    static std::filesystem::path getLogsDir();
    static std::filesystem::path getUtilitiesDir();
    static std::filesystem::path getExecutableDir();
    static bool ensureDirectoriesExist();
};

} // namespace utilist::app::platform
