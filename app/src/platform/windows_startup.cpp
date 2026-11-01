#include "windows_startup.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace utilist::app::platform {

namespace {
const wchar_t* RUN_KEY_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* APP_KEY_NAME = L"UtilistDesktop";
}

bool WindowsStartup::isStartupEnabled() {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    LONG result = RegQueryValueExW(hKey, APP_KEY_NAME, nullptr, &type, nullptr, &size);
    RegCloseKey(hKey);

    return (result == ERROR_SUCCESS);
}

bool WindowsStartup::setStartupEnabled(bool enable, const std::wstring& applicationPath) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enable) {
        std::wstring exePath = applicationPath;
        if (exePath.empty()) {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(nullptr, buffer, MAX_PATH);
            exePath = buffer;
        }

        std::wstring cmd = L"\"" + exePath + L"\" --minimized";
        result = RegSetValueExW(
            hKey,
            APP_KEY_NAME,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(cmd.c_str()),
            static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t))
        );
    } else {
        result = RegDeleteValueW(hKey, APP_KEY_NAME);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS);
}

} // namespace utilist::app::platform
