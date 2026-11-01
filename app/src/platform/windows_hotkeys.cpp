#include "windows_hotkeys.hpp"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

namespace utilist::app::platform {

namespace {

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string toUpper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

} // anonymous namespace

std::optional<ParsedHotkey> WindowsHotkeys::parseShortcut(const std::string& shortcutText) {
    if (shortcutText.empty()) {
        return std::nullopt;
    }

    std::stringstream ss(shortcutText);
    std::string token;
    uint32_t modifiers = MOD_NOREPEAT;
    uint32_t vk = 0;

    std::vector<std::string> parts;
    while (std::getline(ss, token, '+')) {
        token = trim(token);
        if (!token.empty()) {
            parts.push_back(token);
        }
    }

    if (parts.empty()) {
        return std::nullopt;
    }

    std::string canonical;

    for (size_t i = 0; i < parts.size(); ++i) {
        std::string partUpper = toUpper(parts[i]);
        if (i < parts.size() - 1 || parts.size() > 1 && (partUpper == "CTRL" || partUpper == "CONTROL" || partUpper == "SHIFT" || partUpper == "ALT" || partUpper == "WIN")) {
            if (partUpper == "CTRL" || partUpper == "CONTROL") {
                modifiers |= MOD_CONTROL;
                if (!canonical.empty()) canonical += "+";
                canonical += "Ctrl";
            } else if (partUpper == "SHIFT") {
                modifiers |= MOD_SHIFT;
                if (!canonical.empty()) canonical += "+";
                canonical += "Shift";
            } else if (partUpper == "ALT") {
                modifiers |= MOD_ALT;
                if (!canonical.empty()) canonical += "+";
                canonical += "Alt";
            } else if (partUpper == "WIN" || partUpper == "WINDOWS") {
                modifiers |= MOD_WIN;
                if (!canonical.empty()) canonical += "+";
                canonical += "Win";
            } else {
                return std::nullopt;
            }
        } else {
            // Main key
            if (partUpper.size() == 1 && partUpper[0] >= 'A' && partUpper[0] <= 'Z') {
                vk = static_cast<uint32_t>(partUpper[0]);
                if (!canonical.empty()) canonical += "+";
                canonical += partUpper;
            } else if (partUpper.size() == 1 && partUpper[0] >= '0' && partUpper[0] <= '9') {
                vk = static_cast<uint32_t>(partUpper[0]);
                if (!canonical.empty()) canonical += "+";
                canonical += partUpper;
            } else if (partUpper.rfind("F", 0) == 0 && partUpper.size() >= 2 && partUpper.size() <= 3) {
                try {
                    int fNum = std::stoi(partUpper.substr(1));
                    if (fNum >= 1 && fNum <= 24) {
                        vk = VK_F1 + (fNum - 1);
                        if (!canonical.empty()) canonical += "+";
                        canonical += "F" + std::to_string(fNum);
                    } else {
                        return std::nullopt;
                    }
                } catch (...) {
                    return std::nullopt;
                }
            } else if (partUpper == "SPACE") {
                vk = VK_SPACE;
                if (!canonical.empty()) canonical += "+";
                canonical += "Space";
            } else if (partUpper == "ENTER" || partUpper == "RETURN") {
                vk = VK_RETURN;
                if (!canonical.empty()) canonical += "+";
                canonical += "Enter";
            } else if (partUpper == "ESC" || partUpper == "ESCAPE") {
                vk = VK_ESCAPE;
                if (!canonical.empty()) canonical += "+";
                canonical += "Esc";
            } else {
                return std::nullopt;
            }
        }
    }

    if (vk == 0) {
        return std::nullopt;
    }

    ParsedHotkey res;
    res.modifiers = modifiers;
    res.virtualKey = vk;
    res.canonicalString = canonical;
    return res;
}

bool WindowsHotkeys::registerHotkey(HWND hwnd, int hotkeyId, uint32_t modifiers, uint32_t virtualKey) {
    return RegisterHotKey(hwnd, hotkeyId, modifiers, virtualKey) != FALSE;
}

bool WindowsHotkeys::unregisterHotkey(HWND hwnd, int hotkeyId) {
    return UnregisterHotKey(hwnd, hotkeyId) != FALSE;
}

} // namespace utilist::app::platform
