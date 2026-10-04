#include "dpi.h"
#include <windows.h>

// ---------------------------------------------------------------------------
// 兼容性垫片
// 这个 MinGW 的 windows.h 里没有 Per-Monitor V2 那套 API 和类型，
// 但它们本来就存在于系统的 user32 / shcore 里，所以这里自己声明。
// 这样就不必依赖 SDK 版本，也不必改 _WIN32_WINNT。
// ---------------------------------------------------------------------------
namespace {

// 取自 winuser.h：DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
// 它是一个 -4 的伪句柄，通过值传递而不是真的指针。
#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE)(LONG_PTR)-4)
#endif

typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFn)(HANDLE);

// DPI_AWARENESS_CONTEXT。声明为 void*，SetProcessDpiAwareness 只认这个枚举值。
typedef enum {
    kProcessDpiAwarenessInvalid = -1,
    kProcessDpiAwarenessUnaware = 0,
    kProcessDpiAwarenessSystemAware = 1,
    kProcessDpiAwarenessPerMonitorAware = 2
} ProcessDpiAwarenessShim;

typedef HRESULT (WINAPI *SetProcessDpiAwarenessFn)(ProcessDpiAwarenessShim);

// 旧 API，作为最后兜底
extern "C" WINUSERAPI BOOL WINAPI SetProcessDPIAware(void);

// 声明进程为 Per-Monitor V2 感知（Win10 1703+）。
// 拿不到就退到 Per-Monitor（Win8.1+），再退到系统感知（Vista+）。
void enablePerMonitorV2() {
    // 1) 最理想：Per-Monitor V2。只有它才会在跨显示器时收到 WM_DPICHANGED。
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        SetProcessDpiAwarenessContextFn setCtx =
            reinterpret_cast<SetProcessDpiAwarenessContextFn>(
                reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
        if (setCtx && setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            return;
        }
    }

    // 2) 退一步：Per-Monitor（Win8.1 的 shcore）
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        SetProcessDpiAwarenessFn setAware =
            reinterpret_cast<SetProcessDpiAwarenessFn>(
                reinterpret_cast<void*>(GetProcAddress(shcore, "SetProcessDpiAwareness")));
        if (setAware) {
            HRESULT hr = setAware(kProcessDpiAwarenessPerMonitorAware);
            // S_OK 或 E_ACCESSDENIED（已经设过了）都算可用，不再往下退
            if (SUCCEEDED(hr) || hr == E_ACCESSDENIED) return;
        }
    }

    // 3) 兜底：系统 DPI 感知。跨显示器不会重排，但至少比例正确
    SetProcessDPIAware();
}

} // namespace

namespace dpi {

namespace {

int   g_dpi = 96;
float g_factor = 1.0f;
bool  g_inited = false;

void recompute() {
    g_factor = (float)g_dpi / 96.0f;
}

} // namespace

void init() {
    if (g_inited) return;
    g_inited = true;

    // 必须是第一件事：一旦创建过窗口，DPI 感知模式就锁死了
    enablePerMonitorV2();

    // 取屏幕 DC 的 DPI 作为初值。窗口建好之后，真正的每显示器 DPI
    // 由 WM_DPICHANGED 送过来（Per-Monitor V2 下建窗时也会立刻收到一次）。
    refresh();
}

void refresh() {
    HDC screen = GetDC(nullptr);
    if (screen) {
        int d = GetDeviceCaps(screen, LOGPIXELSX);
        ReleaseDC(nullptr, screen);
        if (d >= 72 && d <= 480) g_dpi = d;
    }
    recompute();
}

void setDpi(int newDpi) {
    // WM_DPICHANGED 的 wParam 高位/低位都是新 DPI，比再查一次更准
    if (newDpi >= 72 && newDpi <= 480) {
        g_dpi = newDpi;
        recompute();
    }
}

int dpi() {
    return g_dpi;
}

float factor() {
    return g_factor;
}

int scale(int logicalPixels) {
    return (int)(logicalPixels * g_factor + 0.5f);
}

float scale(float logicalPixels) {
    return logicalPixels * g_factor;
}

} // namespace dpi
