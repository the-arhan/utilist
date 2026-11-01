#include "ipc_client.hpp"
#include <utilist/protocol/json_helper.hpp>
#include "logging_service.hpp"

namespace utilist::app::core {

IpcClient::IpcClient(QObject* parent)
    : QObject(parent),
      m_channel(std::make_unique<protocol::PipeChannel>()),
      m_pollTimer(new QTimer(this)) {
    connect(m_pollTimer, &QTimer::timeout, this, &IpcClient::onPollTimer);
}

IpcClient::~IpcClient() {
    close();
}

bool IpcClient::createPipeServer(const std::wstring& pipeName) {
    m_pipeName = pipeName;
    m_wasConnected = false;
    if (!m_channel->createServer(pipeName)) {
        return false;
    }
    // Start poll timer to detect client connection and incoming messages
    m_pollTimer->start(25); // 25ms polling
    return true;
}

void IpcClient::checkConnectionAsync() {
    if (!m_channel->isConnected()) {
        m_channel->waitForClientConnection(10);
    }
}

bool IpcClient::sendCommand(const std::string& command, const std::string& payloadJson) {
    if (!m_channel->isConnected()) {
        return false;
    }

    protocol::Request req;
    req.protocolVersion = protocol::CURRENT_PROTOCOL_VERSION;
    req.requestId = "req-" + std::to_string(m_nextRequestId++);
    req.type = "command";
    req.command = command;
    req.payloadJson = payloadJson.empty() ? "{}" : payloadJson;

    std::string serialized = protocol::JsonHelper::serializeRequest(req);
    return m_channel->sendMessage(serialized);
}

bool IpcClient::isConnected() const noexcept {
    return m_channel && m_channel->isConnected();
}

void IpcClient::close() {
    m_pollTimer->stop();
    if (m_channel) {
        m_channel->close();
    }
    if (m_wasConnected) {
        m_wasConnected = false;
        emit disconnected();
    }
}

void IpcClient::onPollTimer() {
    if (!m_channel) return;

    if (!m_channel->isConnected()) {
        // Try connecting if waiting for client
        if (m_channel->waitForClientConnection(0)) {
            m_wasConnected = true;
            emit connected();
            LoggingService::instance().info("Utility connected to named pipe.");
        }
        return;
    }

    if (!m_wasConnected && m_channel->isConnected()) {
        m_wasConnected = true;
        emit connected();
    }

    auto messages = m_channel->pollMessages();
    for (const auto& raw : messages) {
        auto resOpt = protocol::JsonHelper::parseResponse(raw);
        if (resOpt) {
            emit responseReceived(*resOpt);

            // Check if response contains a status payload
            auto statusOpt = protocol::JsonHelper::parseStatusPayload(resOpt->payloadJson);
            if (statusOpt) {
                emit statusUpdated(*statusOpt);
            }
        }
    }

    if (!m_channel->isConnected() && m_wasConnected) {
        m_wasConnected = false;
        emit disconnected();
        LoggingService::instance().warn("Utility disconnected from named pipe.");
    }
}

} // namespace utilist::app::core
