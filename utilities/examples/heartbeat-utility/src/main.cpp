#include <utilist/protocol/pipe_channel.hpp>
#include <utilist/protocol/json_helper.hpp>
#include <utilist/protocol/messages.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <string>

int main(int argc, char* argv[]) {
    std::wstring pipeName;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("--utilist-pipe=", 0) == 0) {
            std::string sub = arg.substr(15);
            pipeName = std::wstring(sub.begin(), sub.end());
        }
    }

    if (pipeName.empty()) {
        std::cerr << "Usage: HeartbeatUtility.exe --utilist-pipe=\\\\.\\pipe\\<name>" << std::endl;
        return 1;
    }

    std::cout << "[Heartbeat] Connecting to UTILIST named pipe..." << std::endl;
    utilist::protocol::PipeChannel channel;
    if (!channel.connectToServer(pipeName, 5000)) {
        std::cerr << "[Heartbeat] Failed to connect to pipe." << std::endl;
        return 2;
    }

    std::cout << "[Heartbeat] Connected to host! Entering dispatch loop." << std::endl;
    auto startTime = std::chrono::steady_clock::now();
    bool featureActive = false;
    bool running = true;

    while (running && channel.isConnected()) {
        auto messages = channel.pollMessages();
        for (const auto& rawMsg : messages) {
            auto reqOpt = utilist::protocol::JsonHelper::parseRequest(rawMsg);
            if (!reqOpt) {
                continue;
            }

            const auto& req = *reqOpt;
            utilist::protocol::Response res;
            res.requestId = req.requestId;
            res.type = "response";
            res.success = true;

            if (req.command == "handshake") {
                res.payloadJson = "{\"utility_id\":\"example.heartbeat\",\"protocol_version\":1}";
            } else if (req.command == "get_status") {
                auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - startTime).count();

                utilist::protocol::StatusPayload status;
                status.featureActive = featureActive;
                status.stateDescription = featureActive ? "Heartbeat pulsing" : "Heartbeat idle";
                status.capabilities = {"status", "activate", "deactivate", "toggle"};
                status.uptimeSeconds = static_cast<uint64_t>(uptime);

                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "activate") {
                featureActive = true;
                res.payloadJson = "{\"feature_active\":true}";
            } else if (req.command == "deactivate") {
                featureActive = false;
                res.payloadJson = "{\"feature_active\":false}";
            } else if (req.command == "toggle") {
                featureActive = !featureActive;
                res.payloadJson = std::string("{\"feature_active\":") + (featureActive ? "true}" : "false}");
            } else if (req.command == "shutdown") {
                res.payloadJson = "{\"shutting_down\":true}";
                channel.sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
                running = false;
                break;
            } else {
                res.success = false;
                res.error = utilist::protocol::ErrorInfo{"UNSUPPORTED_COMMAND", "Unknown command: " + req.command};
            }

            channel.sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "[Heartbeat] Utility exiting cleanly." << std::endl;
    channel.close();
    return 0;
}
