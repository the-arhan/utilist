#pragma once

#include "messages.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <optional>

namespace utilist::protocol {

class FrameBuffer {
public:
    static std::vector<uint8_t> frameMessage(const std::string& jsonPayload) {
        if (jsonPayload.size() > MAX_MESSAGE_SIZE) {
            return {};
        }

        uint32_t length = static_cast<uint32_t>(jsonPayload.size());
        std::vector<uint8_t> buffer(sizeof(uint32_t) + length);

        // 4 bytes length prefix in little-endian
        buffer[0] = static_cast<uint8_t>(length & 0xFF);
        buffer[1] = static_cast<uint8_t>((length >> 8) & 0xFF);
        buffer[2] = static_cast<uint8_t>((length >> 16) & 0xFF);
        buffer[3] = static_cast<uint8_t>((length >> 24) & 0xFF);

        std::memcpy(buffer.data() + sizeof(uint32_t), jsonPayload.data(), length);
        return buffer;
    }

    void appendData(const uint8_t* data, size_t length) {
        m_buffer.insert(m_buffer.end(), data, data + length);
    }

    void appendData(const char* data, size_t length) {
        appendData(reinterpret_cast<const uint8_t*>(data), length);
    }

    std::optional<std::string> extractNextMessage() {
        if (m_buffer.size() < sizeof(uint32_t)) {
            return std::nullopt;
        }

        uint32_t payloadLength = static_cast<uint32_t>(m_buffer[0]) |
                                (static_cast<uint32_t>(m_buffer[1]) << 8) |
                                (static_cast<uint32_t>(m_buffer[2]) << 16) |
                                (static_cast<uint32_t>(m_buffer[3]) << 24);

        if (payloadLength > MAX_MESSAGE_SIZE) {
            // Buffer corrupted or oversized payload; clear buffer to recover
            m_buffer.clear();
            return std::nullopt;
        }

        size_t totalExpectedSize = sizeof(uint32_t) + payloadLength;
        if (m_buffer.size() < totalExpectedSize) {
            // Incomplete message, wait for more data
            return std::nullopt;
        }

        std::string message(
            reinterpret_cast<const char*>(m_buffer.data() + sizeof(uint32_t)),
            payloadLength
        );

        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + totalExpectedSize);
        return message;
    }

    void clear() {
        m_buffer.clear();
    }

    [[nodiscard]] size_t bufferedBytes() const noexcept {
        return m_buffer.size();
    }

private:
    std::vector<uint8_t> m_buffer;
};

} // namespace utilist::protocol
