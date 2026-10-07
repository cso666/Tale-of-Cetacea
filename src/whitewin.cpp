#include "whitewin.h"
#include "wintext.h"
#include "layout.h"
#include "dpi.h"
#include "settings.h"
#include "ui.h"

#include <gdiplus.h>

using namespace Gdiplus;

namespace whitewin {

namespace {

HWND        g_wnd = nullptr;
HINSTANCE   g_inst = nullptr;   // 供关闭后重建窗口使用
int  g_tab = 0;             // 当前页签：0 = 文本界面，1 = 空白界面
bool g_thumbDrag = false;   // 是否正在拖滑块
int  g_dragGrabDy = 0;      // 按住滑块时，鼠标相对滑块顶端的偏移

// 页签名。索引与 g_tab 对应
const wchar_t* kTabNames[layout::kTabCount] = { L"文本", L"界面二" };

// 滚轮/翻页的步长，按 DPI 换算成实际像素
int wheelStep() {
    return dpi::fixedAssetSize(layout::kWheelScrollStep);
}

// 页签栏宽度、页签高度等，都按 DPI 换算
int sidebarWidth()  { return dpi::fixedAssetSize(layout::kSidebarW); }
int tabHeight()     { return dpi::fixedAssetSize(layout::kTabH); }
int tabGap()        { return dpi::fixedAssetSize(layout::kTabGap); }
int tabTop()        { return dpi::fixedAssetSize(layout::kTabTop); }

// 第二个页签里那个开关的矩形（客户区坐标）。
// 位置只取决于页签栏宽度，与客户区尺寸无关。
RECT switchRect() {
    int x = sidebarWidth() + dpi::fixedAssetSize(layout::kSwitchX);
    int y = dpi::fixedAssetSize(layout::kSwitchY);
    int w = dpi::fixedAssetSize(layout::kSwitchW);
    int h = dpi::fixedAssetSize(layout::kSwitchH);
    RECT r = { x, y, x + w, y + h };
    return r;
}

// 命中区域：在开关四周各留一点余量，鼠标不必点得很准
RECT switchHitRect() {
    RECT r = switchRect();
    int m = dpi::fixedAssetSize(8);
    r.left -= m; r.top -= m; r.right += m; r.bottom += m;
    return r;
}

// 第 index 条页签的矩形（客户区坐标）
RECT tabRect(int index) {
    int x = 0;
    int w = sidebarWidth();
    int h = tabHeight();
    int y = tabTop() + index * (h + tabGap());
    RECT r = { x, y, x + w, y + h };
    return r;
}

// 内容区矩形 = 客户区去掉左侧页签栏
RECT contentRect(int clientW, int clientH) {
    RECT r = { sidebarWidth(), 0, clientW, clientH };
    return r;
}

// 命中哪条页签；返回 -1 表示没命中
int tabHitTest(POINT pt) {
    for (int i = 0; i < layout::kTabCount; i++) {
        RECT r = tabRect(i);
        if (pt.x >= r.left && pt.x <= r.right && pt.y >= r.top && pt.y <= r.bottom)
            return i;
    }
    return -1;
}

// 画左侧页签栏。选中项用左侧竖条 + 加粗字表示，不用底色，保持整体素净。
void drawSidebar(HDC hdc, int clientH) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    int barW = sidebarWidth();

    // 栏与内容之间的分隔线
    SolidBrush lineBrush(Color(255, 228, 228, 228));
    g.FillRectangle(&lineBrush, (REAL)(barW - 1), 0.0f, 1.0f, (REAL)clientH);

    FontFamily ff(DSF_TEXT_FONT);
    REAL fsz = dpi::fixedAssetSize(layout::kSidebarFontSize);
    Font normalFont(&ff, fsz, FontStyleRegular, UnitPixel);
    Font activeFont(&ff, fsz, FontStyleBold, UnitPixel);

    REAL markW = dpi::fixedAssetSize(layout::kSidebarMarkW);
    int  textPad = dpi::fixedAssetSize(layout::kSidebarTextPad);

    for (int i = 0; i < layout::kTabCount; i++) {
        RECT r = tabRect(i);
        bool active = (i == g_tab);

        if (active) {
            SolidBrush mark(Color(255, 60, 60, 60));
            g.FillRectangle(&mark, (REAL)r.left, (REAL)r.top, markW, (REAL)(r.bottom - r.top));
        }

        int v = active ? 20 : 140;
        SolidBrush text(Color(255, v, v, v));
        const Font* f = active ? &activeFont : &normalFont;

        // 不用 StringFormat::SetLineAlignment 做垂直居中 ——
        // 这个调用在 MinGW 的 GDI+ 上会直接崩（实测 0xC0000005）。
        // 改成先量文字高度、自己算居中偏移，等价且稳。
        RectF box((REAL)(r.left + textPad), (REAL)r.top,
                  (REAL)(r.right - r.left - textPad), (REAL)(r.bottom - r.top));
        RectF bounds;
        g.MeasureString(kTabNames[i], -1, f, box, nullptr, &bounds);

        REAL ty = box.Y + (box.Height - bounds.Height) / 2.0f;
        if (ty < box.Y) ty = box.Y;
        RectF drawBox(box.X, ty, box.Width, box.Height);

        g.DrawString(kTabNames[i], -1, f, drawBox, nullptr, &text);
    }
}

// 画第二个页签里的开关面板。
//
// 开关语义：
//   开 → 使用 Personality.txt（原来的角色提示词），带好感度系统
//   关 → 使用 Assistant.txt（新的通用助手提示词），不发送好感度
void drawSwitchPanel(HDC hdc) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    const bool on = settings::useOriginalPrompt();
    RECT sr = switchRect();
    REAL h = (REAL)(sr.bottom - sr.top);

    // 轨道：胶囊形。用两段半圆 + 上下直线构成，避免自己算圆角。
    SolidBrush track(on ? Color(255, 70, 130, 200) : Color(255, 205, 205, 205));
    {
        GraphicsPath path;
        path.AddArc((REAL)sr.left, (REAL)sr.top, h, h, 90.0f, 180.0f);
        path.AddArc((REAL)(sr.right - h), (REAL)sr.top, h, h, 270.0f, 180.0f);
        path.CloseFigure();
        g.FillPath(&track, &path);
    }

    // 滑块（白色圆）
    REAL knobD = h - dpi::fixedAssetSize(6);
    REAL knobY = (REAL)sr.top + dpi::fixedAssetSize(3);
    REAL knobX = on ? ((REAL)sr.right - dpi::fixedAssetSize(3) - knobD)
                    : ((REAL)sr.left + dpi::fixedAssetSize(3));
    SolidBrush knob(Color(255, 255, 255, 255));
    g.FillEllipse(&knob, knobX, knobY, knobD, knobD);

    // 说明文字
    FontFamily ff(DSF_TEXT_FONT);
    Font font(&ff, dpi::fixedAssetSize(layout::kSwitchFontSize),
              FontStyleRegular, UnitPixel);

    int textX = sidebarWidth() + dpi::fixedAssetSize(layout::kSwitchX);
    REAL textY = (REAL)sr.bottom + dpi::fixedAssetSize(layout::kSwitchLabelGap);
    REAL textW = (REAL)dpi::fixedAssetSize(layout::kSwitchTextW);

    SolidBrush dark(Color(255, 40, 40, 40));
    SolidBrush gray(Color(255, 130, 130, 130));

    RectF title(textX, textY, textW, 60.0f);
    g.DrawString(on ? L"已启用：原来的 system prompt" : L"已关闭：使用新的 system prompt",
                 -1, &font, title, nullptr, &dark);

    RectF body(textX, textY + dpi::fixedAssetSize(26), textW, 300.0f);
    const wchar_t* lines = on
        ? L"当前使用 Personality.txt（鲸鱼娘），\n带好感度系统。\n\n两种模式的对话历史各自独立保存，\n来回切换不会互相影响。"
        : L"当前使用 Assistant.txt（通用助手），\n不发送好感度相关内容。\n\n两种模式的对话历史各自独立保存，\n来回切换不会互相影响。";
    g.DrawString(lines, -1, &font, body, nullptr, &gray);
}

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

    // 页签栏
    drawSidebar(memDC, h);

    // 内容区：界面 0 画正文，界面 1 放开关面板
    RECT content = contentRect(w, h);
    if (g_tab == 0) {
        wintext::draw(memDC, content);
    } else {
        // 切到别的界面时也刷新一次 viewport，否则切回来时滚动位置是旧的
        wintext::setViewport(content);
        drawSwitchPanel(memDC);
    }

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
        if (g_tab != 0) return 0;   // 空白界面无可滚动内容
        int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        if (notches == 0) {
            notches = (GET_WHEEL_DELTA_WPARAM(wParam) > 0) ? 1 : -1;
        }
        if (wintext::scrollBy(-notches * wheelStep()))
            redraw(hwnd);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = clientPointFromLParam(lParam);

        // 先判页签栏：点中某条就切界面
        int tab = tabHitTest(pt);
        if (tab >= 0) {
            if (tab != g_tab) {
                g_tab = tab;
                g_thumbDrag = false;
                redraw(hwnd);
            }
            return 0;
        }

        // 界面 1：只处理那个开关
        if (g_tab == 1) {
            RECT hit = switchHitRect();
            if (pt.x >= hit.left && pt.x <= hit.right &&
                pt.y >= hit.top && pt.y <= hit.bottom) {
                // 切换 system prompt。会同步 state 的当前模式并落盘。
                settings::setUseOriginalPrompt(!settings::useOriginalPrompt());
                redraw(hwnd);
            }
            return 0;
        }

        // 内容区里处理滑块
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
            RECT content = contentRect(rc.right - rc.left, rc.bottom - rc.top);
            int page = (content.bottom - content.top) - layout::kWindowTextPad * 2;
            if (page < wheelStep()) page = wheelStep();
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
        dpi::setDpi((int)LOWORD(wParam));

        // 位置可以借用系统建议值（能保证窗口仍落在可见区域内），
        // 但【尺寸必须自己按 dpi::windowSize 算】。
        // 系统的 suggested 矩形是按"DPI 越高窗口越大"给的，和本项目
        // "只缩窗口"的策略正好相反，直接采用会让窗口被放大。
        const RECT* suggested = (const RECT*)lParam;
        const DWORD style = WS_OVERLAPPEDWINDOW;

        RECT rc = {0, 0, dpi::windowSize(layout::kMainW), dpi::windowSize(layout::kMainH)};
        AdjustWindowRect(&rc, style, FALSE);

        int x = suggested ? suggested->left : 0;
        int y = suggested ? suggested->top : 0;
        SetWindowPos(hwnd, nullptr, x, y,
                     rc.right - rc.left, rc.bottom - rc.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);

        // 重画即可：正文按当前内容区尺寸重新换行、页签栏按新栏宽重算，
        // 字号和栏宽是固定设计值乘缩放比，不需要重建任何东西。
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
    g_inst = hInstance;

    // 窗口类只需注册一次。
    // 这里不能每次都 RegisterClassW —— 类名已存在时它会失败（ERROR_CLASS_ALREADY_EXISTS），
    // 若照旧 return nullptr，就会出现"关掉主窗口后再也打不开"的问题。
    // openOrFocus() 重建窗口时会走到这里，所以必须幂等。
    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = BlankWndProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"DeepSeekFishBlank";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        if (!RegisterClassW(&wc)) return nullptr;
        classRegistered = true;
    }

    // 保留标题栏，但标题文字留空（下面传入 L""），所以看上去只有最小化和关闭
    // 两个按钮，没有标题。
    //
    // 说明：这两个按钮是标题栏自带的非客户区控件，必须靠 WS_CAPTION + WS_SYSMENU
    // 才有。如果改用 WS_POPUP（彻底无标题栏），按钮也会一起消失，想保留就得自己
    // 画按钮并处理命中消息，没必要。
    const DWORD style = WS_OVERLAPPEDWINDOW;

    // 把"期望的客户区尺寸"换算成"包含标题栏和边框的外框尺寸"。
    // 直接把客户区尺寸传给 CreateWindowExW 是常见错误：那样得到的客户区
    // 会小掉标题栏加边框的量，画出来的区域就不是设计尺寸了。
    //
    // 尺寸走 dpi::windowSize：窗口要"按 DPI 缩小"。
    RECT rc = {0, 0, dpi::windowSize(layout::kMainW), dpi::windowSize(layout::kMainH)};
    AdjustWindowRect(&rc, style, FALSE);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    g_wnd = CreateWindowExW(
        0,
        L"DeepSeekFishBlank",
        L"",                       // 标题留空
        style,
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

HWND window() {
    return g_wnd;
}

void rememberInstance(HINSTANCE hInstance) {
    g_inst = hInstance;
}

bool openOrFocus() {
    if (g_wnd) {
        // 已经开着：置前并给焦点（可能被最小化了）
        if (IsIconic(g_wnd)) ShowWindow(g_wnd, SW_RESTORE);
        SetForegroundWindow(g_wnd);
        SetFocus(g_wnd);
        return true;
    }

    // 被关掉过：重建。实例句柄在首次创建时已记下。
    if (!g_inst) return false;
    HWND w = createWindow(g_inst, SW_SHOWNORMAL);
    if (w) SetForegroundWindow(w);
    return w != nullptr;
}

void requestQuit() {
    // 直接结束消息循环。之后再销毁窗口，WinMain 的收尾代码负责存盘。
    // 这里不能走 DestroyWindow(pet) 那条路 —— 那个 WM_DESTROY 也会
    // PostQuitMessage，但桌宠窗口可能在别处已被销毁，靠它不可靠。
    HWND pet = ui::petWindow();
    if (pet) DestroyWindow(pet);
    if (g_wnd) DestroyWindow(g_wnd);
    PostQuitMessage(0);
}

} // namespace whitewin