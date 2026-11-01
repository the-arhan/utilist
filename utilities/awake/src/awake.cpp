#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <chrono>
#include <string>
#include <format>
#include <memory>
#include <vector>

#include <utilist/protocol/pipe_channel.hpp>
#include <utilist/protocol/messages.hpp>
#include <utilist/protocol/json_helper.hpp>

// -----------------------------------------------------------------------------
// Constants & Enums
// -----------------------------------------------------------------------------
constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr UINT_PTR TIMER_ID = 1001;
constexpr UINT_PTR IPC_TIMER_ID = 1002;

constexpr int IDM_EXIT = 2000;
constexpr int IDM_MODE_DISABLED = 2001;
constexpr int IDM_MODE_AWAKE = 2002;
constexpr int IDM_MODE_DISPLAY = 2003;

constexpr int IDM_TIME_15M = 2115;
constexpr int IDM_TIME_30M = 2130;
constexpr int IDM_TIME_1H = 2160;
constexpr int IDM_TIME_2H = 21120;
constexpr int IDM_TIME_4H = 21240;
constexpr int IDM_TIME_8H = 21480;
constexpr int IDM_TIME_INDEF = 21999;

constexpr int HOTKEY_TOGGLE_AWAKE = 3001;
constexpr int HOTKEY_TOGGLE_DISPLAY = 3002;

enum class PowerMode {
    Disabled,
    KeepAwake,
    KeepDisplayOn
};

// -----------------------------------------------------------------------------
// Core Application Class
// -----------------------------------------------------------------------------
class AwakeApp {
public:
    explicit AwakeApp(HINSTANCE hInstance) : m_hInstance(hInstance) {}
    ~AwakeApp() { Cleanup(); }

    bool Initialize(const std::wstring& pipeName = L"") {
        m_pipeName = pipeName;
        m_managedMode = !pipeName.empty();

        if (m_managedMode) {
            m_ipc = std::make_unique<utilist::protocol::PipeChannel>();
            if (!m_ipc->connectToServer(m_pipeName, 5000)) {
                OutputDebugStringW(L"[Awake] Failed to connect to UTILIST pipe.\n");
                return false;
            }
        }

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = WndProcStatic;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = L"UtilistAwakeHiddenWindow";
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Awake", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, m_hInstance, this);
        if (!m_hwnd) return false;

        InitTrayIcon();
        if (!m_managedMode) {
            RegisterShortcuts();
        }

        SetMode(PowerMode::Disabled); // Explicitly start disabled
        SetDuration(0); // Indefinite

        SetTimer(m_hwnd, TIMER_ID, 1000, NULL);
        if (m_managedMode) {
            SetTimer(m_hwnd, IPC_TIMER_ID, 25, NULL);
        }

        m_startTime = std::chrono::steady_clock::now();
        return true;
    }

    void Run() {
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

private:
    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    NOTIFYICONDATAW m_nid = { sizeof(NOTIFYICONDATAW) };
    
    PowerMode m_mode = PowerMode::Disabled;
    std::chrono::steady_clock::time_point m_deadline;
    bool m_isTimed = false;

    // UTILIST Integration
    bool m_managedMode = false;
    std::wstring m_pipeName;
    std::unique_ptr<utilist::protocol::PipeChannel> m_ipc;
    std::chrono::steady_clock::time_point m_startTime{std::chrono::steady_clock::now()};

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        AwakeApp* app = nullptr;
        if (uMsg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            app = reinterpret_cast<AwakeApp*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<AwakeApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (app) return app->WndProc(hwnd, uMsg, wParam, lParam);
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    LRESULT WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_TRAYICON:
            if (LOWORD(lParam) == WM_RBUTTONUP) {
                ShowContextMenu();
            }
            return 0;

        case WM_COMMAND:
            HandleCommand(LOWORD(wParam));
            return 0;

        case WM_HOTKEY:
            if (wParam == HOTKEY_TOGGLE_AWAKE) {
                SetMode(m_mode == PowerMode::KeepAwake ? PowerMode::Disabled : PowerMode::KeepAwake);
            } else if (wParam == HOTKEY_TOGGLE_DISPLAY) {
                SetMode(m_mode == PowerMode::KeepDisplayOn ? PowerMode::Disabled : PowerMode::KeepDisplayOn);
            }
            return 0;

        case WM_TIMER:
            if (wParam == TIMER_ID) {
                CheckTimer();
            } else if (wParam == IPC_TIMER_ID) {
                PollIPC();
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    utilist::protocol::StatusPayload GetCurrentStatus() const {
        utilist::protocol::StatusPayload status;
        status.featureActive = (m_mode != PowerMode::Disabled);
        if (m_mode == PowerMode::Disabled) {
            status.stateDescription = "Awake dormant";
        } else if (m_mode == PowerMode::KeepAwake) {
            status.stateDescription = m_isTimed ? "Keep Awake (Timed)" : "Keep Awake (Indefinite)";
        } else {
            status.stateDescription = m_isTimed ? "Keep Display On (Timed)" : "Keep Display On (Indefinite)";
        }
        status.capabilities = {"status", "activate", "deactivate", "toggle", "settings"};
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - m_startTime).count();
        status.uptimeSeconds = static_cast<uint64_t>(uptime);
        return status;
    }

    void PollIPC() {
        if (!m_managedMode || !m_ipc || !m_ipc->isConnected()) return;

        auto msgs = m_ipc->pollMessages();
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
                SetMode(PowerMode::KeepAwake);
                auto status = GetCurrentStatus();
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "deactivate") {
                SetMode(PowerMode::Disabled);
                auto status = GetCurrentStatus();
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "toggle") {
                SetMode(m_mode == PowerMode::Disabled ? PowerMode::KeepAwake : PowerMode::Disabled);
                auto status = GetCurrentStatus();
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "update_settings") {
                // Cycle modes: Disabled -> KeepAwake -> KeepDisplayOn
                if (m_mode == PowerMode::Disabled) SetMode(PowerMode::KeepAwake);
                else if (m_mode == PowerMode::KeepAwake) SetMode(PowerMode::KeepDisplayOn);
                else SetMode(PowerMode::KeepAwake);

                auto status = GetCurrentStatus();
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "shutdown") {
                auto status = GetCurrentStatus();
                status.stateDescription = "Shutting down";
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
                m_ipc->sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
                DestroyWindow(m_hwnd);
                break;
            } else {
                res.success = false;
                res.error = utilist::protocol::ErrorInfo{"UNSUPPORTED_COMMAND", "Unknown command: " + req.command};
            }

            m_ipc->sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
        }
    }

    void HandleCommand(WORD cmd) {
        switch (cmd) {
        case IDM_EXIT: DestroyWindow(m_hwnd); break;
        case IDM_MODE_DISABLED: SetMode(PowerMode::Disabled); break;
        case IDM_MODE_AWAKE: SetMode(PowerMode::KeepAwake); break;
        case IDM_MODE_DISPLAY: SetMode(PowerMode::KeepDisplayOn); break;
        
        case IDM_TIME_15M: SetDuration(15); break;
        case IDM_TIME_30M: SetDuration(30); break;
        case IDM_TIME_1H: SetDuration(60); break;
        case IDM_TIME_2H: SetDuration(120); break;
        case IDM_TIME_4H: SetDuration(240); break;
        case IDM_TIME_8H: SetDuration(480); break;
        case IDM_TIME_INDEF: SetDuration(0); break;
        }
    }

    void SetMode(PowerMode newMode) {
        m_mode = newMode;
        EXECUTION_STATE state = ES_CONTINUOUS;

        if (m_mode == PowerMode::KeepAwake) {
            state |= ES_SYSTEM_REQUIRED;
        } else if (m_mode == PowerMode::KeepDisplayOn) {
            state |= ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED;
        }

        if (SetThreadExecutionState(state) == 0) {
            // Failed to set state - fallback to disabled visually
            m_mode = PowerMode::Disabled;
        }

        if (m_mode == PowerMode::Disabled) {
            m_isTimed = false; // Clear timer if manually disabled
        }

        UpdateTrayState();
    }

    void SetDuration(int minutes) {
        if (minutes == 0) {
            m_isTimed = false;
        } else {
            m_isTimed = true;
            m_deadline = std::chrono::steady_clock::now() + std::chrono::minutes(minutes);
            // Default to Keep Display On if a timer is selected but mode is currently disabled
            if (m_mode == PowerMode::Disabled) {
                SetMode(PowerMode::KeepDisplayOn);
            }
        }
        UpdateTrayState();
    }

    void CheckTimer() {
        if (!m_isTimed || m_mode == PowerMode::Disabled) return;

        if (std::chrono::steady_clock::now() >= m_deadline) {
            SetMode(PowerMode::Disabled);
            ShowNotification(L"Awake Session Ended", L"Normal power management restored.");
        } else {
            UpdateTrayState(); // Update tooltip countdown
        }
    }

    void InitTrayIcon() {
        m_nid.hWnd = m_hwnd;
        m_nid.uID = 1;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        m_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(m_nid.szTip, L"Awake: Disabled");
        Shell_NotifyIconW(NIM_ADD, &m_nid);
    }

    void UpdateTrayState() {
        std::wstring status;
        if (m_mode == PowerMode::Disabled) status = L"Awake: Disabled";
        else if (m_mode == PowerMode::KeepAwake) status = L"Keep Awake";
        else status = L"Keep Display On";

        if (m_isTimed && m_mode != PowerMode::Disabled) {
            auto now = std::chrono::steady_clock::now();
            auto minsLeft = std::chrono::duration_cast<std::chrono::minutes>(m_deadline - now).count();
            if (minsLeft < 1) minsLeft = 1; // Show at least 1 min remaining
            status += std::format(L" ({}m left)", minsLeft);
        }

        wcsncpy_s(m_nid.szTip, status.c_str(), _TRUNCATE);
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
    }

    void ShowNotification(const wchar_t* title, const wchar_t* msg) {
        m_nid.uFlags |= NIF_INFO;
        wcsncpy_s(m_nid.szInfoTitle, title, _TRUNCATE);
        wcsncpy_s(m_nid.szInfo, msg, _TRUNCATE);
        m_nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &m_nid);
        m_nid.uFlags &= ~NIF_INFO; // Remove flag so it doesn't constantly show on subsequent updates
    }

    void ShowContextMenu() {
        HMENU hMenu = CreatePopupMenu();
        
        AppendMenuW(hMenu, MF_STRING | (m_mode == PowerMode::Disabled ? MF_CHECKED : 0), IDM_MODE_DISABLED, L"Disabled");
        AppendMenuW(hMenu, MF_STRING | (m_mode == PowerMode::KeepAwake ? MF_CHECKED : 0), IDM_MODE_AWAKE, L"Keep Awake");
        AppendMenuW(hMenu, MF_STRING | (m_mode == PowerMode::KeepDisplayOn ? MF_CHECKED : 0), IDM_MODE_DISPLAY, L"Keep Display On");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        
        HMENU hTimeMenu = CreatePopupMenu();
        AppendMenuW(hTimeMenu, MF_STRING | (!m_isTimed ? MF_CHECKED : 0), IDM_TIME_INDEF, L"Indefinite");
        AppendMenuW(hTimeMenu, MF_STRING, IDM_TIME_15M, L"15 Minutes");
        AppendMenuW(hTimeMenu, MF_STRING, IDM_TIME_30M, L"30 Minutes");
        AppendMenuW(hTimeMenu, MF_STRING, IDM_TIME_1H, L"1 Hour");
        AppendMenuW(hTimeMenu, MF_STRING, IDM_TIME_2H, L"2 Hours");
        AppendMenuW(hTimeMenu, MF_STRING, IDM_TIME_4H, L"4 Hours");
        AppendMenuW(hTimeMenu, MF_STRING, IDM_TIME_8H, L"8 Hours");
        
        AppendMenuW(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(hTimeMenu), L"Duration");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, m_hwnd, NULL);
        DestroyMenu(hMenu);
    }

    void RegisterShortcuts() {
        RegisterHotKey(m_hwnd, HOTKEY_TOGGLE_AWAKE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'A');
        RegisterHotKey(m_hwnd, HOTKEY_TOGGLE_DISPLAY, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'A');
    }

    void Cleanup() {
        if (m_hwnd) {
            KillTimer(m_hwnd, TIMER_ID);
            if (m_managedMode) {
                KillTimer(m_hwnd, IPC_TIMER_ID);
            }
            if (!m_managedMode) {
                UnregisterHotKey(m_hwnd, HOTKEY_TOGGLE_AWAKE);
                UnregisterHotKey(m_hwnd, HOTKEY_TOGGLE_DISPLAY);
            }
            SetThreadExecutionState(ES_CONTINUOUS); // Release states
            Shell_NotifyIconW(NIM_DELETE, &m_nid);
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
        if (m_ipc) {
            m_ipc->close();
        }
    }
};

// -----------------------------------------------------------------------------
// Entry Point
// -----------------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
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

    HANDLE hMutex = nullptr;
    if (pipeName.empty()) {
        // Ensure only one standalone instance runs
        hMutex = CreateMutexW(NULL, TRUE, L"UtilistAwakeMutex_1984");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
    }

    AwakeApp app(hInstance);
    if (app.Initialize(pipeName)) {
        app.Run();
    }

    if (hMutex) {
        CloseHandle(hMutex);
    }
    return 0;
}
