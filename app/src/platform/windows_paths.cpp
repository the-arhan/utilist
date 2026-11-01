#include "windows_paths.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

namespace utilist::app::platform {

namespace fs = std::filesystem;

fs::path WindowsPaths::getAppDataDir() {
    PWSTR localAppDataPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppDataPath))) {
        fs::path path(localAppDataPath);
        CoTaskMemFree(localAppDataPath);
        return path / "Utilist";
    }
    // Fallback to current working directory if Shell call fails
    return fs::current_path() / "AppData";
}

fs::path WindowsPaths::getConfigFilePath() {
    return getAppDataDir() / "config.json";
}

fs::path WindowsPaths::getLogsDir() {
    return getAppDataDir() / "logs";
}

fs::path WindowsPaths::getUtilitiesDir() {
    return getAppDataDir() / "utilities";
}

fs::path WindowsPaths::getExecutableDir() {
    wchar_t buffer[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        return fs::path(buffer).parent_path();
    }
    return fs::current_path();
}

bool WindowsPaths::ensureDirectoriesExist() {
    std::error_code ec;
    fs::create_directories(getAppDataDir(), ec);
    fs::create_directories(getLogsDir(), ec);
    fs::create_directories(getUtilitiesDir(), ec);
    return !ec;
}

} // namespace utilist::app::platform
