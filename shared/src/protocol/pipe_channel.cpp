#include <utilist/protocol/pipe_channel.hpp>
#include <chrono>
#include <thread>

namespace utilist::protocol {

PipeChannel::PipeChannel() = default;

PipeChannel::~PipeChannel() {
    close();
}

PipeChannel::PipeChannel(PipeChannel&& other) noexcept
    : m_pipeHandle(other.m_pipeHandle),
      m_isServer(other.m_isServer),
      m_connected(other.m_connected),
      m_frameBuffer(std::move(other.m_frameBuffer)) {
    other.m_pipeHandle = INVALID_HANDLE_VALUE;
    other.m_isServer = false;
    other.m_connected = false;
}

PipeChannel& PipeChannel::operator=(PipeChannel&& other) noexcept {
    if (this != &other) {
        close();
        m_pipeHandle = other.m_pipeHandle;
        m_isServer = other.m_isServer;
        m_connected = other.m_connected;
        m_frameBuffer = std::move(other.m_frameBuffer);

        other.m_pipeHandle = INVALID_HANDLE_VALUE;
        other.m_isServer = false;
        other.m_connected = false;
    }
    return *this;
}

bool PipeChannel::createServer(const std::wstring& pipeName) {
    close();
    m_isServer = true;

    // Security: reject remote clients, duplex, byte mode
    DWORD openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
    DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS;
    DWORD maxInstances = 1;
    DWORD outBufferSize = 65536;
    DWORD inBufferSize = 65536;
    DWORD defaultTimeout = 5000;

    m_pipeHandle = CreateNamedPipeW(
        pipeName.c_str(),
        openMode,
        pipeMode,
        maxInstances,
        outBufferSize,
        inBufferSize,
        defaultTimeout,
        nullptr // default security descriptor: current user token
    );

    return m_pipeHandle != INVALID_HANDLE_VALUE;
}

bool PipeChannel::waitForClientConnection(uint32_t timeoutMs) {
    if (m_pipeHandle == INVALID_HANDLE_VALUE || !m_isServer) {
        return false;
    }

    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ov.hEvent) {
        return false;
    }

    BOOL connectResult = ConnectNamedPipe(m_pipeHandle, &ov);
    DWORD lastError = GetLastError();

    if (connectResult) {
        // Connected immediately
        CloseHandle(ov.hEvent);
        m_connected = true;
        return true;
    }

    if (lastError == ERROR_PIPE_CONNECTED) {
        // Client already connected between CreateNamedPipe and ConnectNamedPipe
        CloseHandle(ov.hEvent);
        m_connected = true;
        return true;
    }

    if (lastError == ERROR_IO_PENDING) {
        DWORD waitResult = WaitForSingleObject(ov.hEvent, timeoutMs);
        if (waitResult == WAIT_OBJECT_0) {
            DWORD bytesTransferred = 0;
            if (GetOverlappedResult(m_pipeHandle, &ov, &bytesTransferred, FALSE)) {
                CloseHandle(ov.hEvent);
                m_connected = true;
                return true;
            }
        }
        // Timeout or failed: cancel I/O
        CancelIo(m_pipeHandle);
    }

    CloseHandle(ov.hEvent);
    return false;
}

bool PipeChannel::connectToServer(const std::wstring& pipeName, uint32_t timeoutMs) {
    close();
    m_isServer = false;

    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        m_pipeHandle = CreateFileW(
            pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (m_pipeHandle != INVALID_HANDLE_VALUE) {
            m_connected = true;
            return true;
        }

        DWORD err = GetLastError();
        if (err != ERROR_PIPE_BUSY && err != ERROR_FILE_NOT_FOUND) {
            return false;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (static_cast<uint32_t>(elapsed) >= timeoutMs) {
            return false;
        }

        if (err == ERROR_PIPE_BUSY) {
            uint32_t remaining = timeoutMs - static_cast<uint32_t>(elapsed);
            if (!WaitNamedPipeW(pipeName.c_str(), remaining > 500 ? 500 : remaining)) {
                // wait failed or timed out
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

bool PipeChannel::sendMessage(const std::string& jsonPayload) {
    if (!m_connected || m_pipeHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::vector<uint8_t> frame = FrameBuffer::frameMessage(jsonPayload);
    if (frame.empty()) {
        return false;
    }

    DWORD bytesWritten = 0;
    BOOL success = WriteFile(
        m_pipeHandle,
        frame.data(),
        static_cast<DWORD>(frame.size()),
        &bytesWritten,
        nullptr
    );

    if (!success || bytesWritten != frame.size()) {
        m_connected = false;
        return false;
    }

    return true;
}

std::vector<std::string> PipeChannel::pollMessages() {
    std::vector<std::string> messages;
    if (!m_connected || m_pipeHandle == INVALID_HANDLE_VALUE) {
        return messages;
    }

    DWORD bytesAvailable = 0;
    BOOL peekOk = PeekNamedPipe(m_pipeHandle, nullptr, 0, nullptr, &bytesAvailable, nullptr);
    if (!peekOk) {
        DWORD err = GetLastError();
        if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
            m_connected = false;
        }
        return messages;
    }

    if (bytesAvailable > 0) {
        std::vector<uint8_t> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        BOOL readOk = ReadFile(m_pipeHandle, buffer.data(), bytesAvailable, &bytesRead, nullptr);
        if (readOk && bytesRead > 0) {
            m_frameBuffer.appendData(buffer.data(), bytesRead);
            while (auto msg = m_frameBuffer.extractNextMessage()) {
                messages.push_back(std::move(*msg));
            }
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                m_connected = false;
            }
        }
    }

    return messages;
}

bool PipeChannel::isConnected() const noexcept {
    return m_connected && m_pipeHandle != INVALID_HANDLE_VALUE;
}

void PipeChannel::close() {
    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        if (m_isServer && m_connected) {
            DisconnectNamedPipe(m_pipeHandle);
        }
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
    }
    m_connected = false;
    m_isServer = false;
    m_frameBuffer.clear();
}

} // namespace utilist::protocol
