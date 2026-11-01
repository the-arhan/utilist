#pragma once

#include <string>

namespace utilist::app::platform {

class WindowsStartup {
public:
    static bool isStartupEnabled();
    static bool setStartupEnabled(bool enable, const std::wstring& applicationPath = L"");
};

} // namespace utilist::app::platform
