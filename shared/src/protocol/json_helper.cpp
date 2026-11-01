#include <utilist/protocol/json_helper.hpp>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <functional>

namespace utilist::protocol {

namespace {

void skipWhitespace(const std::string& s, size_t& pos) {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
}

std::optional<std::string> parseJsonString(const std::string& s, size_t& pos) {
    skipWhitespace(s, pos);
    if (pos >= s.size() || s[pos] != '"') {
        return std::nullopt;
    }
    ++pos; // skip leading '"'
    std::string result;
    while (pos < s.size()) {
        char c = s[pos++];
        if (c == '"') {
            return result;
        }
        if (c == '\\') {
            if (pos >= s.size()) return std::nullopt;
            char esc = s[pos++];
            switch (esc) {
                case '"': result.push_back('"'); break;
                case '\\': result.push_back('\\'); break;
                case '/': result.push_back('/'); break;
                case 'b': result.push_back('\b'); break;
                case 'f': result.push_back('\f'); break;
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                default: result.push_back(esc); break;
            }
        } else {
            result.push_back(c);
        }
    }
    return std::nullopt;
}

// Simple key-value extractor from a top-level JSON object
bool extractObjectMembers(const std::string& json,
                          const std::function<void(const std::string& key, const std::string& rawValue)>& callback) {
    size_t pos = 0;
    skipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '{') return false;
    ++pos; // skip '{'

    while (pos < json.size()) {
        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == '}') {
            return true;
        }

        auto keyOpt = parseJsonString(json, pos);
        if (!keyOpt) return false;

        skipWhitespace(json, pos);
        if (pos >= json.size() || json[pos] != ':') return false;
        ++pos; // skip ':'

        skipWhitespace(json, pos);
        if (pos >= json.size()) return false;

        size_t valStart = pos;
        if (json[pos] == '"') {
            auto s = parseJsonString(json, pos);
            if (!s) return false;
            callback(*keyOpt, json.substr(valStart, pos - valStart));
        } else if (json[pos] == '{' || json[pos] == '[') {
            char openCh = json[pos];
            char closeCh = (openCh == '{') ? '}' : ']';
            int depth = 0;
            while (pos < json.size()) {
                if (json[pos] == '"') {
                    parseJsonString(json, pos);
                    continue;
                }
                if (json[pos] == openCh) depth++;
                else if (json[pos] == closeCh) {
                    depth--;
                    if (depth == 0) {
                        ++pos;
                        break;
                    }
                }
                ++pos;
            }
            if (depth != 0) return false;
            callback(*keyOpt, json.substr(valStart, pos - valStart));
        } else {
            // Number, boolean, null
            while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && !std::isspace(static_cast<unsigned char>(json[pos]))) {
                ++pos;
            }
            callback(*keyOpt, json.substr(valStart, pos - valStart));
        }

        skipWhitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
        } else if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return true;
        }
    }
    return false;
}

std::string cleanScalar(const std::string& raw) {
    std::string s = raw;
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        size_t p = 0;
        auto val = parseJsonString(s, p);
        if (val) return *val;
    }
    return s;
}

} // anonymous namespace

std::string JsonHelper::escapeString(const std::string& str) {
    std::string out;
    out.reserve(str.size() + 8);
    for (char c : str) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

std::string JsonHelper::unescapeString(const std::string& str) {
    std::string out;
    size_t i = 0;
    while (i < str.size()) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            char next = str[i + 1];
            switch (next) {
                case '"': out.push_back('"'); i += 2; break;
                case '\\': out.push_back('\\'); i += 2; break;
                case 'n': out.push_back('\n'); i += 2; break;
                case 'r': out.push_back('\r'); i += 2; break;
                case 't': out.push_back('\t'); i += 2; break;
                case 'b': out.push_back('\b'); i += 2; break;
                case 'f': out.push_back('\f'); i += 2; break;
                default: out.push_back(next); i += 2; break;
            }
        } else {
            out.push_back(str[i++]);
        }
    }
    return out;
}

std::string JsonHelper::serializeRequest(const Request& req) {
    std::ostringstream ss;
    ss << "{"
       << "\"protocol_version\":" << req.protocolVersion << ","
       << "\"request_id\":\"" << escapeString(req.requestId) << "\","
       << "\"type\":\"" << escapeString(req.type) << "\","
       << "\"command\":\"" << escapeString(req.command) << "\","
       << "\"payload\":" << (req.payloadJson.empty() ? "{}" : req.payloadJson)
       << "}";
    return ss.str();
}

std::optional<Request> JsonHelper::parseRequest(const std::string& json) {
    Request req;
    bool hasId = false;
    bool hasType = false;
    bool hasCommand = false;

    bool ok = extractObjectMembers(json, [&](const std::string& key, const std::string& raw) {
        if (key == "protocol_version") {
            try {
                req.protocolVersion = static_cast<uint32_t>(std::stoul(cleanScalar(raw)));
            } catch (...) {}
        } else if (key == "request_id") {
            req.requestId = cleanScalar(raw);
            hasId = true;
        } else if (key == "type") {
            req.type = cleanScalar(raw);
            hasType = true;
        } else if (key == "command") {
            req.command = cleanScalar(raw);
            hasCommand = true;
        } else if (key == "payload") {
            req.payloadJson = raw;
        }
    });

    if (!ok || !hasId || !hasType) {
        return std::nullopt;
    }
    return req;
}

std::string JsonHelper::serializeResponse(const Response& res) {
    std::ostringstream ss;
    ss << "{"
       << "\"protocol_version\":" << res.protocolVersion << ","
       << "\"request_id\":\"" << escapeString(res.requestId) << "\","
       << "\"type\":\"" << escapeString(res.type) << "\","
       << "\"success\":" << (res.success ? "true" : "false") << ",";

    if (res.error) {
        ss << "\"error\":{"
           << "\"code\":\"" << escapeString(res.error->code) << "\","
           << "\"message\":\"" << escapeString(res.error->message) << "\""
           << "},";
    } else {
        ss << "\"error\":null,";
    }

    ss << "\"payload\":" << (res.payloadJson.empty() ? "{}" : res.payloadJson)
       << "}";
    return ss.str();
}

std::optional<Response> JsonHelper::parseResponse(const std::string& json) {
    Response res;
    bool hasId = false;
    bool hasSuccess = false;

    bool ok = extractObjectMembers(json, [&](const std::string& key, const std::string& raw) {
        if (key == "protocol_version") {
            try {
                res.protocolVersion = static_cast<uint32_t>(std::stoul(cleanScalar(raw)));
            } catch (...) {}
        } else if (key == "request_id") {
            res.requestId = cleanScalar(raw);
            hasId = true;
        } else if (key == "type") {
            res.type = cleanScalar(raw);
        } else if (key == "success") {
            std::string s = cleanScalar(raw);
            res.success = (s == "true");
            hasSuccess = true;
        } else if (key == "error") {
            std::string s = cleanScalar(raw);
            if (s != "null" && s != "{}") {
                ErrorInfo err;
                extractObjectMembers(raw, [&](const std::string& eKey, const std::string& eVal) {
                    if (eKey == "code") err.code = cleanScalar(eVal);
                    else if (eKey == "message") err.message = cleanScalar(eVal);
                });
                res.error = err;
            }
        } else if (key == "payload") {
            res.payloadJson = raw;
        }
    });

    if (!ok || !hasId || !hasSuccess) {
        return std::nullopt;
    }
    return res;
}

std::string JsonHelper::serializeStatusPayload(const StatusPayload& payload) {
    std::ostringstream ss;
    ss << "{"
       << "\"feature_active\":" << (payload.featureActive ? "true" : "false") << ","
       << "\"state_description\":\"" << escapeString(payload.stateDescription) << "\","
       << "\"uptime_seconds\":" << payload.uptimeSeconds << ","
       << "\"capabilities\":[";
    for (size_t i = 0; i < payload.capabilities.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\"" << escapeString(payload.capabilities[i]) << "\"";
    }
    ss << "]}";
    return ss.str();
}

std::optional<StatusPayload> JsonHelper::parseStatusPayload(const std::string& json) {
    StatusPayload payload;
    bool ok = extractObjectMembers(json, [&](const std::string& key, const std::string& raw) {
        if (key == "feature_active") {
            payload.featureActive = (cleanScalar(raw) == "true");
        } else if (key == "state_description") {
            payload.stateDescription = cleanScalar(raw);
        } else if (key == "uptime_seconds") {
            try {
                payload.uptimeSeconds = std::stoull(cleanScalar(raw));
            } catch (...) {}
        } else if (key == "capabilities") {
            // Parse string array
            size_t pos = 0;
            skipWhitespace(raw, pos);
            if (pos < raw.size() && raw[pos] == '[') {
                ++pos;
                while (pos < raw.size()) {
                    skipWhitespace(raw, pos);
                    if (pos < raw.size() && raw[pos] == ']') break;
                    auto strVal = parseJsonString(raw, pos);
                    if (strVal) {
                        payload.capabilities.push_back(*strVal);
                    }
                    skipWhitespace(raw, pos);
                    if (pos < raw.size() && raw[pos] == ',') ++pos;
                }
            }
        }
    });

    if (!ok) return std::nullopt;
    return payload;
}

} // namespace utilist::protocol
