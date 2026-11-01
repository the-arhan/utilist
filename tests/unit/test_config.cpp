#include <core/configuration_manager.hpp>
#include <platform/windows_hotkeys.hpp>
#include <iostream>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

void test_hotkey_parsing() {
    using namespace utilist::app::platform;
    std::cout << "[TEST] Running test_hotkey_parsing..." << std::endl;

    auto h1 = WindowsHotkeys::parseShortcut("Ctrl+Shift+H");
    assert(h1.has_value());
    assert(h1->virtualKey == 'H');
    assert((h1->modifiers & MOD_CONTROL) != 0);
    assert((h1->modifiers & MOD_SHIFT) != 0);

    auto h2 = WindowsHotkeys::parseShortcut("Alt+F12");
    assert(h2.has_value());
    assert(h2->virtualKey == VK_F12);
    assert((h2->modifiers & MOD_ALT) != 0);

    auto hInvalid = WindowsHotkeys::parseShortcut("Invalid+Key+Combination");
    assert(!hInvalid.has_value());

    std::cout << "[PASS] test_hotkey_parsing passed." << std::endl;
}

void test_configuration_lifecycle() {
    using namespace utilist::app::core;
    std::cout << "[TEST] Running test_configuration_lifecycle..." << std::endl;

    fs::path tempConfig = fs::temp_directory_path() / "utilist_test_config.json";
    if (fs::exists(tempConfig)) {
        fs::remove(tempConfig);
    }

    auto& mgr = ConfigurationManager::instance();
    bool loaded = mgr.load(tempConfig);
    assert(loaded);
    assert(mgr.appSettings().minimizeToTrayOnClose == true);

    // Modify and save
    mgr.appSettings().minimizeToTrayOnClose = false;
    UtilityConfig uCfg;
    uCfg.enabled = true;
    uCfg.autoStart = false;
    mgr.setUtilityConfig("test.util", uCfg);

    // Reload and verify
    assert(mgr.getUtilityConfig("test.util").autoStart == false);

    // Cleanup
    if (fs::exists(tempConfig)) {
        fs::remove(tempConfig);
    }
    fs::path tmp = tempConfig.string() + ".tmp";
    if (fs::exists(tmp)) {
        fs::remove(tmp);
    }

    std::cout << "[PASS] test_configuration_lifecycle passed." << std::endl;
}

int main() {
    std::cout << "Starting Configuration & Hotkey Unit Tests..." << std::endl;
    test_hotkey_parsing();
    test_configuration_lifecycle();
    std::cout << "All Configuration & Hotkey Unit Tests PASSED successfully." << std::endl;
    return 0;
}
