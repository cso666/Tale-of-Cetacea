#include "whitewin.h"
#include "wintext.h"
#include "layout.h"
#include "dpi.h"

namespace whitewin {

namespace {

HWND g_wnd = nullptr;
bool g_thumbDrag = false;   // 是否正在拖滑块
int  g_dragGrabDy = 0;      // 按住滑块时，鼠标相对滑块顶端的偏移

// 双缓冲把整块客户区画好再一次性贴上去。
// 直接用屏幕 DC + GDI+ 会闪，而且"白底 + 正文"要作为一个整体呈现。
void paintTo(HDC hdc, int w, int h) {
    if (w <= 0 || h <= 0) return;

    HDC memDC = CreateCompatibleDC(hdc);
    if (!memDC) return;

    HBITMAP hBmp = CreateCompatibleBitmap(hdc, w, h);
    if (!hBmp) {
        DeleteDC(memDC);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    // 铺纯白底
    RECT rc = {0, 0, w, h};
    FillRect(memDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    // 正文 + 滑块
    wintext::draw(memDC, rc);

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
}

void redraw(HWND hwnd) {
    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);
}

// 从鼠标消息的 lParam 取客户区坐标。
// GET_X_LPARAM / GET_Y_LPARAM 声明在 <windowsx.h> 里，为了不把那一整包
// 旧宏（SelectFont / SetWindowFont 之类）拖进来，这里自己按位拆。
// 必须走 (short) 再转 int：坐标可以为负，直接取低位会得到很大的正数。
POINT clientPointFromLParam(LPARAM lParam) {
    POINT pt;
    pt.x = (int)(short)LOWORD(lParam);
    pt.y = (int)(short)HIWORD(lParam);
    return pt;
}

LRESULT CALLBACK BlankWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        // 交给 WM_PAINT 的双缓冲一次画完，避免先白后字的闪烁
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        paintTo(hdc, rc.right - rc.left, rc.bottom - rc.top);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        if (notches == 0) {
            notches = (GET_WHEEL_DELTA_WPARAM(wParam) > 0) ? 1 : -1;
        }
        if (wintext::scrollBy(-notches * dpi::scale(layout::kWheelScrollStep)))
            redraw(hwnd);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = clientPointFromLParam(lParam);
        wintext::ScrollBar sb = wintext::scrollBar();
        if (!sb.visible) return 0;

        // 点在滑块上 → 开始拖动
        if (pt.x >= sb.thumb.left && pt.x <= sb.thumb.right &&
            pt.y >= sb.thumb.top && pt.y <= sb.thumb.bottom) {
            g_thumbDrag = true;
            g_dragGrabDy = pt.y - sb.thumb.top;
            SetCapture(hwnd);
            return 0;
        }

        // 点在轨道空白处 → 上下翻一页
        int dir = wintext::hitTrack(pt.y);
        if (dir != 0) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int page = (rc.bottom - rc.top) - dpi::scale(layout::kWindowTextPad) * 2;
            if (page < dpi::scale(layout::kWheelScrollStep))
                page = dpi::scale(layout::kWheelScrollStep);
            if (wintext::scrollBy(dir * page)) redraw(hwnd);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!g_thumbDrag) return 0;
        POINT pt = clientPointFromLParam(lParam);
        wintext::scrollFromThumbTop(pt.y - g_dragGrabDy);
        redraw(hwnd);
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_thumbDrag) {
            g_thumbDrag = false;
            ReleaseCapture();
        }
        return 0;
    }

    case WM_DPICHANGED: {
        // 窗口被拖到了另一块缩放比例不同的显示器上。
        // wParam 低 16 位是新 DPI，lParam 是系统建议的新窗口矩形
        // （已经按新 DPI 换算过，直接用能避免我们自己算错边框尺寸）。
        dpi::setDpi((int)LOWORD(wParam));

        const RECT* suggested = (const RECT*)lParam;
        if (suggested) {
            SetWindowPos(hwnd, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }

        // 重画即可：正文排版、字号、留白、滑块尺寸全都是绘制时按
        // dpi::scale() 现算的，所以不需要另外重建任何东西。
        redraw(hwnd);
        return 0;
    }

    case WM_CLOSE:
        // 只关掉本窗口。桌宠是独立窗口，不受影响，程序继续运行。
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        // 这里千万不能 PostQuitMessage —— 那样会连带把桌宠一起结束掉。
        // 退出程序的职责归桌宠窗口。
        g_wnd = nullptr;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace

HWND createWindow(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = BlankWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DeepSeekFishBlank";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    if (!RegisterClassW(&wc)) return nullptr;

    // 把"期望的客户区尺寸"换算成"包含标题栏和边框的外框尺寸"。
    // 直接把客户区尺寸传给 CreateWindowExW 是常见错误：那样得到的客户区
    // 会小掉标题栏加边框的量，画出来的区域就不是 800x900 个像素点了。
    //
    // 再过一遍 DPI 缩放：800x900 是按 96 DPI（100% 缩放）设计的逻辑尺寸，
    // 在 125% / 150% 缩放下按比例放大，视觉大小才一致。
    RECT rc = {0, 0, dpi::scale(layout::kMainW), dpi::scale(layout::kMainH)};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    // WS_OVERLAPPEDWINDOW = 普通窗口：标题栏 + 可缩放 + 最小化/最大化/关闭
    g_wnd = CreateWindowExW(
        0,
        L"DeepSeekFishBlank",
        L"DeepSeek Fish",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        winW, winH,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_wnd) return nullptr;

    ShowWindow(g_wnd, nCmdShow);
    UpdateWindow(g_wnd);
    // 滚轮消息只发给有焦点的窗口，主动拿一下焦点
    SetFocus(g_wnd);
    return g_wnd;
}

} // namespace whitewin
