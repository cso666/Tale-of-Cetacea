#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <thread>
#include <winhttp.h>
#include <imm.h>
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "imm32.lib")
using namespace Gdiplus;

#define WM_SHOW_BUBBLE (WM_APP + 1)

RECT g_sendRect = {295, 30, 365, 70};

std::string API_KEY;
std::string PERSONALITY;

HWND g_inputWnd = nullptr;
std::wstring g_inputText;
std::wstring g_imeComp;

HWND g_bubbleWnd = nullptr;
std::wstring g_bubbleText;
int g_bubbleW = 400;
int g_bubbleH = 205;

int winW = 180;
int winH = 316;

bool isSitting = false;
int sitOffset = 5;
bool userDragging = false;

int frameIndex = 0;
std::vector<std::wstring> frames;

std::vector<std::wstring> sitFrames;
int sitFrameIndex = 0;

std::vector<std::wstring> eatFrames;
int eatFrameIndex = 0;
bool isEating = false;

std::vector<std::wstring> tokenFrames;
int tokenFrameIndex = 0;
bool isEatingToken = false;

POINT downPt = {};

std::string chatHistory;
int chatRound = 0;
int g_favor = 0;

void UpdateSitState(HWND hwnd);
void CreateChatInput(HWND parent, HINSTANCE hInst);
void CreateBubble(HWND parent, const std::wstring& text);

std::string WStringToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(len - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], len, nullptr, nullptr);
    return s;
}

std::string EscapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        unsigned char uc = (unsigned char)c;
        if (uc < 0x20) continue;
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        default: out += c;
        }
    }
    return out;
}

std::string ExtractReply(const std::string& result) {
    size_t pos = result.find("\"content\":\"");
    if (pos != std::string::npos) {
        pos += 11;
        size_t end = result.find("\"", pos);
        return result.substr(pos, end - pos);
    }
    return result;
}

std::string HttpPostJson(const std::string& body, const std::string& key) {
    std::string reply = "请求失败";

    HINTERNET hSession = WinHttpOpen(L"DeepSeekFish/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (hSession) {
        HINTERNET hConnect = WinHttpConnect(hSession, L"api.deepseek.com", 443, 0);
        if (hConnect) {
            HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/chat/completions",
                nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);
            if (hRequest) {
                std::wstring headers = L"Content-Type: application/json\r\n";
                headers += L"Authorization: Bearer " + std::wstring(key.begin(), key.end()) + L"\r\n";

                if (WinHttpSendRequest(hRequest, headers.c_str(), -1,
                    (LPVOID)body.c_str(), body.size(), body.size(), 0)) {
                    if (WinHttpReceiveResponse(hRequest, nullptr)) {
                        std::string result;
                        DWORD size = 0;
                        do {
                            WinHttpQueryDataAvailable(hRequest, &size);
                            if (size > 0) {
                                char* buf = new char[size + 1];
                                DWORD read = 0;
                                WinHttpReadData(hRequest, buf, size, &read);
                                buf[read] = 0;
                                result += buf;
                                delete[] buf;
                            }
                        } while (size > 0);
                        reply = ExtractReply(result);
                    }
                }
                WinHttpCloseHandle(hRequest);
            }
            WinHttpCloseHandle(hConnect);
        }
        WinHttpCloseHandle(hSession);
    }

    size_t p = 0;
    while ((p = reply.find("\\n", p)) != std::string::npos) {
        reply.replace(p, 2, "\n");
        p += 1;
    }
    return reply;
}

void SaveFavor() {
    if (g_favor > 100) g_favor = 100;
    if (g_favor < -100) g_favor = -100;
    FILE* ff = fopen("archive\\favor.txt", "wb");
    if (ff) {
        std::string s = std::to_string(g_favor);
        fwrite(s.c_str(), 1, s.size(), ff);
        fclose(ff);
    }
}

void SaveHistory() {
    FILE* hf = fopen("archive\\history.txt", "wb");
    if (hf) {
        fwrite(chatHistory.c_str(), 1, chatHistory.size(), hf);
        fclose(hf);
    }
}

void SummarizeHistory(HWND hwnd, const std::string& key) {
    if (chatHistory.empty()) return;

    std::string body = "{\"model\":\"deepseek-flash\",\"messages\":[";
    body += "{\"role\":\"system\",\"content\":\"你是对话摘要器。只输出两句话的客观摘要，使用第三人称，不模仿任何角色语气，不加动作描写。\"},";
    body += "{\"role\":\"user\",\"content\":\"" + EscapeJson(chatHistory) + "\"}";
    body += "],\"stream\":false}";

    std::string summary = HttpPostJson(body, key);

    if (!summary.empty() && summary != "请求失败") {
        chatHistory = "[对话摘要]\n" + summary + "\n\n[最近对话]\n";
        chatRound = 0;
        SaveHistory();
    }
}

void AskDeepSeek(HWND hwnd, std::string key, std::string userMsg) {
    chatRound++;
    if (chatRound >= 5) {
        SummarizeHistory(hwnd, key);
    }

    std::string favorLine = "[system]当前好感度：" + std::to_string(g_favor) +
        "。请在回复末尾加上[favor+数字]或[favor-数字]，范围-25到+25，表示你对用户好感度的变化。";
    std::string fullUser = chatHistory + "\n" + favorLine + "\n" + userMsg;

    std::string body = "{\"model\":\"deepseek-flash\",\"messages\":[";
    body += "{\"role\":\"system\",\"content\":\"" + EscapeJson(PERSONALITY) + "\"},";
    body += "{\"role\":\"user\",\"content\":\"" + EscapeJson(fullUser) + "\"}";
    body += "],\"stream\":false}";

    std::string reply = HttpPostJson(body, key);

    int delta = 0;
    size_t fp = reply.find("[favor");
    if (fp != std::string::npos) {
        size_t fe = reply.find("]", fp);
        if (fe != std::string::npos) {
            std::string tag = reply.substr(fp, fe - fp + 1);
            int d = atoi(tag.c_str() + 6);
            delta=d;
            reply.erase(fp, fe - fp + 1);
        }
    }

    g_favor += delta;
    if (g_favor > 100) g_favor = 100;
    if (g_favor < -100) g_favor = -100;
    SaveFavor();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, reply.c_str(), -1, nullptr, 0);
    std::wstring wreply(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, reply.c_str(), -1, &wreply[0], wlen);

    chatHistory += "user: " + userMsg + "\n";
    chatHistory += "assistant: " + reply + "\n";
    SaveHistory();

    std::wstring* pReply = new std::wstring(wreply);
    PostMessage(hwnd, WM_SHOW_BUBBLE, 0, (LPARAM)pReply);
}

void DrawFish(HWND hwnd) {
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = winW;
    bmi.bmiHeader.biHeight = -winH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(memDC, hBitmap);

    Graphics graphics(memDC);

    std::wstring path;
    if (isEatingToken) {
        path = tokenFrames[tokenFrameIndex].c_str();
    } else if (isEating) {
        path = eatFrames[eatFrameIndex].c_str();
    } else if (isSitting) {
        path = sitFrames[sitFrameIndex].c_str();
    } else {
        path = frames[frameIndex].c_str();
    }

    Image image(path.c_str());
    graphics.DrawImage(&image, 0, 0, winW, winH);

    POINT ptSrc = {0, 0};
    SIZE sz = {winW, winH};
    POINT ptDst = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, screenDC, nullptr, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void DrawInputBox(HWND hwnd) {
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 400;
    bmi.bmiHeader.biHeight = -100;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(memDC, hBmp);

    Graphics g(memDC);

    Image img(L"input\\input.png");
    g.DrawImage(&img, 0, 0, 400, 100);

    FontFamily ff(L"Microsoft YaHei");
    Font font(&ff, 18, FontStyleRegular, UnitPixel);
    SolidBrush brush(Color(255, 40, 40, 40));

    std::wstring showText = g_inputText + g_imeComp;
    g.DrawString(showText.c_str(), -1, &font, PointF(30, 35), &brush);

    POINT ptSrc = {0, 0};
    SIZE sz = {400, 100};
    POINT ptDst = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, screenDC, nullptr, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void DrawBubble(HWND hwnd) {
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_bubbleW;
    bmi.bmiHeader.biHeight = -g_bubbleH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(memDC, hBmp);

    Graphics g(memDC);

    Image img(L"output\\output.png");
    g.DrawImage(&img, 0, 0, g_bubbleW, g_bubbleH);

    FontFamily ff(L"幼圆");
    Font font(&ff, 16, FontStyleRegular, UnitPixel);
    SolidBrush brush(Color(255, 40, 40, 40));

    RectF layout(25, 20, g_bubbleW - 50, g_bubbleH - 40);
    g.DrawString(g_bubbleText.c_str(), -1, &font, layout, nullptr, &brush);

    POINT ptSrc = {0, 0};
    SIZE sz = {g_bubbleW, g_bubbleH};
    POINT ptDst = {0, 0};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, screenDC, nullptr, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

bool dragging = false;
POINT dragOffset = {};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN: {
        GetCursorPos(&downPt);
        userDragging = true;
        if (isSitting) {
            isSitting = false;
            DrawFish(hwnd);
        }
        dragging = true;
        POINT pt;
        GetCursorPos(&pt);
        RECT rc;
        GetWindowRect(hwnd, &rc);
        dragOffset.x = pt.x - rc.left;
        dragOffset.y = pt.y - rc.top;
        SetCapture(hwnd);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (dragging) {
            POINT pt;
            GetCursorPos(&pt);
            SetWindowPos(hwnd, nullptr, pt.x - dragOffset.x, pt.y - dragOffset.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        userDragging = false;
        dragging = false;
        ReleaseCapture();

        POINT upPt;
        GetCursorPos(&upPt);
        if (abs(upPt.x - downPt.x) < 3 && abs(upPt.y - downPt.y) < 3) {
            HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE);
            CreateChatInput(hwnd, hInst);
        }
        return 0;
    }
    case WM_DESTROY:
        SaveFavor();
        SaveHistory();
        PostQuitMessage(0);
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        return 0;
    case WM_TIMER: {
        if (wParam == 1) {
            if (isEatingToken) {
                tokenFrameIndex++;
                if (tokenFrameIndex >= tokenFrames.size()) {
                    tokenFrameIndex = 0;
                    isEatingToken = false;
                }
            } else if (isEating) {
                eatFrameIndex++;
                if (eatFrameIndex >= eatFrames.size()) {
                    eatFrameIndex = 0;
                    isEating = false;
                }
            } else if (isSitting) {
                sitFrameIndex = (sitFrameIndex + 1) % sitFrames.size();
            } else {
                frameIndex = (frameIndex + 1) % frames.size();
            }
            DrawFish(hwnd);
        } else if (wParam == 2) {
            UpdateSitState(hwnd);
        }
        return 0;
    }
    case WM_DROPFILES: {
        if (isSitting || isEating) return 0;

        HDROP hDrop = (HDROP)wParam;
        char file[MAX_PATH];
        DragQueryFile(hDrop, 0, file, MAX_PATH);
        DragFinish(hDrop);

        DWORD attr = GetFileAttributes(file);
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return 0;
        }

        char* name = strrchr(file, '\\');
        if (name) name++; else name = file;

        bool isToken = (strstr(name, "token") != nullptr);

        if (isToken) {
            DeleteFile(file);
            isEatingToken = true;
            tokenFrameIndex = 0;
            std::thread t(AskDeepSeek, hwnd, API_KEY, "user给你了一个token");
            t.detach();
        } else {
            DeleteFile(file);
            isEating = true;
            eatFrameIndex = 0;
            std::thread t(AskDeepSeek, hwnd, API_KEY, "user给你了一碗米饭");
            t.detach();
        }
        return 0;
    }
    case WM_SHOW_BUBBLE: {
        std::wstring* pText = (std::wstring*)lParam;
        if (pText) {
            CreateBubble(hwnd, *pText);
            delete pText;
        }
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK InputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CHAR: {
        if (wParam == VK_RETURN) {
            std::string msg = WStringToUtf8(g_inputText);
            if (!msg.empty()) {
                HWND fish = GetParent(hwnd);
                std::thread t(AskDeepSeek, fish, API_KEY, msg);
                t.detach();
                DestroyWindow(hwnd);
                g_inputWnd = nullptr;
                g_inputText.clear();
                g_imeComp.clear();
            }
        } else if (wParam == VK_BACK) {
            if (!g_imeComp.empty()) {
                g_imeComp.pop_back();
            } else if (!g_inputText.empty()) {
                g_inputText.pop_back();
            }
            DrawInputBox(hwnd);
        } else if (wParam >= 32 && wParam < 128) {
            g_inputText.push_back((wchar_t)wParam);
            DrawInputBox(hwnd);
        }
        return 0;
    }
    case WM_IME_CHAR: {
        g_inputText.push_back((wchar_t)wParam);
        g_imeComp.clear();
        DrawInputBox(hwnd);
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
        DrawInputBox(hwnd);
        return 0;
    }
    case WM_IME_ENDCOMPOSITION: {
        g_imeComp.clear();
        DrawInputBox(hwnd);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        if (pt.x >= g_sendRect.left && pt.x <= g_sendRect.right &&
            pt.y >= g_sendRect.top && pt.y <= g_sendRect.bottom) {
            std::string msg = WStringToUtf8(g_inputText);
            if (!msg.empty()) {
                HWND fish = GetParent(hwnd);
                std::thread t(AskDeepSeek, fish, API_KEY, msg);
                t.detach();
                DestroyWindow(hwnd);
                g_inputWnd = nullptr;
                g_inputText.clear();
                g_imeComp.clear();
            }
        }
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
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

void CreateBubble(HWND parent, const std::wstring& text) {
    if (g_bubbleWnd) {
        DestroyWindow(g_bubbleWnd);
        g_bubbleWnd = nullptr;
    }

    static bool bRegistered = false;
    if (!bRegistered) {
        WNDCLASS bwc = {};
        bwc.lpfnWndProc = BubbleWndProc;
        bwc.hInstance = GetModuleHandle(nullptr);
        bwc.lpszClassName = "BubbleWnd";
        RegisterClass(&bwc);
        bRegistered = true;
    }

    RECT rc;
    GetWindowRect(parent, &rc);
    int x = rc.left + winW + 10;
    int y = rc.top;

    g_bubbleText = text;
    g_bubbleWnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "BubbleWnd", "",
        WS_POPUP,
        x, y, g_bubbleW, g_bubbleH,
        parent, nullptr, GetModuleHandle(nullptr), nullptr);

    ShowWindow(g_bubbleWnd, SW_SHOW);
    DrawBubble(g_bubbleWnd);
}

void CreateChatInput(HWND parent, HINSTANCE hInst) {
    if (g_inputWnd) return;

    static bool registered = false;
    if (!registered) {
        WNDCLASS iwc = {};
        iwc.lpfnWndProc = InputWndProc;
        iwc.hInstance = hInst;
        iwc.lpszClassName = "InputBoxWnd";
        RegisterClass(&iwc);
        registered = true;
    }

    RECT rc;
    GetWindowRect(parent, &rc);
    int x = rc.left;
    int y = rc.top - 120;
    if (y < 0) y = rc.bottom + 10;

    g_inputWnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "InputBoxWnd", "",
        WS_POPUP,
        x, y, 400, 100,
        parent, nullptr, hInst, nullptr);

    g_inputText.clear();
    g_imeComp.clear();
    ShowWindow(g_inputWnd, SW_SHOW);
    SetFocus(g_inputWnd);
    DrawInputBox(g_inputWnd);
}

void UpdateSitState(HWND hwnd) {
    if (userDragging) return;
    HWND hTaskbar = FindWindow("Shell_TrayWnd", nullptr);
    if (!hTaskbar) return;

    RECT frc, trc, inter;
    GetWindowRect(hwnd, &frc);
    GetWindowRect(hTaskbar, &trc);

    if (IntersectRect(&inter, &frc, &trc)) {
        if (!isSitting) {
            isSitting = true;
            RECT frc2, trc2;
            GetWindowRect(hwnd, &frc2);
            GetWindowRect(hTaskbar, &trc2);
            int fishW = frc2.right - frc2.left;
            int x = frc2.left;
            double scale = (double)winH / 2752.0;
            int bottomOffset = (int)(1120 * scale) + 5;
            int y = trc2.top - winH + bottomOffset;
            SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            DrawFish(hwnd);
        }
    } else {
        if (isSitting) {
            isSitting = false;
            DrawFish(hwnd);
        }
    }
}

void SitOnTaskbar(HWND hwnd) {
    isSitting = true;
    HWND hTaskbar = FindWindow("Shell_TrayWnd", nullptr);
    if (!hTaskbar) return;

    RECT rc;
    GetWindowRect(hTaskbar, &rc);

    RECT fish;
    GetWindowRect(hwnd, &fish);
    int fishW = fish.right - fish.left;
    int fishH = fish.bottom - fish.top;

    double scale = (double)winH / 2752.0;
    int bottomOffset = (int)(1120 * scale) + 5;

    int x = rc.right - fishW;
    int y = rc.top - winH + bottomOffset;

    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    CreateDirectory("archive", nullptr);

    FILE* f = fopen("API_key.txt", "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = new char[len + 1];
    fread(buf, 1, len, f);
    buf[len] = 0;
    fclose(f);
    API_KEY = buf;
    delete[] buf;
    while (!API_KEY.empty() && (API_KEY.back() == '\n' || API_KEY.back() == '\r')) {
        API_KEY.pop_back();
    }

    FILE* pf = fopen("Personality.txt", "rb");
    if (pf) {
        fseek(pf, 0, SEEK_END);
        long plen = ftell(pf);
        fseek(pf, 0, SEEK_SET);
        char* pbuf = new char[plen + 1];
        fread(pbuf, 1, plen, pf);
        pbuf[plen] = 0;
        fclose(pf);
        PERSONALITY = pbuf;
        delete[] pbuf;
    }

    FILE* hf = fopen("archive\\history.txt", "rb");
    if (hf) {
        fseek(hf, 0, SEEK_END);
        long hlen = ftell(hf);
        fseek(hf, 0, SEEK_SET);
        char* hbuf = new char[hlen + 1];
        fread(hbuf, 1, hlen, hf);
        hbuf[hlen] = 0;
        chatHistory = hbuf;
        delete[] hbuf;
        fclose(hf);
    }

    FILE* ff = fopen("archive\\favor.txt", "rb");
    if (ff) {
        char fbuf[32] = {};
        fread(fbuf, 1, 31, ff);
        fclose(ff);
        g_favor = atoi(fbuf);
    }

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    const char CLASS_NAME[] = "DeepSeekFishWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        CLASS_NAME,
        "DeepSeek Fish",
        WS_POPUP,
        100, 100, winW, winH,
        nullptr, nullptr, hInstance, nullptr
    );

    for (int i = 1; i <= 40; i++) {
        wchar_t name[64];
        swprintf(name, 64, L"blink\\frame_%02d.png", i);
        frames.push_back(name);
    }

    for (int i = 1; i <= 24; i++) {
        wchar_t name[64];
        swprintf(name, 64, L"sit\\fit_%02d.png", i);
        sitFrames.push_back(name);
    }
    for (int i = 24; i >= 1; i--) {
        wchar_t name[64];
        swprintf(name, 64, L"sit\\fit_%02d.png", i);
        sitFrames.push_back(name);
    }

    for (int i = 1; i <= 24; i++) {
        wchar_t name[64];
        swprintf(name, 64, L"eatfile\\scaled_%02d.png", i);
        eatFrames.push_back(name);
    }

    for (int i = 1; i <= 24; i++) {
        wchar_t name[64];
        swprintf(name, 64, L"token\\final_%02d.png", i);
        tokenFrames.push_back(name);
    }

    SetTimer(hwnd, 1, 250, nullptr);
    SetTimer(hwnd, 2, 50, nullptr);

    ShowWindow(hwnd, nCmdShow);
    DragAcceptFiles(hwnd, TRUE);
    SitOnTaskbar(hwnd);
    DrawFish(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}