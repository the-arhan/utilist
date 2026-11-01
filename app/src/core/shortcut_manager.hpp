#pragma once

#include <platform/windows_hotkeys.hpp>
#include <QObject>
#include <QAbstractNativeEventFilter>
#include <string>
#include <map>
#include <optional>

namespace utilist::app::core {

struct RegisteredShortcut {
    int hotkeyId{0};
    std::string utilityId;
    std::string action{"toggle"};
    std::string keySequence;
    uint32_t modifiers{0};
    uint32_t virtualKey{0};
    bool active{false};
    std::string lastError;
};

class ShortcutManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit ShortcutManager(QObject* parent = nullptr);
    ~ShortcutManager() override;

    bool registerShortcut(const std::string& utilityId,
                          const std::string& keySequence,
                          const std::string& action = "toggle");

    bool unregisterShortcut(const std::string& utilityId);
    void unregisterAll();

    [[nodiscard]] std::optional<RegisteredShortcut> getShortcut(const std::string& utilityId) const;
    [[nodiscard]] std::vector<RegisteredShortcut> getAllShortcuts() const;

    // Native event filter to capture WM_HOTKEY
    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void shortcutTriggered(const std::string& utilityId, const std::string& action);

private:
    int m_nextId{100};
    HWND m_nativeHwnd{nullptr};
    std::map<std::string, RegisteredShortcut> m_shortcutsByUtility;
    std::map<int, std::string> m_utilityByHotkeyId;

    bool isConflict(const std::string& utilityId, uint32_t modifiers, uint32_t vk) const;
};

} // namespace utilist::app::core
