#include <utilist/protocol/pipe_channel.hpp>
#include <utilist/protocol/json_helper.hpp>
#include <utilist/protocol/messages.hpp>
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::cout << "[INTEGRATION TEST] Starting Lifecycle & IPC test..." << std::endl;

    fs::path heartbeatExe;
    if (argc > 1) {
        heartbeatExe = argv[1];
    } else {
        fs::path testExe(argv[0]);
        fs::path exeDir = testExe.parent_path();
        heartbeatExe = exeDir / "HeartbeatUtility.exe";

        if (!fs::exists(heartbeatExe)) {
            // Check multi-config directories
            auto buildRoot = exeDir.parent_path().parent_path();
            auto configName = exeDir.filename(); // e.g. Debug or Release
            auto candidate1 = buildRoot / "utilities" / "examples" / "heartbeat-utility" / configName / "HeartbeatUtility.exe";
            auto candidate2 = buildRoot / "utilities" / "examples" / "heartbeat-utility" / "HeartbeatUtility.exe";
            if (fs::exists(candidate1)) {
                heartbeatExe = candidate1;
            } else if (fs::exists(candidate2)) {
                heartbeatExe = candidate2;
            }
        }
    }

    if (!fs::exists(heartbeatExe)) {
        std::cerr << "Could not find HeartbeatUtility.exe at " << heartbeatExe.string() << std::endl;
        return 1;
    }

    std::wstring pipeName = L"\\\\.\\pipe\\utilist.test.lifecycle." + std::to_wstring(GetCurrentProcessId());

    utilist::protocol::PipeChannel serverChannel;
    if (!serverChannel.createServer(pipeName)) {
        std::cerr << "Failed to create named pipe server." << std::endl;
        return 2;
    }

    // Launch HeartbeatUtility
    std::wstring cmdLine = L"\"" + heartbeatExe.wstring() + L"\" --utilist-pipe=" + pipeName;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuffer(cmdLine.begin(), cmdLine.end());
    cmdBuffer.push_back(L'\0');

    BOOL created = CreateProcessW(
        nullptr,
        cmdBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    if (!created) {
        std::cerr << "Failed to launch HeartbeatUtility.exe, error=" << GetLastError() << std::endl;
        return 3;
    }

    std::cout << "[INTEGRATION TEST] Child launched, waiting for pipe connection..." << std::endl;
    bool connected = serverChannel.waitForClientConnection(5000);
    assert(connected && "Client failed to connect within 5 seconds");

    auto sendAndReceive = [&](const utilist::protocol::Request& req) -> std::optional<utilist::protocol::Response> {
        serverChannel.sendMessage(utilist::protocol::JsonHelper::serializeRequest(req));
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 3000) {
            auto msgs = serverChannel.pollMessages();
            for (const auto& m : msgs) {
                auto res = utilist::protocol::JsonHelper::parseResponse(m);
                if (res && res->requestId == req.requestId) {
                    return res;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return std::nullopt;
    };

    // 1. Handshake
    std::cout << "[INTEGRATION TEST] Testing handshake..." << std::endl;
    utilist::protocol::Request handshakeReq;
    handshakeReq.requestId = "req-1";
    handshakeReq.type = "handshake";
    handshakeReq.command = "handshake";
    auto handshakeRes = sendAndReceive(handshakeReq);
    assert(handshakeRes.has_value());
    assert(handshakeRes->success);

    // 2. Query Status
    std::cout << "[INTEGRATION TEST] Testing get_status..." << std::endl;
    utilist::protocol::Request statusReq;
    statusReq.requestId = "req-2";
    statusReq.type = "command";
    statusReq.command = "get_status";
    auto statusRes = sendAndReceive(statusReq);
    assert(statusRes.has_value());
    assert(statusRes->success);
    auto statusPayload = utilist::protocol::JsonHelper::parseStatusPayload(statusRes->payloadJson);
    assert(statusPayload.has_value());
    assert(!statusPayload->featureActive);

    // 3. Activate feature
    std::cout << "[INTEGRATION TEST] Testing activate..." << std::endl;
    utilist::protocol::Request actReq;
    actReq.requestId = "req-3";
    actReq.type = "command";
    actReq.command = "activate";
    auto actRes = sendAndReceive(actReq);
    assert(actRes.has_value());
    assert(actRes->success);

    // 4. Verify activated via get_status
    statusReq.requestId = "req-4";
    auto statusRes2 = sendAndReceive(statusReq);
    assert(statusRes2.has_value());
    auto statusPayload2 = utilist::protocol::JsonHelper::parseStatusPayload(statusRes2->payloadJson);
    assert(statusPayload2.has_value());
    assert(statusPayload2->featureActive);

    // 5. Deactivate
    std::cout << "[INTEGRATION TEST] Testing deactivate..." << std::endl;
    utilist::protocol::Request deactReq;
    deactReq.requestId = "req-5";
    deactReq.type = "command";
    deactReq.command = "deactivate";
    auto deactRes = sendAndReceive(deactReq);
    assert(deactRes.has_value());
    assert(deactRes->success);

    // 6. Graceful shutdown
    std::cout << "[INTEGRATION TEST] Testing graceful shutdown..." << std::endl;
    utilist::protocol::Request shutReq;
    shutReq.requestId = "req-6";
    shutReq.type = "command";
    shutReq.command = "shutdown";
    auto shutRes = sendAndReceive(shutReq);
    assert(shutRes.has_value());
    assert(shutRes->success);

    // Wait for child process exit
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 3000);
    assert(waitResult == WAIT_OBJECT_0 && "Child process did not exit within 3 seconds");

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    assert(exitCode == 0 && "Child process exited with non-zero code");

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    serverChannel.close();

    // 7. Testing unexpected process termination (simulating crash)
    std::cout << "[INTEGRATION TEST] Testing unexpected process termination handling..." << std::endl;
    std::wstring crashPipeName = L"\\\\.\\pipe\\utilist.test.crash." + std::to_wstring(GetCurrentProcessId());
    utilist::protocol::PipeChannel crashServer;
    assert(crashServer.createServer(crashPipeName));

    std::wstring crashCmd = L"\"" + heartbeatExe.wstring() + L"\" --utilist-pipe=" + crashPipeName;
    std::vector<wchar_t> crashCmdBuf(crashCmd.begin(), crashCmd.end());
    crashCmdBuf.push_back(L'\0');
    STARTUPINFOW crashSi{};
    crashSi.cb = sizeof(crashSi);
    PROCESS_INFORMATION crashPi{};
    assert(CreateProcessW(nullptr, crashCmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &crashSi, &crashPi));
    assert(crashServer.waitForClientConnection(5000));

    // Abruptly terminate the process simulating a crash
    TerminateProcess(crashPi.hProcess, 99);
    WaitForSingleObject(crashPi.hProcess, 3000);

    // Host should detect disconnected pipe
    crashServer.pollMessages();
    assert(!crashServer.isConnected() && "Host did not detect pipe disconnection after child crashed");

    CloseHandle(crashPi.hProcess);
    CloseHandle(crashPi.hThread);
    crashServer.close();
    std::cout << "[INTEGRATION TEST] Unexpected termination handling PASSED." << std::endl;

    std::cout << "[INTEGRATION TEST] All lifecycle and IPC checks PASSED." << std::endl;
    return 0;
}
