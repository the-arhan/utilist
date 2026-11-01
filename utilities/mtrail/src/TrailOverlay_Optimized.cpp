// =============================================================================
// TrailOverlay_Optimized.cpp
// Hardware-accelerated ribbon trail overlay: DirectComposition + Direct2D +
// Direct3D 11, fed by a low-level mouse hook running on its own thread.
//
// Modified for UTILIST integration.
// =============================================================================

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellscalingapi.h>
#include <shellapi.h> // For CommandLineToArgvW
#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <wrl.h>

#include <utilist/protocol/pipe_channel.hpp>
#include <utilist/protocol/messages.hpp>
#include <utilist/protocol/json_helper.hpp>
#include <chrono>
#include <regex>
#include <sstream>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <future>
#include <limits>
#include <thread>
#include <vector>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "user32.lib")

using Microsoft::WRL::ComPtr;

// -----------------------------------------------------------------------------
// Tunables
// -----------------------------------------------------------------------------
namespace cfg {
constexpr double kTailMs          = 270.0;   // lifetime of a trail point
constexpr float  kMinPointDistPx  = 2.0f;    // raw-sample acceptance spacing (x DPI scale)
constexpr float  kMinSegmentPx    = 3.0f;    // spacing of the smoothed ribbon spine (x DPI scale)
constexpr float  kTeleportPx      = 400.0f;  // one-event jump that starts a new stroke (x DPI scale)
constexpr size_t kMaxPoints       = 1024;    // hard cap on stored raw points
constexpr float  kSpeedWindowSec  = 0.080f;  // path length over this window = raw speed
constexpr float  kSpeedSmoothSec  = 0.060f;  // EMA time constant for speed
constexpr float  kSpeedRefPxSec   = 2000.0f; // speed that counts as "fast" (x DPI scale)
constexpr float  kGradientLenPx   = 150.0f;  // Aurora colour ramp length (x DPI scale)
constexpr float  kHueDegPerSec    = 72.0f;   // Spectrum hue drift (one cycle per 5 s)
constexpr UINT   kMaxSurfacePx    = 16384;   // D3D11 feature-level 11 texture limit
}  // namespace cfg

static const UINT HK_MODE = 1;  // Ctrl+Alt+M
static const UINT HK_EXIT = 2;  // Ctrl+Alt+E

static const UINT kSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

enum class TrailMode { RGB, NeonPurple, NeonWhite, VelocityFlare, ThemeGradient, Count };

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------
static bool Check(HRESULT hr, const wchar_t* what) {
    if (SUCCEEDED(hr)) return true;
    wchar_t buf[192];
    swprintf_s(buf, L"TrailOverlay: %s failed (0x%08X)\n", what, static_cast<unsigned>(hr));
    OutputDebugStringW(buf);
    return false;
}

static void ShowMessage(const wchar_t* text, UINT icon) {
    MessageBoxW(nullptr, text, L"Trail Overlay", MB_OK | icon | MB_SETFOREGROUND);
}

static D2D1_COLOR_F HsvToRgb(float h, float s, float v, float a) {
    h = std::fmod(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    const float c = v * s;
    const float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;
    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }
    return D2D1::ColorF(r + m, g + m, b + m, a);
}

// ---- BEGIN GEOMETRY (pure math; no Windows dependencies beyond D2D1_POINT_2F) ----
constexpr float kPi          = 3.14159265358979f;
constexpr float kMiterLimit  = 2.0f;  
constexpr int   kCapSegments = 10;    
constexpr float kRadiusFraction = 0.7f;  
constexpr float kTaperSlope     = 1.0f;  
constexpr float kHeadHold       = 0.92f; 

inline float Clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
inline float SmoothStep(float e0, float e1, float x) {
    const float t = Clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

inline float TaperProfile(float life) { const float t = Clamp01(life / kHeadHold); return t * t; }

struct PathVertex { float x, y, life; };  

struct RibbonFrame {
    float x, y;          
    float nx, ny;        
    float tx, ty;        
    float widthFactor;   
    float invCos;        
    float cap;           
};

struct TrailGeometry {
    std::vector<PathVertex>      scratchA, scratchB, path;
    std::vector<D2D1_POINT_2F>   dirs;
    std::vector<float>           lens;
    std::vector<RibbonFrame>     frames;
    std::vector<D2D1_POINT_2F>   outline;

    static PathVertex Mix(const PathVertex& a, const PathVertex& b, float t) {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.life + (b.life - a.life) * t };
    }
    static float Dist(const PathVertex& a, const PathVertex& b) {
        const float dx = a.x - b.x, dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    static void Chaikin(const std::vector<PathVertex>& in, std::vector<PathVertex>& out) {
        out.clear();
        const size_t n = in.size();
        out.push_back(in.front());
        for (size_t i = 0; i + 1 < n; ++i) {
            if (i > 0)     out.push_back(Mix(in[i], in[i + 1], 0.25f));
            if (i + 2 < n) out.push_back(Mix(in[i], in[i + 1], 0.75f));
        }
        out.push_back(in.back());
    }

    bool Build(const std::vector<PathVertex>& src, float minSegment) {
        path.clear();
        frames.clear();
        if (src.size() < 2) return false;

        Chaikin(src, scratchA);
        Chaikin(scratchA, scratchB);
        Chaikin(scratchB, scratchA);

        path.push_back(scratchA.front());
        for (size_t i = 1; i < scratchA.size(); ++i) {
            const PathVertex& v = scratchA[i];
            const bool last = (i + 1 == scratchA.size());
            if (Dist(path.back(), v) >= minSegment) path.push_back(v);
            else if (last && path.size() > 1)       path.back() = v;  
        }
        if (path.size() < 2) return false;

        const size_t n = path.size();
        dirs.resize(n - 1);
        lens.resize(n - 1);
        for (size_t i = 0; i + 1 < n; ++i) {
            const float dx = path[i + 1].x - path[i].x, dy = path[i + 1].y - path[i].y;
            const float len = std::sqrt(dx * dx + dy * dy);
            lens[i] = len;
            if (len > 1e-4f)  dirs[i] = { dx / len, dy / len };
            else              dirs[i] = (i > 0) ? dirs[i - 1] : D2D1_POINT_2F{ 1.0f, 0.0f };
        }

        frames.resize(n);
        for (size_t i = 0; i < n; ++i) {
            RibbonFrame& f = frames[i];
            f.x = path[i].x;
            f.y = path[i].y;
            f.widthFactor = TaperProfile(path[i].life);
            f.invCos = 1.0f;
            f.cap = std::numeric_limits<float>::max();

            D2D1_POINT_2F t;
            if (i == 0)          t = dirs[0];
            else if (i == n - 1) t = dirs[n - 2];
            else {
                const D2D1_POINT_2F& d0 = dirs[i - 1];
                const D2D1_POINT_2F& d1 = dirs[i];
                const float sx = d0.x + d1.x, sy = d0.y + d1.y;
                const float sl = std::sqrt(sx * sx + sy * sy);
                const float cosHalf = 0.5f * sl;                       
                const float sinHalf = std::sqrt(std::max(0.0f, 1.0f - cosHalf * cosHalf));
                t = (sl < 1e-3f) ? d1 : D2D1_POINT_2F{ sx / sl, sy / sl };  
                f.invCos = std::min(1.0f / std::max(cosHalf, 1e-3f), kMiterLimit);
                f.cap = kRadiusFraction * 0.5f * std::min(lens[i - 1], lens[i]) / std::max(sinHalf, 1e-3f);
            }
            f.tx = t.x; f.ty = t.y;
            f.nx = -t.y; f.ny = t.x;
        }

        for (size_t i = 1; i < n; ++i)
            frames[i].cap = std::min(frames[i].cap, frames[i - 1].cap + kTaperSlope * lens[i - 1]);
        for (size_t i = n - 1; i-- > 0;)
            frames[i].cap = std::min(frames[i].cap, frames[i + 1].cap + kTaperSlope * lens[i]);
        return true;
    }

    void BuildOutline(float halfWidth) {
        outline.clear();
        const size_t n = frames.size();
        for (size_t i = 0; i < n; ++i) {
            const RibbonFrame& f = frames[i];
            const float o = std::min(halfWidth * f.widthFactor * f.invCos, f.cap);
            outline.push_back({ f.x + f.nx * o, f.y + f.ny * o });
        }
        const RibbonFrame& h = frames.back();
        const float r = std::min(halfWidth * h.widthFactor, h.cap);
        for (int k = 1; k < kCapSegments; ++k) {
            const float phi = kPi * static_cast<float>(k) / static_cast<float>(kCapSegments);
            const float c = std::cos(phi), s = std::sin(phi);
            outline.push_back({ h.x + (h.nx * c + h.tx * s) * r, h.y + (h.ny * c + h.ty * s) * r });
        }
        for (size_t i = n; i-- > 0;) {
            const RibbonFrame& f = frames[i];
            const float o = std::min(halfWidth * f.widthFactor * f.invCos, f.cap);
            outline.push_back({ f.x - f.nx * o, f.y - f.ny * o });
        }
    }

    void GradientAxis(float length, D2D1_POINT_2F& start, D2D1_POINT_2F& end) const {
        const PathVertex& head = path.back();
        start = { head.x, head.y };
        float acc = 0.0f;
        size_t j = path.size() - 1;
        while (j > 0 && acc < length) { acc += lens[j - 1]; --j; }
        float dx = path[j].x - head.x, dy = path[j].y - head.y;
        float d = std::sqrt(dx * dx + dy * dy);
        if (d < 1e-3f) { dx = -frames.back().tx; dy = -frames.back().ty; d = 1.0f; }
        end = { head.x + dx / d * length, head.y + dy / d * length };
    }

    void Bounds(float& x0, float& y0, float& x1, float& y1) const {
        x0 = x1 = path[0].x;
        y0 = y1 = path[0].y;
        for (const PathVertex& v : path) {
            x0 = std::min(x0, v.x); x1 = std::max(x1, v.x);
            y0 = std::min(y0, v.y); y1 = std::max(y1, v.y);
        }
    }
};
// ---- END GEOMETRY ----

// -----------------------------------------------------------------------------
// Mouse input: hook thread -> lock-free ring -> main thread
// -----------------------------------------------------------------------------
struct MouseSample {
    LONG    x, y;  
    int64_t t;     
};

class SampleRing {
public:
    bool Push(const MouseSample& s) noexcept {
        const size_t h = head_.load(std::memory_order_relaxed);
        const size_t n = (h + 1) % kCapacity;
        if (n == tail_.load(std::memory_order_acquire)) return false;
        buf_[h] = s;
        head_.store(n, std::memory_order_release);
        return true;
    }
    bool Pop(MouseSample& out) noexcept {
        const size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false;
        out = buf_[t];
        tail_.store((t + 1) % kCapacity, std::memory_order_release);
        return true;
    }
private:
    static constexpr size_t kCapacity = 4096;
    MouseSample buf_[kCapacity];
    std::atomic<size_t> head_{ 0 };
    std::atomic<size_t> tail_{ 0 };
};

static SampleRing         g_ring;
static HANDLE             g_wakeEvent = nullptr;  
static std::atomic<bool>  g_wakePending{ false };

static LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_MOUSEMOVE) {
        const auto* ms = reinterpret_cast<const MSLLHOOKSTRUCT*>(lParam);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        g_ring.Push({ ms->pt.x, ms->pt.y, now.QuadPart });
        if (!g_wakePending.exchange(true, std::memory_order_acq_rel)) SetEvent(g_wakeEvent);
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);  
}

class MouseHookThread {
public:
    bool Start(HINSTANCE hInst) {
        std::promise<bool> ready;
        std::future<bool> result = ready.get_future();
        thread_ = std::thread([this, hInst, p = std::move(ready)]() mutable {
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
            MSG msg;
            PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);  
            threadId_ = GetCurrentThreadId();
            HHOOK hook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, hInst, 0);
            p.set_value(hook != nullptr);
            if (!hook) return;
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            UnhookWindowsHookEx(hook);
        });
        return result.get();
    }
    void Stop() {
        if (!thread_.joinable()) return;
        PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
        thread_.join();
    }
private:
    std::thread thread_;
    DWORD threadId_ = 0;
};

// -----------------------------------------------------------------------------
// Visual language
// -----------------------------------------------------------------------------
struct Layer   { float width; D2D1_COLOR_F color; };  
struct Palette { Layer halo, body, core; bool gradientBody; bool speedGated; };

static Palette MakePalette(TrailMode mode, float hue) {
    switch (mode) {
    case TrailMode::RGB:
        return { { 20.0f, HsvToRgb(hue, 0.85f, 1.0f, 0.10f) },
                 {  9.0f, HsvToRgb(hue, 0.70f, 1.0f, 0.40f) },
                 {  3.5f, HsvToRgb(hue, 0.12f, 1.0f, 0.95f) }, false, false };
    case TrailMode::NeonWhite:
        return { { 18.0f, { 0.45f, 0.55f, 1.00f, 0.14f } },
                 {  7.0f, { 0.82f, 0.88f, 1.00f, 0.35f } },
                 {  3.0f, { 1.00f, 1.00f, 1.00f, 0.95f } }, false, false };
    case TrailMode::VelocityFlare:
        return { { 22.0f, { 1.00f, 0.32f, 0.05f, 0.12f } },
                 {  9.0f, { 1.00f, 0.50f, 0.10f, 0.45f } },
                 {  3.5f, { 1.00f, 0.93f, 0.78f, 0.95f } }, false, true };
    case TrailMode::ThemeGradient:
        return { { 20.0f, { 0.74f, 0.58f, 0.98f, 0.11f } },
                 {  9.0f, { 1.00f, 1.00f, 1.00f, 1.00f } },   
                 {  3.5f, { 0.92f, 0.98f, 1.00f, 0.92f } }, true, false };
    case TrailMode::NeonPurple:
    default:
        return { { 22.0f, { 0.55f, 0.15f, 1.00f, 0.11f } },
                 {  9.0f, { 0.62f, 0.30f, 1.00f, 0.40f } },
                 {  3.5f, { 0.96f, 0.88f, 1.00f, 0.95f } }, false, false };
    }
}

// -----------------------------------------------------------------------------
// Application
// -----------------------------------------------------------------------------
struct TrailPoint { float x, y; int64_t t; };  

class TrailApp {
public:
    explicit TrailApp(HINSTANCE hInstance) : hInst_(hInstance) {}
    ~TrailApp() { Shutdown(); }
    TrailApp(const TrailApp&) = delete;
    TrailApp& operator=(const TrailApp&) = delete;

    bool Initialize(const std::wstring& pipeName) {
        pipeName_ = pipeName;
        managedMode_ = !pipeName_.empty();
        featureEnabled_ = !managedMode_; // In managed mode, start dormant until activated

        if (managedMode_) {
            ipc_ = std::make_unique<utilist::protocol::PipeChannel>();
            if (!ipc_->connectToServer(pipeName_, 5000)) {
                OutputDebugStringW(L"TrailOverlay: Failed to connect to UTILIST pipe.\n");
                return false; 
            }
        }

        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        freq_ = f.QuadPart;

        g_wakeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!g_wakeEvent) { ShowMessage(L"Could not create the input wake event.", MB_ICONERROR); return false; }

        int l, t, w, h;
        QueryVirtualScreen(l, t, w, h);
        if (w <= 0 || h <= 0 || w > static_cast<int>(cfg::kMaxSurfacePx) || h > static_cast<int>(cfg::kMaxSurfacePx)) {
            ShowMessage(L"The combined desktop size is outside what the GPU can render as one surface "
                        L"(16384 px per side).", MB_ICONERROR);
            return false;
        }
        vLeft_ = l; vTop_ = t; surfaceW_ = w; surfaceH_ = h;

        WNDCLASSW wc{};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst_;
        wc.lpszClassName = kClassName;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        if (!RegisterClassW(&wc)) { ShowMessage(L"Could not register the overlay window class.", MB_ICONERROR); return false; }
        classRegistered_ = true;

        hwnd_ = CreateWindowExW(
            WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST |
                WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kClassName, L"", WS_POPUP, vLeft_, vTop_, surfaceW_, surfaceH_,
            nullptr, nullptr, hInst_, this);
        if (!hwnd_) { ShowMessage(L"Could not create the overlay window.", MB_ICONERROR); return false; }

        if (!managedMode_) {
            hotkeyExit_ = RegisterHotKey(hwnd_, HK_EXIT, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'E') != FALSE;
            if (!hotkeyExit_) {
                ShowMessage(L"Ctrl+Alt+E could not be registered. Another application (or another copy of "
                            L"Trail Overlay) is already using it, so Trail Overlay will not start.", MB_ICONERROR);
                return false;
            }
            hotkeyMode_ = RegisterHotKey(hwnd_, HK_MODE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'M') != FALSE;
            if (!hotkeyMode_) {
                ShowMessage(L"Ctrl+Alt+M could not be registered (already in use). The trail will run in its "
                            L"default mode; Ctrl+Alt+E still exits.", MB_ICONWARNING);
            }
        }

        if (!CreateGraphics()) {
            ShowMessage(L"Could not initialise Direct3D / Direct2D / DirectComposition. "
                        L"Update your graphics driver and try again.", MB_ICONERROR);
            return false;
        }
        if (!hook_.Start(hInst_)) {
            ShowMessage(L"Could not install the mouse hook.", MB_ICONERROR);
            return false;
        }
        hookStarted_ = true;

        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        ready_ = true;
        return true;
    }

    void PollIPC() {
        if (!managedMode_ || !ipc_ || !ipc_->isConnected()) return;

        auto msgs = ipc_->pollMessages();
        for (const auto& msgJson : msgs) {
            auto reqOpt = utilist::protocol::JsonHelper::parseRequest(msgJson);
            if (!reqOpt) continue;

            const auto& req = *reqOpt;
            utilist::protocol::Response res;
            res.requestId = req.requestId;
            res.type = "response";
            res.success = true;

            const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime_).count();

            utilist::protocol::StatusPayload status;
            status.featureActive = featureEnabled_;
            status.stateDescription = featureEnabled_ ? "Trail active" : "Trail dormant";
            status.capabilities = {"status", "activate", "deactivate", "toggle", "settings"};
            status.uptimeSeconds = static_cast<uint64_t>(uptime);

            if (req.command == "handshake") {
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "get_status") {
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "activate") {
                featureEnabled_ = true;
                points_.clear();
                status.featureActive = true;
                status.stateDescription = "Trail active";
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "deactivate") {
                featureEnabled_ = false;
                points_.clear();
                status.featureActive = false;
                status.stateDescription = "Trail dormant";
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "toggle") {
                featureEnabled_ = !featureEnabled_;
                if (!featureEnabled_) points_.clear();
                status.featureActive = featureEnabled_;
                status.stateDescription = featureEnabled_ ? "Trail active" : "Trail dormant";
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "update_settings") {
                CycleMode();
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
            } else if (req.command == "shutdown") {
                status.stateDescription = "Shutting down";
                res.payloadJson = utilist::protocol::JsonHelper::serializeStatusPayload(status);
                ipc_->sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
                running_ = false;
                PostQuitMessage(0);
                break;
            } else {
                res.success = false;
                res.error = utilist::protocol::ErrorInfo{"UNSUPPORTED_COMMAND", "Command not recognized: " + req.command};
            }

            ipc_->sendMessage(utilist::protocol::JsonHelper::serializeResponse(res));
        }
    }

    void Run() {
        while (running_) {
            HANDLE wait = g_wakeEvent;
            DWORD timeout = (managedMode_ && !active_) ? 100 : INFINITE; 
            if (graphicsLost_)                              timeout = 500;  
            else if (active_ && frameWaitable_ && !presentFailed_) { wait = frameWaitable_; timeout = 100; }
            else if (active_)                               timeout = 16;   

            MsgWaitForMultipleObjectsEx(1, &wait, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) running_ = false;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (!running_) break;

            if (graphicsLost_ && !RecoverGraphics()) continue;
            Tick();
        }
    }

private:
    // UTILIST Integration
    bool managedMode_ = false;
    bool featureEnabled_ = true; 
    std::unique_ptr<utilist::protocol::PipeChannel> ipc_;
    std::wstring pipeName_;
    std::chrono::steady_clock::time_point startTime_{std::chrono::steady_clock::now()};

    static constexpr const wchar_t* kClassName = L"TrailOverlayRibbonClass";

    // ---- state -------------------------------------------------------------
    HINSTANCE hInst_;
    HWND hwnd_ = nullptr;
    bool classRegistered_ = false, hotkeyMode_ = false, hotkeyExit_ = false, hookStarted_ = false;
    bool ready_ = false, running_ = true;
    TrailMode mode_ = TrailMode::NeonPurple;

    int64_t freq_ = 1;
    int vLeft_ = 0, vTop_ = 0, surfaceW_ = 0, surfaceH_ = 0;

    MouseHookThread hook_;

    // Input / trail data
    std::deque<TrailPoint> points_;
    TrailPoint last_{};            
    bool haveLast_ = false;
    LONG lastScreenX_ = 0, lastScreenY_ = 0;
    std::vector<PathVertex> src_;
    TrailGeometry geometry_;
    float speed_ = 0.0f, hue_ = 0.0f, dpiScale_ = 1.0f;
    bool dpiValid_ = false;
    int64_t prevTick_ = 0;

    // Frame state
    bool active_ = false;          
    bool graphicsLost_ = false, presentFailed_ = false;
    bool fullPresent_ = true, hasPrev_ = false;
    RECT prevRect_{};

    // Graphics
    ComPtr<ID3D11Device>           d3dDevice_;
    ComPtr<IDXGIDevice>            dxgiDevice_;
    ComPtr<IDXGIFactory2>          dxgiFactory_;
    ComPtr<IDXGISwapChain2>        swapChain_;
    HANDLE                         frameWaitable_ = nullptr;
    ComPtr<IDCompositionDevice>    dcompDevice_;
    ComPtr<IDCompositionTarget>    dcompTarget_;
    ComPtr<IDCompositionVisual>    dcompVisual_;
    ComPtr<ID2D1Factory1>          d2dFactory_;   
    ComPtr<ID2D1Device>            d2dDevice_;
    ComPtr<ID2D1DeviceContext>     d2dContext_;
    ComPtr<ID2D1Bitmap1>           targetBitmap_;
    ComPtr<ID2D1SolidColorBrush>   solidBrush_;
    ComPtr<ID2D1GradientStopCollection> gradientStops_;
    ComPtr<ID2D1LinearGradientBrush>    gradientBrush_;

    // ---- screen / graphics lifetime ---------------------------------------
    static void QueryVirtualScreen(int& l, int& t, int& w, int& h) {
        l = GetSystemMetrics(SM_XVIRTUALSCREEN);
        t = GetSystemMetrics(SM_YVIRTUALSCREEN);
        w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }

    bool CreateTargetBitmap() {
        ComPtr<IDXGISurface> surface;
        if (!Check(swapChain_->GetBuffer(0, IID_PPV_ARGS(&surface)), L"GetBuffer")) return false;
        const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96.0f, 96.0f);
        if (!Check(d2dContext_->CreateBitmapFromDxgiSurface(surface.Get(), &props, &targetBitmap_),
                   L"CreateBitmapFromDxgiSurface")) return false;
        d2dContext_->SetTarget(targetBitmap_.Get());
        return true;
    }

    bool CreateBrushes() {
        if (!Check(d2dContext_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &solidBrush_), L"CreateSolidColorBrush"))
            return false;
        D2D1_GRADIENT_STOP stops[3];
        stops[0] = { 0.0f, D2D1::ColorF(0.545f, 0.914f, 0.992f, 0.95f) };
        stops[1] = { 0.5f, D2D1::ColorF(0.650f, 0.700f, 0.980f, 0.75f) };
        stops[2] = { 1.0f, D2D1::ColorF(0.741f, 0.576f, 0.976f, 0.25f) };
        if (!Check(d2dContext_->CreateGradientStopCollection(stops, 3, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &gradientStops_),
                   L"CreateGradientStopCollection")) return false;
        return Check(d2dContext_->CreateLinearGradientBrush(
                         D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(1, 0)),
                         gradientStops_.Get(), &gradientBrush_), L"CreateLinearGradientBrush");
    }

    bool CreateGraphics() {
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                       nullptr, 0, D3D11_SDK_VERSION, &d3dDevice_, nullptr, nullptr);
        if (FAILED(hr)) {
            Check(hr, L"D3D11CreateDevice (hardware)");
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   nullptr, 0, D3D11_SDK_VERSION, &d3dDevice_, nullptr, nullptr);
        }
        if (!Check(hr, L"D3D11CreateDevice")) return false;
        if (!Check(d3dDevice_.As(&dxgiDevice_), L"QueryInterface(IDXGIDevice)")) return false;

        ComPtr<IDXGIAdapter> adapter;
        if (!Check(dxgiDevice_->GetAdapter(&adapter), L"GetAdapter")) return false;
        if (!Check(adapter->GetParent(IID_PPV_ARGS(&dxgiFactory_)), L"GetParent(IDXGIFactory2)")) return false;

        if (!d2dFactory_ &&
            !Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf()), L"D2D1CreateFactory"))
            return false;
        if (!Check(d2dFactory_->CreateDevice(dxgiDevice_.Get(), &d2dDevice_), L"ID2D1Factory1::CreateDevice")) return false;
        if (!Check(d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d2dContext_), L"CreateDeviceContext"))
            return false;
        d2dContext_->SetDpi(96.0f, 96.0f);

        DXGI_SWAP_CHAIN_DESC1 sc = {};
        sc.Width = surfaceW_;
        sc.Height = surfaceH_;
        sc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sc.SampleDesc.Count = 1;
        sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sc.BufferCount = 2;
        sc.Scaling = DXGI_SCALING_STRETCH;
        sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        sc.Flags = kSwapChainFlags;
        ComPtr<IDXGISwapChain1> sc1;
        if (!Check(dxgiFactory_->CreateSwapChainForComposition(dxgiDevice_.Get(), &sc, nullptr, &sc1),
                   L"CreateSwapChainForComposition")) return false;
        if (!Check(sc1.As(&swapChain_), L"QueryInterface(IDXGISwapChain2)")) return false;
        Check(swapChain_->SetMaximumFrameLatency(1), L"SetMaximumFrameLatency");
        frameWaitable_ = swapChain_->GetFrameLatencyWaitableObject();

        if (!CreateTargetBitmap()) return false;

        if (!Check(DCompositionCreateDevice(dxgiDevice_.Get(), IID_PPV_ARGS(&dcompDevice_)), L"DCompositionCreateDevice")) return false;
        if (!Check(dcompDevice_->CreateTargetForHwnd(hwnd_, TRUE, &dcompTarget_), L"CreateTargetForHwnd")) return false;
        if (!Check(dcompDevice_->CreateVisual(&dcompVisual_), L"CreateVisual")) return false;
        if (!Check(dcompVisual_->SetContent(swapChain_.Get()), L"SetContent")) return false;
        if (!Check(dcompTarget_->SetRoot(dcompVisual_.Get()), L"SetRoot")) return false;
        if (!Check(dcompDevice_->Commit(), L"Commit")) return false;

        if (!CreateBrushes()) return false;

        fullPresent_ = true;
        hasPrev_ = false;
        presentFailed_ = false;
        return true;
    }

    void ReleaseGraphics() {
        if (d2dContext_) d2dContext_->SetTarget(nullptr);
        gradientBrush_.Reset(); gradientStops_.Reset(); solidBrush_.Reset();
        targetBitmap_.Reset(); d2dContext_.Reset(); d2dDevice_.Reset();
        dcompVisual_.Reset(); dcompTarget_.Reset(); dcompDevice_.Reset();
        if (frameWaitable_) { CloseHandle(frameWaitable_); frameWaitable_ = nullptr; }
        swapChain_.Reset(); dxgiFactory_.Reset(); dxgiDevice_.Reset(); d3dDevice_.Reset();
    }

    bool RecoverGraphics() {
        OutputDebugStringW(L"TrailOverlay: recreating graphics resources\n");
        ReleaseGraphics();
        if (!CreateGraphics()) { ReleaseGraphics(); return false; }
        graphicsLost_ = false;
        return true;
    }

    bool ResizeSurface() {
        d2dContext_->SetTarget(nullptr);
        targetBitmap_.Reset();
        if (!Check(swapChain_->ResizeBuffers(0, surfaceW_, surfaceH_, DXGI_FORMAT_UNKNOWN, kSwapChainFlags), L"ResizeBuffers"))
            return false;
        fullPresent_ = true;
        hasPrev_ = false;
        return CreateTargetBitmap();
    }

    void OnDisplayChange() {
        if (!ready_) return;
        int l, t, w, h;
        QueryVirtualScreen(l, t, w, h);
        if (w <= 0 || h <= 0 || w > static_cast<int>(cfg::kMaxSurfacePx) || h > static_cast<int>(cfg::kMaxSurfacePx)) return;
        const bool sizeChanged = (w != surfaceW_ || h != surfaceH_);
        vLeft_ = l; vTop_ = t; surfaceW_ = w; surfaceH_ = h;
        points_.clear();          
        haveLast_ = false;
        SetWindowPos(hwnd_, HWND_TOPMOST, vLeft_, vTop_, surfaceW_, surfaceH_, SWP_NOACTIVATE);
        if (sizeChanged && swapChain_ && !ResizeSurface()) graphicsLost_ = true;
        fullPresent_ = true;
        hasPrev_ = false;
    }

    void CycleMode() {
        mode_ = static_cast<TrailMode>((static_cast<int>(mode_) + 1) % static_cast<int>(TrailMode::Count));
    }

    void Shutdown() {
        if (hookStarted_) { hook_.Stop(); hookStarted_ = false; }
        if (hwnd_) {
            if (hotkeyMode_) UnregisterHotKey(hwnd_, HK_MODE);
            if (hotkeyExit_) UnregisterHotKey(hwnd_, HK_EXIT);
            hotkeyMode_ = hotkeyExit_ = false;
        }
        ReleaseGraphics();  
        if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
        if (classRegistered_) { UnregisterClassW(kClassName, hInst_); classRegistered_ = false; }
        if (g_wakeEvent) { CloseHandle(g_wakeEvent); g_wakeEvent = nullptr; }
        ready_ = false;
    }

    // ---- per-frame logic ----------------------------------------------------
    static int64_t QpcNow() {
        LARGE_INTEGER q;
        QueryPerformanceCounter(&q);
        return q.QuadPart;
    }
    double TicksToMs(int64_t ticks) const { return static_cast<double>(ticks) * 1000.0 / static_cast<double>(freq_); }
    static float Dist(float ax, float ay, float bx, float by) {
        const float dx = ax - bx, dy = ay - by;
        return std::sqrt(dx * dx + dy * dy);
    }

    void DrainSamples() {
        g_wakePending.exchange(false, std::memory_order_acq_rel);
        const float minDist = cfg::kMinPointDistPx * dpiScale_;
        MouseSample s;
        while (g_ring.Pop(s)) {
            const TrailPoint tp{ static_cast<float>(s.x - vLeft_), static_cast<float>(s.y - vTop_), s.t };
            if (haveLast_ && TicksToMs(s.t - last_.t) < 100.0 &&
                Dist(last_.x, last_.y, tp.x, tp.y) > cfg::kTeleportPx * dpiScale_) {
                points_.clear();
            }
            last_ = tp;
            haveLast_ = true;
            lastScreenX_ = s.x;
            lastScreenY_ = s.y;
            if (points_.empty() || Dist(points_.back().x, points_.back().y, tp.x, tp.y) >= minDist) {
                points_.push_back(tp);
                if (points_.size() > cfg::kMaxPoints) points_.pop_front();
            }
        }
    }

    void PruneOldPoints(int64_t now) {
        const int64_t tailTicks = static_cast<int64_t>(cfg::kTailMs * static_cast<double>(freq_) / 1000.0);
        while (!points_.empty() && now - points_.front().t > tailTicks) points_.pop_front();
    }

    void UpdateDpiScale(float dt) {
        if (!haveLast_) return;
        const POINT pt{ lastScreenX_, lastScreenY_ };
        UINT dx = 96, dy = 96;
        if (FAILED(GetDpiForMonitor(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), MDT_EFFECTIVE_DPI, &dx, &dy))) return;
        const float target = static_cast<float>(dx) / 96.0f;
        if (!dpiValid_) { dpiScale_ = target; dpiValid_ = true; }
        else dpiScale_ += (target - dpiScale_) * (1.0f - std::exp(-dt / 0.08f));  
    }

    void UpdateSpeed(int64_t now, float dt) {
        float len = 0.0f;
        if (haveLast_) {
            const int64_t window = static_cast<int64_t>(cfg::kSpeedWindowSec * static_cast<float>(freq_));
            float cx = last_.x, cy = last_.y;
            for (auto it = points_.rbegin(); it != points_.rend() && now - it->t <= window; ++it) {
                len += Dist(cx, cy, it->x, it->y);
                cx = it->x; cy = it->y;
            }
        }
        const float inst = len / cfg::kSpeedWindowSec;
        speed_ += (inst - speed_) * (1.0f - std::exp(-dt / cfg::kSpeedSmoothSec));
    }

    void BuildSource(int64_t now) {
        src_.clear();
        const double tailTicks = cfg::kTailMs * static_cast<double>(freq_) / 1000.0;
        auto lifeOf = [&](int64_t t) {
            return Clamp01(static_cast<float>(1.0 - static_cast<double>(now - t) / tailTicks));
        };
        for (const TrailPoint& p : points_) src_.push_back({ p.x, p.y, lifeOf(p.t) });
        if (haveLast_ && !points_.empty() && static_cast<double>(now - last_.t) <= tailTicks &&
            Dist(last_.x, last_.y, points_.back().x, points_.back().y) > 0.01f) {
            src_.push_back({ last_.x, last_.y, lifeOf(last_.t) });
        }
    }

    void Tick() {
        PollIPC(); 

        const int64_t now = QpcNow();
        float dt = prevTick_ ? static_cast<float>(static_cast<double>(now - prevTick_) / static_cast<double>(freq_))
                             : (1.0f / 60.0f);
        dt = std::clamp(dt, 0.001f, 0.1f);
        prevTick_ = now;

        DrainSamples();
        PruneOldPoints(now);
        UpdateDpiScale(dt);
        UpdateSpeed(now, dt);
        hue_ = std::fmod(hue_ + cfg::kHueDegPerSec * dt, 360.0f);

        Render(now);

        if (!active_) { prevTick_ = 0; speed_ = 0.0f; }  
    }

    // ---- drawing ------------------------------------------------------------
    HRESULT FillRibbon(float halfWidth, ID2D1Brush* brush) {
        geometry_.BuildOutline(halfWidth);
        ComPtr<ID2D1PathGeometry> geom;
        HRESULT hr = d2dFactory_->CreatePathGeometry(&geom);
        if (FAILED(hr)) return hr;
        ComPtr<ID2D1GeometrySink> sink;
        hr = geom->Open(&sink);
        if (FAILED(hr)) return hr;
        sink->SetFillMode(D2D1_FILL_MODE_WINDING);
        sink->BeginFigure(geometry_.outline.front(), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLines(geometry_.outline.data() + 1, static_cast<UINT32>(geometry_.outline.size() - 1));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        hr = sink->Close();
        if (FAILED(hr)) return hr;
        d2dContext_->FillGeometry(geom.Get(), brush);
        return S_OK;
    }

    static RECT UnionRect(const RECT& a, const RECT& b) {
        return { std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right), std::max(a.bottom, b.bottom) };
    }

    bool PresentFrame(bool full, const RECT& dirty) {
        DXGI_PRESENT_PARAMETERS pp = {};
        if (!full) { pp.DirtyRectsCount = 1; pp.pDirtyRects = const_cast<RECT*>(&dirty); }
        const HRESULT hr = swapChain_->Present1(1, 0, &pp);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) { graphicsLost_ = true; return false; }
        return Check(hr, L"Present1");
    }

    void Render(int64_t now) {
        if (graphicsLost_ || !d2dContext_ || !swapChain_) { active_ = false; return; }

        BuildSource(now);
        const bool havePath = geometry_.Build(src_, cfg::kMinSegmentPx * dpiScale_);

        const Palette pal = MakePalette(mode_, hue_);
        const float speedN = Clamp01(speed_ / (cfg::kSpeedRefPxSec * dpiScale_));
        const float gate = pal.speedGated ? SmoothStep(0.03f, 0.45f, speedN) : 1.0f;
        const float headLife = havePath ? geometry_.path.back().life : 0.0f;
        const float alphaMul = (0.85f + 0.15f * speedN) * gate * SmoothStep(0.0f, 0.55f, headLife);
        const float widthMul = (0.88f + 0.24f * speedN) * (pal.speedGated ? 0.55f + 0.45f * gate : 1.0f);
        
        // Gate rendering based on UTILIST feature enablement
        const bool drawable = featureEnabled_ && havePath && alphaMul > 0.01f;

        if (!drawable && !hasPrev_ && !fullPresent_) { active_ = false; return; }

        RECT cur{};
        d2dContext_->BeginDraw();
        d2dContext_->Clear(D2D1::ColorF(0, 0, 0, 0));

        if (drawable) {
            const Layer layers[3] = { pal.halo, pal.body, pal.core };
            for (int i = 0; i < 3; ++i) {
                const float half = 0.5f * layers[i].width * dpiScale_ * widthMul;
                ID2D1Brush* brush;
                if (i == 1 && pal.gradientBody) {
                    D2D1_POINT_2F a, b;
                    geometry_.GradientAxis(cfg::kGradientLenPx * dpiScale_, a, b);
                    gradientBrush_->SetStartPoint(a);
                    gradientBrush_->SetEndPoint(b);
                    gradientBrush_->SetOpacity(alphaMul);
                    brush = gradientBrush_.Get();
                } else {
                    D2D1_COLOR_F c = layers[i].color;
                    c.a *= alphaMul;
                    solidBrush_->SetColor(c);
                    brush = solidBrush_.Get();
                }
                Check(FillRibbon(half, brush), L"FillRibbon");
            }
            float x0, y0, x1, y1;
            geometry_.Bounds(x0, y0, x1, y1);
            const float margin = 0.5f * pal.halo.width * dpiScale_ * widthMul * kMiterLimit + 4.0f;
            cur.left   = std::max(0, static_cast<int>(std::floor(x0 - margin)));
            cur.top    = std::max(0, static_cast<int>(std::floor(y0 - margin)));
            cur.right  = std::min(surfaceW_, static_cast<int>(std::ceil(x1 + margin)));
            cur.bottom = std::min(surfaceH_, static_cast<int>(std::ceil(y1 + margin)));
        }

        const HRESULT hr = d2dContext_->EndDraw();
        if (hr == D2DERR_RECREATE_TARGET) { graphicsLost_ = true; active_ = false; return; }
        if (!Check(hr, L"EndDraw")) { presentFailed_ = true; fullPresent_ = true; active_ = true; return; }

        RECT dirty = cur;
        if (drawable && hasPrev_)  dirty = UnionRect(cur, prevRect_);
        else if (hasPrev_)         dirty = prevRect_;

        if (!PresentFrame(fullPresent_, dirty)) {
            presentFailed_ = !graphicsLost_;
            fullPresent_ = true;
            active_ = !graphicsLost_;
            return;
        }
        presentFailed_ = false;
        fullPresent_ = false;
        hasPrev_ = drawable;
        if (drawable) prevRect_ = cur;
        active_ = drawable;  
    }

    // ---- window procedure ---------------------------------------------------
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            const auto* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        auto* app = reinterpret_cast<TrailApp*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (msg) {
        case WM_HOTKEY:
            if (app) {
                if (wParam == HK_MODE) app->CycleMode();
                else if (wParam == HK_EXIT) PostQuitMessage(0);
            }
            return 0;
        case WM_DISPLAYCHANGE:
            if (app) app->OnDisplayChange();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

// -----------------------------------------------------------------------------
static void EnableDpiAwareness() {
    using SetCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto fn = user32 ? reinterpret_cast<SetCtxFn>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")) : nullptr;
    if (!(fn && fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))) SetProcessDPIAware();
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    EnableDpiAwareness();

    std::wstring pipeName;
    int argc;
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

    TrailApp app(hInstance);
    if (!app.Initialize(pipeName)) return 1;
    app.Run();
    return 0;
}