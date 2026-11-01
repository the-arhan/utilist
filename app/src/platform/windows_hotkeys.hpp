#pragma once

#include <string>
#include <optional>
#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace utilist::app::platform {

struct ParsedHotkey {
    uint32_t modifiers{0};
    uint32_t virtualKey{0};
    std::string canonicalString;
};

class WindowsHotkeys {
public:
    static std::optional<ParsedHotkey> parseShortcut(const std::string& shortcutText);
    static bool registerHotkey(HWND hwnd, int hotkeyId, uint32_t modifiers, uint32_t virtualKey);
    static bool unregisterHotkey(HWND hwnd, int hotkeyId);
};

} // namespace utilist::app::platform
