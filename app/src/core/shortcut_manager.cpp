#include "shortcut_manager.hpp"
#include "logging_service.hpp"
#include <QCoreApplication>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace utilist::app::core {

ShortcutManager::ShortcutManager(QObject* parent)
    : QObject(parent) {
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
}

ShortcutManager::~ShortcutManager() {
    unregisterAll();
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

bool ShortcutManager::isConflict(const std::string& utilityId, uint32_t modifiers, uint32_t vk) const {
    for (const auto& [id, sc] : m_shortcutsByUtility) {
        if (id != utilityId && sc.active && sc.modifiers == modifiers && sc.virtualKey == vk) {
            return true;
        }
    }
    return false;
}

bool ShortcutManager::registerShortcut(const std::string& utilityId,
                                      const std::string& keySequence,
                                      const std::string& action) {
    unregisterShortcut(utilityId);

    if (keySequence.empty()) {
        return true;
    }

    auto parsed = platform::WindowsHotkeys::parseShortcut(keySequence);
    if (!parsed) {
        LoggingService::instance().warn("Failed to parse shortcut sequence: " + keySequence);
        RegisteredShortcut sc;
        sc.utilityId = utilityId;
        sc.keySequence = keySequence;
        sc.action = action;
        sc.active = false;
        sc.lastError = "Invalid shortcut format";
        m_shortcutsByUtility[utilityId] = sc;
        return false;
    }

    if (isConflict(utilityId, parsed->modifiers, parsed->virtualKey)) {
        LoggingService::instance().warn("Shortcut conflict detected for: " + keySequence);
        RegisteredShortcut sc;
        sc.utilityId = utilityId;
        sc.keySequence = parsed->canonicalString;
        sc.action = action;
        sc.active = false;
        sc.lastError = "Conflict with another registered shortcut";
        m_shortcutsByUtility[utilityId] = sc;
        return false;
    }

    int id = m_nextId++;
    bool ok = platform::WindowsHotkeys::registerHotkey(nullptr, id, parsed->modifiers, parsed->virtualKey);

    RegisteredShortcut sc;
    sc.hotkeyId = id;
    sc.utilityId = utilityId;
    sc.action = action;
    sc.keySequence = parsed->canonicalString;
    sc.modifiers = parsed->modifiers;
    sc.virtualKey = parsed->virtualKey;
    sc.active = ok;

    if (!ok) {
        DWORD err = GetLastError();
        sc.lastError = "OS rejected registration (error code: " + std::to_string(err) + "). Shortcut may be in use by another app.";
        LoggingService::instance().warn("RegisterHotKey failed for '" + sc.keySequence + "': " + sc.lastError);
    } else {
        sc.lastError.clear();
        m_utilityByHotkeyId[id] = utilityId;
        LoggingService::instance().info("Registered global hotkey '" + sc.keySequence + "' for utility: " + utilityId);
    }

    m_shortcutsByUtility[utilityId] = sc;
    return ok;
}

bool ShortcutManager::unregisterShortcut(const std::string& utilityId) {
    auto it = m_shortcutsByUtility.find(utilityId);
    if (it != m_shortcutsByUtility.end()) {
        if (it->second.active) {
            platform::WindowsHotkeys::unregisterHotkey(nullptr, it->second.hotkeyId);
            m_utilityByHotkeyId.erase(it->second.hotkeyId);
            LoggingService::instance().info("Unregistered hotkey for utility: " + utilityId);
        }
        m_shortcutsByUtility.erase(it);
        return true;
    }
    return false;
}

void ShortcutManager::unregisterAll() {
    for (auto& [id, sc] : m_shortcutsByUtility) {
        if (sc.active) {
            platform::WindowsHotkeys::unregisterHotkey(nullptr, sc.hotkeyId);
        }
    }
    m_shortcutsByUtility.clear();
    m_utilityByHotkeyId.clear();
}

std::optional<RegisteredShortcut> ShortcutManager::getShortcut(const std::string& utilityId) const {
    auto it = m_shortcutsByUtility.find(utilityId);
    if (it != m_shortcutsByUtility.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<RegisteredShortcut> ShortcutManager::getAllShortcuts() const {
    std::vector<RegisteredShortcut> list;
    for (const auto& [id, sc] : m_shortcutsByUtility) {
        list.push_back(sc);
    }
    return list;
}

bool ShortcutManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(result);
    if (eventType.startsWith("windows_")) {
        auto* msg = static_cast<MSG*>(message);
        if (msg && msg->message == WM_HOTKEY) {
            int id = static_cast<int>(msg->wParam);
            auto it = m_utilityByHotkeyId.find(id);
            if (it != m_utilityByHotkeyId.end()) {
                const std::string& utilId = it->second;
                auto scIt = m_shortcutsByUtility.find(utilId);
                std::string action = (scIt != m_shortcutsByUtility.end()) ? scIt->second.action : "toggle";
                LoggingService::instance().info("Hotkey triggered for: " + utilId + " (action: " + action + ")");
                emit shortcutTriggered(utilId, action);
                return true;
            }
        }
    }
    return false;
}

} // namespace utilist::app::core
