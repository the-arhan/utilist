#pragma once

#include <utilist/protocol/pipe_channel.hpp>
#include <utilist/protocol/messages.hpp>
#include <QObject>
#include <QTimer>
#include <string>
#include <memory>

namespace utilist::app::core {

class IpcClient : public QObject {
    Q_OBJECT

public:
    explicit IpcClient(QObject* parent = nullptr);
    ~IpcClient() override;

    bool createPipeServer(const std::wstring& pipeName);
    void checkConnectionAsync();

    bool sendCommand(const std::string& command, const std::string& payloadJson = "{}");
    [[nodiscard]] bool isConnected() const noexcept;
    void close();

signals:
    void connected();
    void disconnected();
    void responseReceived(const utilist::protocol::Response& response);
    void statusUpdated(const utilist::protocol::StatusPayload& status);

private slots:
    void onPollTimer();

private:
    std::unique_ptr<protocol::PipeChannel> m_channel;
    QTimer* m_pollTimer{nullptr};
    std::wstring m_pipeName;
    bool m_wasConnected{false};
    uint64_t m_nextRequestId{1};
};

} // namespace utilist::app::core
