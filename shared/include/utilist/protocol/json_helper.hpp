#pragma once

#include "messages.hpp"
#include <string>
#include <optional>

namespace utilist::protocol {

class JsonHelper {
public:
    static std::string serializeRequest(const Request& req);
    static std::optional<Request> parseRequest(const std::string& json);

    static std::string serializeResponse(const Response& res);
    static std::optional<Response> parseResponse(const std::string& json);

    static std::string serializeStatusPayload(const StatusPayload& payload);
    static std::optional<StatusPayload> parseStatusPayload(const std::string& json);

    // Helpers
    static std::string escapeString(const std::string& str);
    static std::string unescapeString(const std::string& str);
};

} // namespace utilist::protocol
