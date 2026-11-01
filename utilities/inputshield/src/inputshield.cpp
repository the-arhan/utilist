#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <atomic>
#include <string>
#include <memory>
#include <vector>
#include <chrono>

#include <utilist/protocol/pipe_channel.hpp>
#include <utilist/protocol/messages.hpp>
#include <utilist/protocol/json_helper.hpp>

// --- Application State ---
enum class AppState { UNLOCKED, LOCKED, ERROR_STATE };

static std::atomic<AppState> g_currentState{ AppState::UNLOCKED }; // Starts UNLOCKED
static std::atomic<bool> g_running{ true };
static std::atomic<bool> g_stateChangedManually{ false };

static HHOOK g_hKeyboardHook = NULL;
static HHOOK g_hMouseHook = NULL;
static DWORD g_mainThreadId = 0;

static const UINT WM_APP_TOGGLE_CLEANUP = WM_APP + 1;

// Manual modifier tracking (safer than GetAsyncKeyState inside hooks)
static std::atomic<bool> g_ctrlPressed{ false };
static std::atomic<bool> g_altPressed{ false };

// UTILIST IPC state
static bool g_managedMode = false;
static std::wstring g_pipeName;
static std::unique_ptr<utilist::protocol::PipeChannel> g_ipc;
static std::chrono::steady_clock::time_point g_startTime{ std::chrono::steady_clock::now() };

// --- Callbacks & Hooks ---

static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKeyBoard = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        
        // 1. Manually track Modifier Keys (Catches Left and Right variants)
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            if (pKeyBoard->vkCode == VK_CONTROL || pKeyBoard->vkCode == VK_LCONTROL || pKeyBoard->vkCode == VK_RCONTROL) g_ctrlPressed = true;
            if (pKeyBoard->vkCode == VK_MENU || pKeyBoard->vkCode == VK_LMENU || pKeyBoard->vkCode == VK_RMENU) g_altPressed = true;
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            if (pKeyBoard->vkCode == VK_CONTROL || pKeyBoard->vkCode == VK_LCONTROL || pKeyBoard->vkCode == VK_RCONTROL) g_ctrlPressed = false;
            if (pKeyBoard->vkCode == VK_MENU || pKeyBoard->vkCode == VK_LMENU || pKeyBoard->vkCode == VK_RMENU) g_altPressed = false;
        }
        
        // 2. Check for Emergency Toggle: Ctrl + Alt + I (vkCode 0x49)
        if (pKeyBoard->vkCode == 0x49) {
            if (g_ctrlPressed && g_altPressed) {
                // Only trigger on the exact down-press
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
                    // Instantly flip state for immediate response
                    if (g_currentState == AppState::LOCKED) {
                        g_currentState = AppState::UNLOCKED;
                    } else {
                        g_currentState = AppState::LOCKED;
                    }
                    g_stateChangedManually = true;
                    
                    // Tell main thread to release stuck modifier keys in the OS
                    if (g_mainThreadId != 0) {
                        PostThreadMessage(g_mainThreadId, WM_APP_TOGGLE_CLEANUP, 0, 0);
                    }
                }
                return 1; // Always swallow the toggle combination
            }
        }
        
        // 3. If locked, swallow all other keystrokes
        if (g_currentState == AppState::LOCKED) {
            return 1; 
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (g_currentState == AppState::LOCKED) {
            return 1; // Swallow all mouse movements and clicks
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// --- Hook Management ---

static bool ApplyHooks() {
    g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!g_hKeyboardHook) return false;

    g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);
    if (!g_hMouseHook) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
        return false;
    }
    return true;
}

static void ReleaseModifiers() {
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
}

static void RemoveHooks() {
    if (g_hKeyboardHook) {
        UnhookWindowsHookEx(g_hKeyboardHook);
        g_hKeyboardHook = NULL;
    }
    if (g_hMouseHook) {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = NULL;
    }
    ReleaseModifiers();
}

static utilist::protocol::StatusPayload GetCurrentStatus() {
    utilist::protocol::StatusPayload status;
    status.featureActive = (g_currentState == AppState::LOCKED);
    status.stateDescription = (g_currentState == AppState::LOCKED) ? "Input Shield locked" : "Input Shield dormant";
    status.capabilities = {"status", "activate", "deactivate", "toggle"};
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - g_startTime).count();
    status.uptimeSeconds = static_cast<uint64_t>(uptime);
    return status;
}

static void PollIPC() {
    if (!g_managedMode || !g_ipc || !g_ipc->isConnected()) return;

    if (g_stateChangedManually.exchange(false)) {
        // Status changed via emergency hotkey Ctrl+Alt+I
        auto status = GetCurrentStatus();
        utilist::protocol::Response updateRes;
        updateRes.requestId = "event-status";
        updateRes.type = "response";
        updateRes.success = true;
        updateRes.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
        g_ipc->sendMessage(utilist::protocol::JsonHelper::serializeResponse(updateRes));
    }

    auto msgs = g_ipc->pollMessages();
    for (const auto& raw : msgs) {
        auto reqOpt = utilist::protocol::JsonHelper::parseRequest(raw);
        if (!reqOpt) continue;

        const auto& req = *reqOpt;
        utilist::protocol::Response res;
        res.requestId = req.requestId;
        res.type = "response";
        res.success = true;

        if (req.command == "handshake") {
            auto status = GetCurrentStatus();
            res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
        } else if (req.command == "get_status") {
            auto status = GetCurrentStatus();
            res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
        } else if (req.command == "activate") {
            g_currentState = AppState::LOCKED;
            auto status = GetCurrentStatus();
            res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
        } else if (req.command == "deactivate") {
            g_currentState = AppState::UNLOCKED;
            ReleaseModifiers();
            auto status = GetCurrentStatus();
            res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
        } else if (req.command == "toggle") {
            if (g_currentState == AppState::LOCKED) {
                g_currentState = AppState::UNLOCKED;
                ReleaseModifiers();
            } else {
                g_currentState = AppState::LOCKED;
            }
            auto status = GetCurrentStatus();
            res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
        } else if (req.command == "shutdown") {
            auto status = GetCurrentStatus();
            status.stateDescription = "Shutting down";
            res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            g_ipc->sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
            g_running = false;
            break;
        } else {
            res.success = false;
            res.error = utilist::protocol::ErrorInfo{"UNSUPPORTED_COMMAND", "Unknown command: " + req.command};
        }

        g_ipc->sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
    }
}

// --- Main Execution (Windows App) ---

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    g_mainThreadId = GetCurrentThreadId();

    std::wstring pipeName;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg.rfind(L"--utilist-pipe=", 0) == 0) {
                pipeName = arg.substr(15);
            }
        }
        LocalFree(argv);
    }

    g_pipeName = pipeName;
    g_managedMode = !pipeName.empty();

    if (g_managedMode) {
        // In managed mode, start dormant (UNLOCKED)
        g_currentState = AppState::UNLOCKED;
        g_ipc = std::make_unique<utilist::protocol::PipeChannel>();
        if (!g_ipc->connectToServer(g_pipeName, 5000)) {
            OutputDebugStringW(L"[InputShield] Failed to connect to UTILIST pipe.\n");
            return 2;
        }
    } else {
        // In standalone mode, start LOCKED as originally designed
        g_currentState = AppState::LOCKED;
    }

    if (!ApplyHooks()) {
        if (!g_managedMode) {
            MessageBoxA(NULL, "Failed to install hooks. Security software might be blocking it.", "InputShield Error", MB_ICONERROR);
        }
        return 1;
    }

    // The Message Loop with non-blocking IPC polling
    while (g_running) {
        MsgWaitForMultipleObjectsEx(0, nullptr, 25, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            if (msg.message == WM_APP_TOGGLE_CLEANUP) {
                ReleaseModifiers();
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        if (g_managedMode) {
            PollIPC();
        }
    }

    RemoveHooks();
    if (g_ipc) {
        g_ipc->close();
    }
    return 0;
}
