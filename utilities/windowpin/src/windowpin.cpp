// WindowPin.cpp - UTILIST Module
// C++20 | Native Win32 API

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <system_error>
#include <chrono>
#include <vector>

#include <utilist/protocol/pipe_channel.hpp>
#include <utilist/protocol/messages.hpp>
#include <utilist/protocol/json_helper.hpp>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")

// ---------------------------------------------------------
// Constants & Definitions
// ---------------------------------------------------------
constexpr int HOTKEY_ID = 1001;
constexpr UINT TRAY_ICON_ID = 1002;
constexpr UINT WM_TRAYICON = WM_USER + 1;
constexpr int TRAY_EXIT_ID = 2001;
constexpr UINT_PTR IPC_TIMER_ID = 3001;

const COLORREF COLOR_TRANSPARENT_KEY = RGB(255, 0, 255); // Magenta
const COLORREF COLOR_BORDER = RGB(0, 150, 255);         // Subtle Electric Blue
constexpr int BORDER_THICKNESS = 3;

// ---------------------------------------------------------
// PinnedWindow & Overlay Logic
// ---------------------------------------------------------
class PinnedWindow {
private:
    HWND m_targetHwnd;
    HWND m_overlayHwnd;

    static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // Fill background with transparent color key
            HBRUSH hBrushBg = CreateSolidBrush(COLOR_TRANSPARENT_KEY);
            FillRect(hdc, &rc, hBrushBg);
            DeleteObject(hBrushBg);

            // Draw border
            HPEN hPen = CreatePen(PS_SOLID, BORDER_THICKNESS, COLOR_BORDER);
            HGDIOBJ hOldPen = SelectObject(hdc, hPen);
            HGDIOBJ hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    static void RegisterOverlayClass() {
        static bool registered = false;
        if (registered) return;
        
        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = OverlayWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"WindowPinOverlayClass";
        wc.hbrBackground = nullptr;
        RegisterClassExW(&wc);
        registered = true;
    }

public:
    explicit PinnedWindow(HWND target) : m_targetHwnd(target), m_overlayHwnd(nullptr) {
        RegisterOverlayClass();

        m_overlayHwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            L"WindowPinOverlayClass", L"WindowPinOverlay",
            WS_POPUP,
            0, 0, 0, 0,
            nullptr, nullptr, GetModuleHandle(nullptr), nullptr
        );

        SetLayeredWindowAttributes(m_overlayHwnd, COLOR_TRANSPARENT_KEY, 0, LWA_COLORKEY);
        
        // Initial pin and sync
        SetWindowPos(m_targetHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        UpdateOverlay();
        ShowWindow(m_overlayHwnd, SW_SHOWNA);
    }

    ~PinnedWindow() {
        if (IsWindow(m_targetHwnd)) {
            SetWindowPos(m_targetHwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        if (IsWindow(m_overlayHwnd)) {
            DestroyWindow(m_overlayHwnd);
        }
    }

    void UpdateOverlay() {
        if (!IsWindow(m_targetHwnd)) return;

        if (IsIconic(m_targetHwnd)) {
            ShowWindow(m_overlayHwnd, SW_HIDE);
            return;
        }

        RECT rc;
        if (S_OK != DwmGetWindowAttribute(m_targetHwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc))) {
            GetWindowRect(m_targetHwnd, &rc);
        }

        // Keep overlay exactly on top of the target window without taking focus
        SetWindowPos(m_overlayHwnd, HWND_TOPMOST, 
                     rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 
                     SWP_NOACTIVATE | SWP_NOCOPYBITS);
                     
        if (!IsWindowVisible(m_overlayHwnd)) {
            ShowWindow(m_overlayHwnd, SW_SHOWNA);
        }
    }

    void HideOverlay() {
        ShowWindow(m_overlayHwnd, SW_HIDE);
    }
};

// ---------------------------------------------------------
// Application Architecture & Manager
// ---------------------------------------------------------
class WindowPinApp {
private:
    HWND m_msgHwnd = nullptr;
    std::unordered_map<HWND, std::unique_ptr<PinnedWindow>> m_pinnedWindows;
    HWINEVENTHOOK m_hEventHookLocation = nullptr;
    HWINEVENTHOOK m_hEventHookState = nullptr;
    NOTIFYICONDATAW m_nid = { 0 };

    // UTILIST Integration
    bool m_managedMode = false;
    bool m_featureActive = false;
    std::wstring m_pipeName;
    std::unique_ptr<utilist::protocol::PipeChannel> m_ipc;
    std::chrono::steady_clock::time_point m_startTime{ std::chrono::steady_clock::now() };

    static WindowPinApp* s_instance;

    bool IsValidTarget(HWND hwnd) {
        if (!IsWindow(hwnd) || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) return false;
        
        wchar_t className[256];
        GetClassNameW(hwnd, className, 256);
        std::wstring cls(className);
        if (cls == L"Progman" || cls == L"WorkerW" || cls == L"Shell_TrayWnd" || cls == L"WindowPinOverlayClass") {
            return false;
        }
        return true;
    }

    static void CALLBACK WinEventProc([[maybe_unused]] HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, [[maybe_unused]] LONG idChild, [[maybe_unused]] DWORD dwEventThread, [[maybe_unused]] DWORD dwmsEventTime) {
        if (idObject != OBJID_WINDOW || !s_instance) return;

        auto it = s_instance->m_pinnedWindows.find(hwnd);
        if (it != s_instance->m_pinnedWindows.end()) {
            if (event == EVENT_OBJECT_LOCATIONCHANGE || event == EVENT_SYSTEM_MINIMIZEEND || event == EVENT_OBJECT_SHOW) {
                it->second->UpdateOverlay();
            } else if (event == EVENT_SYSTEM_MINIMIZESTART || event == EVENT_OBJECT_HIDE) {
                it->second->HideOverlay();
            } else if (event == EVENT_OBJECT_DESTROY) {
                s_instance->m_pinnedWindows.erase(it);
                s_instance->NotifyStatusChanged();
            }
        }
    }

    void ShowContextMenu() {
        POINT pt;
        GetCursorPos(&pt);
        HMENU hMenu = CreatePopupMenu();
        InsertMenuW(hMenu, static_cast<UINT>(-1), MF_BYPOSITION | MF_STRING, TRAY_EXIT_ID, L"Exit WindowPin");
        
        SetForegroundWindow(m_msgHwnd);
        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, m_msgHwnd, nullptr);
        DestroyMenu(hMenu);
    }

    static LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_HOTKEY && wParam == HOTKEY_ID) {
            HWND fg = GetForegroundWindow();
            if (s_instance) {
                s_instance->TogglePin(fg);
                s_instance->NotifyStatusChanged();
            }
            return 0;
        } else if (msg == WM_TRAYICON) {
            if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_LBUTTONUP) {
                if (s_instance) s_instance->ShowContextMenu();
            }
            return 0;
        } else if (msg == WM_COMMAND) {
            if (LOWORD(wParam) == TRAY_EXIT_ID) {
                PostQuitMessage(0);
            }
            return 0;
        } else if (msg == WM_TIMER) {
            if (wParam == IPC_TIMER_ID && s_instance) {
                s_instance->PollIPC();
            }
            return 0;
        } else if (msg == WM_DESTROY) {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

public:
    WindowPinApp() {
        s_instance = this;
    }

    ~WindowPinApp() {
        Cleanup();
        s_instance = nullptr;
    }

    void Cleanup() {
        if (m_msgHwnd) {
            KillTimer(m_msgHwnd, IPC_TIMER_ID);
            UnregisterHotKey(m_msgHwnd, HOTKEY_ID);
            Shell_NotifyIconW(NIM_DELETE, &m_nid);
            DestroyWindow(m_msgHwnd);
            m_msgHwnd = nullptr;
        }
        if (m_hEventHookLocation) {
            UnhookWinEvent(m_hEventHookLocation);
            m_hEventHookLocation = nullptr;
        }
        if (m_hEventHookState) {
            UnhookWinEvent(m_hEventHookState);
            m_hEventHookState = nullptr;
        }
        m_pinnedWindows.clear();
        if (m_ipc) {
            m_ipc->close();
        }
    }

    void TogglePin(HWND hwnd) {
        if (!IsValidTarget(hwnd)) return;

        if (m_pinnedWindows.find(hwnd) != m_pinnedWindows.end()) {
            m_pinnedWindows.erase(hwnd); // Destructor unpins and destroys overlay
        } else {
            m_pinnedWindows[hwnd] = std::make_unique<PinnedWindow>(hwnd);
        }
    }

    void UnpinAll() {
        m_pinnedWindows.clear();
    }

    utilist::protocol::StatusPayload GetCurrentStatus() const {
        utilist::protocol::StatusPayload status;
        status.featureActive = m_featureActive || !m_pinnedWindows.empty();
        if (m_pinnedWindows.empty()) {
            status.stateDescription = m_featureActive ? "WindowPin active" : "No pinned windows";
        } else {
            status.stateDescription = std::to_string(m_pinnedWindows.size()) + " window(s) pinned";
        }
        status.capabilities = {"status", "activate", "deactivate", "toggle"};
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - m_startTime).count();
        status.uptimeSeconds = static_cast<uint64_t>(uptime);
        return status;
    }

    void NotifyStatusChanged() {
        if (m_managedMode && m_ipc && m_ipc->isConnected()) {
            auto status = GetCurrentStatus();
            utilist::protocol::Response updateRes;
            updateRes.requestId = "event-status";
            updateRes.type = "response";
            updateRes.success = true;
            updateRes.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            m_ipc->sendMessage(utilist::protocol::JsonHelper::serializeResponse(updateRes));
        }
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
                m_featureActive = true;
                HWND fg = GetForegroundWindow();
                if (fg && IsValidTarget(fg)) {
                    if (m_pinnedWindows.find(fg) == m_pinnedWindows.end()) {
                        m_pinnedWindows[fg] = std::make_unique<PinnedWindow>(fg);
                    }
                }
                auto status = GetCurrentStatus();
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "deactivate") {
                m_featureActive = false;
                UnpinAll();
                auto status = GetCurrentStatus();
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "toggle") {
                m_featureActive = !m_featureActive;
                if (!m_featureActive) {
                    UnpinAll();
                } else {
                    HWND fg = GetForegroundWindow();
                    if (fg && IsValidTarget(fg)) {
                        TogglePin(fg);
                    }
                }
                auto status = GetCurrentStatus();
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "shutdown") {
                m_featureActive = false;
                auto status = GetCurrentStatus();
                status.stateDescription = "Shutting down";
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
                m_ipc->sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
                UnpinAll();
                PostQuitMessage(0);
                break;
            } else {
                res.success = false;
                res.error = utilist::protocol::ErrorInfo{"UNSUPPORTED_COMMAND", "Unknown command: " + req.command};
            }

            m_ipc->sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
        }
    }

    bool Initialize(const std::wstring& pipeName = L"") {
        m_pipeName = pipeName;
        m_managedMode = !pipeName.empty();

        if (m_managedMode) {
            m_ipc = std::make_unique<utilist::protocol::PipeChannel>();
            if (!m_ipc->connectToServer(m_pipeName, 5000)) {
                OutputDebugStringW(L"[WindowPin] Failed to connect to UTILIST pipe.\n");
                return false;
            }
        }

        // High DPI Awareness to ensure border tracks exactly on high-res monitors
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
        wc.lpfnWndProc = MsgWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = L"WindowPinMessageClass";
        RegisterClassExW(&wc);

        m_msgHwnd = CreateWindowExW(0, L"WindowPinMessageClass", L"WindowPinApp", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
        if (!m_msgHwnd) return false;

        if (!m_managedMode) {
            // Register Global Hotkey: Ctrl + Alt + W in standalone mode
            RegisterHotKey(m_msgHwnd, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W');
        }

        // Setup Event Hooks for tracking window movements and lifecycle
        m_hEventHookLocation = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
        m_hEventHookState = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_OBJECT_DESTROY, nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

        // System Tray Icon
        m_nid.cbSize = sizeof(NOTIFYICONDATAW);
        m_nid.hWnd = m_msgHwnd;
        m_nid.uID = TRAY_ICON_ID;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        m_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        wcscpy_s(m_nid.szTip, L"UTILIST WindowPin\nCtrl+Alt+W to Pin");
        Shell_NotifyIconW(NIM_ADD, &m_nid);

        if (m_managedMode) {
            SetTimer(m_msgHwnd, IPC_TIMER_ID, 25, NULL);
        }

        m_startTime = std::chrono::steady_clock::now();
        return true;
    }

    void Run() {
        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
};

WindowPinApp* WindowPinApp::s_instance = nullptr;

// ---------------------------------------------------------
// Entry Point
// ---------------------------------------------------------
int APIENTRY wWinMain([[maybe_unused]] _In_ HINSTANCE hInstance, [[maybe_unused]] _In_opt_ HINSTANCE hPrevInstance, [[maybe_unused]] _In_ LPWSTR lpCmdLine, [[maybe_unused]] _In_ int nCmdShow) {
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
        // Prevent multiple standalone instances
        hMutex = CreateMutexW(nullptr, TRUE, L"UTILIST_WindowPin_Mutex");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (hMutex) CloseHandle(hMutex);
            return 0;
        }
    }

    WindowPinApp app;
    if (app.Initialize(pipeName)) {
        app.Run();
    }

    if (hMutex) {
        CloseHandle(hMutex);
    }
    return 0;
}
