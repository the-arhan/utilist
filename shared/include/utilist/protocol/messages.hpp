#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace utilist::protocol {

inline constexpr uint32_t CURRENT_PROTOCOL_VERSION = 1;
inline constexpr uint32_t MAX_MESSAGE_SIZE = 65536; // 64 KB

enum class MessageType {
    Unknown,
    Command,
    Query,
    Response,
    Handshake
};

struct ErrorInfo {
    std::string code;
    std::string message;
};

struct Request {
    uint32_t protocolVersion{CURRENT_PROTOCOL_VERSION};
    std::string requestId;
    std::string type; // "command", "query", "handshake"
    std::string command;
    std::string payloadJson{"{}"};
};

struct Response {
    uint32_t protocolVersion{CURRENT_PROTOCOL_VERSION};
    std::string requestId;
    std::string type{"response"};
    bool success{false};
    std::optional<ErrorInfo> error;
    std::string payloadJson{"{}"};
};

struct StatusPayload {
    bool featureActive{false};
    std::string stateDescription;
    std::vector<std::string> capabilities;
    uint64_t uptimeSeconds{0};
};

} // namespace utilist::protocol
