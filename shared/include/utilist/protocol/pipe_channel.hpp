#pragma once

#include "framing.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace utilist::protocol {

class PipeChannel {
public:
    PipeChannel();
    ~PipeChannel();

    PipeChannel(const PipeChannel&) = delete;
    PipeChannel& operator=(const PipeChannel&) = delete;

    PipeChannel(PipeChannel&& other) noexcept;
    PipeChannel& operator=(PipeChannel&& other) noexcept;

    // Server mode: creates pipe and waits for client connection
    bool createServer(const std::wstring& pipeName);
    bool waitForClientConnection(uint32_t timeoutMs = 5000);

    // Client mode: connects to existing named pipe
    bool connectToServer(const std::wstring& pipeName, uint32_t timeoutMs = 5000);

    // Send a JSON message (automatically framed)
    bool sendMessage(const std::string& jsonPayload);

    // Read available messages from the pipe. Returns all fully received messages.
    std::vector<std::string> pollMessages();

    // Check if pipe is currently connected and valid
    [[nodiscard]] bool isConnected() const noexcept;

    // Close the pipe and reset state
    void close();

private:
    HANDLE m_pipeHandle{INVALID_HANDLE_VALUE};
    bool m_isServer{false};
    bool m_connected{false};
    FrameBuffer m_frameBuffer;
};

} // namespace utilist::protocol
