#pragma once

#include <string>
#include <cstdint>
#include <memory>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace utilist::app::platform {

class WindowsProcess {
public:
    WindowsProcess();
    ~WindowsProcess();

    WindowsProcess(const WindowsProcess&) = delete;
    WindowsProcess& operator=(const WindowsProcess&) = delete;

    WindowsProcess(WindowsProcess&& other) noexcept;
    WindowsProcess& operator=(WindowsProcess&& other) noexcept;

    bool launch(const std::wstring& executablePath,
                const std::wstring& arguments,
                const std::wstring& workingDirectory);

    bool isAlive() const;
    bool waitForExit(uint32_t timeoutMs = 3000);
    bool forceTerminate(uint32_t exitCode = 1);
    void close();

    [[nodiscard]] uint32_t getProcessId() const noexcept;
    [[nodiscard]] HANDLE getProcessHandle() const noexcept;

private:
    HANDLE m_processHandle{INVALID_HANDLE_VALUE};
    HANDLE m_threadHandle{INVALID_HANDLE_VALUE};
    HANDLE m_jobHandle{INVALID_HANDLE_VALUE};
    uint32_t m_processId{0};

    void initJobObject();
};

} // namespace utilist::app::platform
