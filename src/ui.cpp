#include "ui.h"
#include "layout.h"
#include "render.h"
#include "pet.h"
#include "state.h"
#include "chat.h"
#include "config.h"
#include "textutils.h"
#include "dpi.h"

#include <string>
#include <thread>
#include <mutex>

namespace ui {

namespace {

const char* kPetClass = "DeepSeekFishWindow";
const char* kInputClass = "InputBoxWnd";
const char* kBubbleClass = "BubbleWnd";

HWND         g_inputWnd = nullptr;
std::wstring g_inputText;
std::wstring g_imeComp;

HWND         g_bubbleWnd = nullptr;
std::wstring g_bubbleText;

bool  g_dragging = false;
POINT g_dragOffset = {};
POINT g_downPt = {};

void drawInputBox() {
    if (g_inputWnd) render::inputBox(g_inputWnd, g_inputText + g_imeComp);
}

void closeInputBox() {
    if (g_inputWnd) {
        DestroyWindow(g_inputWnd);
        g_inputWnd = nullptr;
    }
    g_inputText.clear();
    g_imeComp.clear();
}

// 从输入框取出内容并发送
void commitInput() {
    std::wstring text = g_inputText;
    if (text.empty()) return;
    HWND petWnd = GetParent(g_inputWnd);
    submitUserMessage(petWnd, textutils::toUtf8(text).c_str());
    closeInputBox();
}

void startRequest(HWND petWnd, const char* utf8Msg) {
    std::string msg = utf8Msg;
    {
        std::lock_guard<std::mutex> lk(state::g_mutex);
        // 上一句还没回来就先不叠请求
        if (state::g_requestInFlight) return;
        state::g_requestInFlight = true;
        // 立刻落历史，这样即使随后请求失败，这句也不会凭空消失
        state::appendTurn("user", msg);
        state::saveHistory();
    }

    std::thread t([](HWND h, std::string m) {
        chat::ask(h, config::apiKey, m);
        std::lock_guard<std::mutex> lk(state::g_mutex);
        state::g_requestInFlight = false;
    }, petWnd, msg);
    t.detach();
}

LRESULT CALLBACK BubbleWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        DestroyWindow(hwnd);
        g_bubbleWnd = nullptr;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK InputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CHAR: {
        if (wParam == VK_RETURN) {
            commitInput();
        } else if (wParam == VK_BACK) {
            if (!g_imeComp.empty()) g_imeComp.pop_back();
            else if (!g_inputText.empty()) g_inputText.pop_back();
            drawInputBox();
        } else if (wParam == VK_ESCAPE) {
            closeInputBox();
        } else if (wParam >= 32 && wParam < 128) {
            g_inputText.push_back((wchar_t)wParam);
            drawInputBox();
        }
        return 0;
    }
    case WM_IME_CHAR: {
        g_inputText.push_back((wchar_t)wParam);
        g_imeComp.clear();
        drawInputBox();
        return 0;
    }
    case WM_IME_COMPOSITION: {
        HIMC hImc = ImmGetContext(hwnd);
        if (hImc) {
            DWORD resSize = ImmGetCompositionStringW(hImc, GCS_RESULTSTR, nullptr, 0);
            if (resSize > 0) {
                std::wstring result;
                result.resize(resSize / sizeof(wchar_t));
                ImmGetCompositionStringW(hImc, GCS_RESULTSTR, &result[0], resSize);
                g_inputText += result;
                g_imeComp.clear();
            } else {
                DWORD compSize = ImmGetCompositionStringW(hImc, GCS_COMPSTR, nullptr, 0);
                if (compSize > 0) {
                    g_imeComp.resize(compSize / sizeof(wchar_t));
                    ImmGetCompositionStringW(hImc, GCS_COMPSTR, &g_imeComp[0], compSize);
                } else {
                    g_imeComp.clear();
                }
            }
            ImmReleaseContext(hwnd, hImc);
        }
        drawInputBox();
        return 0;
    }
    case WM_IME_ENDCOMPOSITION: {
        g_imeComp.clear();
        drawInputBox();
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        const RECT& r = layout::kSendRect;
        if (pt.x >= r.left && pt.x <= r.right && pt.y >= r.top && pt.y <= r.bottom)
            commitInput();
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void createBubble(HWND parent, const std::wstring& text) {
    if (g_bubbleWnd) {
        DestroyWindow(g_bubbleWnd);
        g_bubbleWnd = nullptr;
    }

    static bool registered = false;
    if (!registered) {
        WNDCLASS bwc = {};
        bwc.lpfnWndProc = BubbleWndProc;
        bwc.hInstance = GetModuleHandle(nullptr);
        bwc.lpszClassName = kBubbleClass;
        RegisterClass(&bwc);
        registered = true;
    }

    RECT rc;
    GetWindowRect(parent, &rc);
    int x = rc.left + dpi::scale(layout::kFishW) + dpi::scale(10);
    int y = rc.top;

    const int bubbleW = dpi::scale(layout::kBubbleW);

    g_bubbleText = text;
    int h = render::bubbleHeight(text, bubbleW);

    g_bubbleWnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kBubbleClass, "",
        WS_POPUP,
        x, y, bubbleW, h,
        parent, nullptr, GetModuleHandle(nullptr), nullptr);

    if (!g_bubbleWnd) return;
    ShowWindow(g_bubbleWnd, SW_SHOW);
    render::bubble(g_bubbleWnd, g_bubbleText, bubbleW, h);
}

void createChatInput(HWND parent, HINSTANCE hInst) {
    if (g_inputWnd) return;

    static bool registered = false;
    if (!registered) {
        WNDCLASS iwc = {};
        iwc.lpfnWndProc = InputWndProc;
        iwc.hInstance = hInst;
        iwc.lpszClassName = kInputClass;
        RegisterClass(&iwc);
        registered = true;
    }

    RECT rc;
    GetWindowRect(parent, &rc);
    int x = rc.left;
    int y = rc.top - dpi::scale(120);
    if (y < 0) y = rc.bottom + dpi::scale(10);

    g_inputWnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kInputClass, "",
        WS_POPUP,
        x, y, dpi::scale(layout::kChatInputW), dpi::scale(layout::kChatInputH),
        parent, nullptr, hInst, nullptr);

    if (!g_inputWnd) return;

    g_inputText.clear();
    g_imeComp.clear();
    ShowWindow(g_inputWnd, SW_SHOW);
    SetFocus(g_inputWnd);
    drawInputBox();
}

LRESULT CALLBACK PetWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN: {
        GetCursorPos(&g_downPt);
        pet::beginDrag(hwnd);
        g_dragging = true;

        POINT pt;
        GetCursorPos(&pt);
        RECT rc;
        GetWindowRect(hwnd, &rc);
        g_dragOffset.x = pt.x - rc.left;
        g_dragOffset.y = pt.y - rc.top;
        SetCapture(hwnd);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (g_dragging) {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, nullptr, pt.x - g_dragOffset.x, pt.y - g_dragOffset.y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        g_dragging = false;
        pet::endDrag();
        ReleaseCapture();

        POINT upPt;
        GetCursorPos(&upPt);
        // 位移小于 3px 才算"点击"，避免拖动结束时误触发输入框
        if (abs((int)upPt.x - (int)g_downPt.x) < 3 && abs((int)upPt.y - (int)g_downPt.y) < 3) {
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            createChatInput(hwnd, hInst);
        }
        return 0;
    }
    case WM_DPICHANGED: {
        // 桌宠被拖到另一块缩放比例不同的显示器上。
        dpi::setDpi((int)LOWORD(wParam));

        // 帧图是固定像素的，窗口物理尺寸必须跟着缩放比例走，
        // 否则在高 DPI 屏上鱼会显得越来越小。
        int w = dpi::scale(layout::kFishW);
        int h = dpi::scale(layout::kFishH);
        SetWindowPos(hwnd, nullptr, 0, 0, w, h,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

        // 任务栏本身也随 DPI 变了，重新吸附一次保持贴底
        pet::sitOnTaskbar(hwnd);
        pet::draw(hwnd);
        return 0;
    }
    case WM_TIMER: {
        if (wParam == layout::kTimerAnim) {
            pet::tick(hwnd);
        } else if (wParam == layout::kTimerSitWatch) {
            pet::watchTaskbar(hwnd);
        }
        return 0;
    }
    case WM_DROPFILES: {
        HDROP hDrop = (HDROP)wParam;
        // FIX: 原实现用 char file[MAX_PATH] 去接宽字符路径。只要路径里有中文
        //      （比如用户名是中文），文件名就整个乱掉，DeleteFile 必然失败。
        wchar_t file[MAX_PATH] = {};
        DragQueryFileW(hDrop, 0, file, MAX_PATH);
        DragFinish(hDrop);

        DWORD attr = GetFileAttributesW(file);
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) return 0;

        // FIX: 原来先删文件再判断能不能接单，删完发现请求在途就直接 return，
        //      g_requestInFlight 永远停在 true，桌宠此后再也不理人。
        if (!pet::startEating(file)) return 0;

        const wchar_t* name = wcsrchr(file, L'\\');
        name = name ? name + 1 : file;
        bool isToken = (wcsstr(name, L"token") != nullptr);

        startRequest(hwnd, isToken ? "user给你了一个token" : "user给你了一碗米饭");
        return 0;
    }
    case WM_SHOW_BUBBLE: {
        std::wstring* pText = (std::wstring*)lParam;
        if (pText) {
            createBubble(hwnd, *pText);
            delete pText;
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace

void submitUserMessage(HWND petWnd, const char* utf8Msg) {
    if (!utf8Msg || !*utf8Msg) return;
    startRequest(petWnd, utf8Msg);
}

HWND createPetWindow(HINSTANCE hInstance) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = PetWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kPetClass;
    if (!RegisterClass(&wc)) return nullptr;

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        kPetClass,
        "DeepSeek Fish",
        WS_POPUP,
        dpi::scale(100), dpi::scale(100),
        dpi::scale(layout::kFishW), dpi::scale(layout::kFishH),
        nullptr, nullptr, hInstance, nullptr);

    if (hwnd) DragAcceptFiles(hwnd, TRUE);
    return hwnd;
}

} // namespace ui
