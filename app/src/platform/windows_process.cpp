#include "windows_process.hpp"
#include <vector>

namespace utilist::app::platform {

WindowsProcess::WindowsProcess() {
    initJobObject();
}

WindowsProcess::~WindowsProcess() {
    close();
}

WindowsProcess::WindowsProcess(WindowsProcess&& other) noexcept
    : m_processHandle(other.m_processHandle),
      m_threadHandle(other.m_threadHandle),
      m_jobHandle(other.m_jobHandle),
      m_processId(other.m_processId) {
    other.m_processHandle = INVALID_HANDLE_VALUE;
    other.m_threadHandle = INVALID_HANDLE_VALUE;
    other.m_jobHandle = INVALID_HANDLE_VALUE;
    other.m_processId = 0;
}

WindowsProcess& WindowsProcess::operator=(WindowsProcess&& other) noexcept {
    if (this != &other) {
        close();
        m_processHandle = other.m_processHandle;
        m_threadHandle = other.m_threadHandle;
        m_jobHandle = other.m_jobHandle;
        m_processId = other.m_processId;

        other.m_processHandle = INVALID_HANDLE_VALUE;
        other.m_threadHandle = INVALID_HANDLE_VALUE;
        other.m_jobHandle = INVALID_HANDLE_VALUE;
        other.m_processId = 0;
    }
    return *this;
}

void WindowsProcess::initJobObject() {
    m_jobHandle = CreateJobObjectW(nullptr, nullptr);
    if (m_jobHandle != nullptr && m_jobHandle != INVALID_HANDLE_VALUE) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(
            m_jobHandle,
            JobObjectExtendedLimitInformation,
            &jeli,
            sizeof(jeli)
        );
    }
}

bool WindowsProcess::launch(const std::wstring& executablePath,
                            const std::wstring& arguments,
                            const std::wstring& workingDirectory) {
    close();
    initJobObject();

    std::wstring cmdLine = L"\"" + executablePath + L"\" " + arguments;
    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    // Pass CREATE_SUSPENDED so we can add to Job Object before running
    DWORD creationFlags = CREATE_SUSPENDED;

    BOOL created = CreateProcessW(
        executablePath.c_str(),
        cmdBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        creationFlags,
        nullptr,
        workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
        &si,
        &pi
    );

    if (!created) {
        return false;
    }

    m_processHandle = pi.hProcess;
    m_threadHandle = pi.hThread;
    m_processId = pi.dwProcessId;

    if (m_jobHandle != nullptr && m_jobHandle != INVALID_HANDLE_VALUE) {
        AssignProcessToJobObject(m_jobHandle, m_processHandle);
    }

    // Resume process execution
    ResumeThread(m_threadHandle);

    return true;
}

bool WindowsProcess::isAlive() const {
    if (m_processHandle == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD waitRes = WaitForSingleObject(m_processHandle, 0);
    return (waitRes == WAIT_TIMEOUT);
}

bool WindowsProcess::waitForExit(uint32_t timeoutMs) {
    if (m_processHandle == INVALID_HANDLE_VALUE) {
        return true;
    }
    DWORD waitRes = WaitForSingleObject(m_processHandle, timeoutMs);
    return (waitRes == WAIT_OBJECT_0);
}

bool WindowsProcess::forceTerminate(uint32_t exitCode) {
    if (m_processHandle == INVALID_HANDLE_VALUE) {
        return true;
    }
    return TerminateProcess(m_processHandle, exitCode) != FALSE;
}

void WindowsProcess::close() {
    if (m_threadHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_threadHandle);
        m_threadHandle = INVALID_HANDLE_VALUE;
    }
    if (m_processHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_processHandle);
        m_processHandle = INVALID_HANDLE_VALUE;
    }
    if (m_jobHandle != nullptr && m_jobHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_jobHandle);
        m_jobHandle = INVALID_HANDLE_VALUE;
    }
    m_processId = 0;
}

uint32_t WindowsProcess::getProcessId() const noexcept {
    return m_processId;
}

HANDLE WindowsProcess::getProcessHandle() const noexcept {
    return m_processHandle;
}

} // namespace utilist::app::platform
