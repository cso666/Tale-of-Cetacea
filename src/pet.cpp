#include "pet.h"
#include "layout.h"
#include "apppath.h"
#include "dpi.h"
#include <gdiplus.h>
#include <vector>

using namespace Gdiplus;

namespace pet {

namespace {

std::vector<std::wstring> g_blink;
std::vector<std::wstring> g_sit;
std::vector<std::wstring> g_eat;
std::vector<std::wstring> g_token;

int  g_blinkIndex = 0;
int  g_sitIndex = 0;
int  g_eatIndex = 0;
int  g_tokenIndex = 0;

bool g_sitting = false;
bool g_eating = false;
bool g_eatingToken = false;
bool g_dragging = false;

// 坐姿动画是正播再反播的呼吸循环。用它记录方向，避免播到末尾时从尾帧
// 瞬间跳回首帧（看起来像抽搐）。
bool g_sitForward = true;
bool g_sitHoldLast = false;

void makeFrameNames(std::vector<std::wstring>& out, const wchar_t* fmt, int count) {
    out.clear();
    out.reserve(count);
    for (int i = 1; i <= count; i++) {
        wchar_t name[64];
        swprintf(name, 64, fmt, i);
        out.push_back(name);
    }
}

} // namespace

void init(int blinkCount, int sitCount, int eatCount, int tokenCount) {
    makeFrameNames(g_blink, L"blink\\frame_%02d.png", blinkCount);
    makeFrameNames(g_sit, L"sit\\fit_%02d.png", sitCount);
    makeFrameNames(g_eat, L"eatfile\\scaled_%02d.png", eatCount);
    makeFrameNames(g_token, L"token\\final_%02d.png", tokenCount);

    g_blinkIndex = g_sitIndex = g_eatIndex = g_tokenIndex = 0;
    g_sitting = g_eating = g_eatingToken = false;
    g_sitForward = true;
    g_sitHoldLast = false;
}

const std::wstring& currentFrame() {
    static const std::wstring empty;

    if (g_eatingToken && !g_token.empty())
        return g_token[g_tokenIndex % g_token.size()];
    if (g_eating && !g_eat.empty())
        return g_eat[g_eatIndex % g_eat.size()];
    if (g_sitting && !g_sit.empty())
        return g_sit[g_sitIndex % g_sit.size()];
    if (!g_blink.empty())
        return g_blink[g_blinkIndex % g_blink.size()];

    return empty;
}

void draw(HWND hwnd) {
    const std::wstring& rel = currentFrame();
    if (rel.empty()) return;

    // 帧图是固定像素的。因为进程现在是 DPI 感知的，Windows 不会再替我们拉伸，
    // 所以必须用 fixedAssetSize 自己把物理尺寸放大回"维持原有观感"的大小。
    // 否则 150% 缩放下桌宠会突然变小。
    const int drawW = dpi::fixedAssetSize(layout::kFishW);
    const int drawH = dpi::fixedAssetSize(layout::kFishH);

    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return;
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = drawW;
    bmi.bmiHeader.biHeight = -drawH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBitmap) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(memDC, hBitmap);

    // FIX: 原实现不检查图片是否真的加载成功。图片缺失时 GDI+ 静默什么都不画，
    //      表现为"桌宠凭空消失"，很难排查。现在加载失败就跳过这一帧。
    Image image(apppath::of(rel).c_str());
    if (image.GetLastStatus() == Ok) {
        Graphics graphics(memDC);
        graphics.DrawImage(&image, 0, 0, drawW, drawH);

        POINT ptSrc = {0, 0};
        SIZE sz = {drawW, drawH};
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(hwnd, screenDC, nullptr, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    }

    SelectObject(memDC, oldBmp);
    DeleteObject(hBitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void tick(HWND hwnd) {
    if (g_eatingToken && !g_token.empty()) {
        g_tokenIndex++;
        if (g_tokenIndex >= (int)g_token.size()) {
            g_tokenIndex = 0;
            g_eatingToken = false;
        }
    } else if (g_eating && !g_eat.empty()) {
        g_eatIndex++;
        if (g_eatIndex >= (int)g_eat.size()) {
            g_eatIndex = 0;
            g_eating = false;
        }
    } else if (g_sitting && !g_sit.empty()) {
        if (g_sitHoldLast) {
            g_sitHoldLast = false;                  // 在尾帧多停一拍
        } else if (g_sitForward) {
            g_sitIndex++;
            if (g_sitIndex >= (int)g_sit.size()) {
                g_sitIndex = (int)g_sit.size() - 1;
                g_sitForward = false;
                g_sitHoldLast = true;
            }
        } else {
            g_sitIndex--;
            if (g_sitIndex <= 0) {
                g_sitIndex = 0;
                g_sitForward = true;
                g_sitHoldLast = true;
            }
        }
    } else if (!g_blink.empty()) {
        g_blinkIndex = (g_blinkIndex + 1) % (int)g_blink.size();
    }

    draw(hwnd);
}

void beginDrag(HWND hwnd) {
    g_dragging = true;
    if (g_sitting) {
        g_sitting = false;
        draw(hwnd);
    }
}

void endDrag() {
    g_dragging = false;
}

bool startEating(const std::wstring& filePath) {
    if (g_sitting || g_eating || g_eatingToken) return false;

    const wchar_t* name = wcsrchr(filePath.c_str(), L'\\');
    name = name ? name + 1 : filePath.c_str();
    bool isToken = (wcsstr(name, L"token") != nullptr);

    // FIX: 原实现先把文件删了再发请求，而且中途 return 会让"请求在途"标记
    //      永久停在 true，之后桌宠再也不理人。现在删不掉就当作没投喂。
    if (!DeleteFileW(filePath.c_str())) return false;

    if (isToken) {
        g_eatingToken = true;
        g_tokenIndex = 0;
    } else {
        g_eating = true;
        g_eatIndex = 0;
    }
    return true;
}

// 坐姿帧里鱼的"视觉底边"和图片底边之间有一段空白。
// 2752 / 1120 是未缩放坐姿帧上的实测比例，这里按 DPI 后的显示高度等比换算，
// 保证坐姿在任何缩放比下都刚好压住任务栏而不是浮空或陷进去。
static int sitBottomOffset() {
    double scale = (double)dpi::fixedAssetSize(layout::kFishH) / 2752.0;
    return (int)(1120 * scale) + dpi::fixedAssetSize(5);
}

void watchTaskbar(HWND hwnd) {
    if (g_dragging) return;

    HWND hTaskbar = FindWindow("Shell_TrayWnd", nullptr);
    if (!hTaskbar) return;

    RECT frc, trc, inter;
    GetWindowRect(hwnd, &frc);
    GetWindowRect(hTaskbar, &trc);

    if (IntersectRect(&inter, &frc, &trc)) {
        if (!g_sitting) {
            g_sitting = true;
            g_sitIndex = 0;
            g_sitForward = true;
            g_sitHoldLast = false;

            const int fishH = dpi::fixedAssetSize(layout::kFishH);
            int y = trc.top - fishH + sitBottomOffset();
            SetWindowPos(hwnd, nullptr, frc.left, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            draw(hwnd);
        }
    } else if (g_sitting) {
        g_sitting = false;
        draw(hwnd);
    }
}

void sitOnTaskbar(HWND hwnd) {
    g_sitting = true;
    g_sitIndex = 0;
    g_sitForward = true;
    g_sitHoldLast = false;

    HWND hTaskbar = FindWindow("Shell_TrayWnd", nullptr);
    if (!hTaskbar) return;

    RECT rc;
    GetWindowRect(hTaskbar, &rc);

    RECT fish;
    GetWindowRect(hwnd, &fish);
    int fishW = fish.right - fish.left;

    const int fishH = dpi::fixedAssetSize(layout::kFishH);
    int x = rc.right - fishW;
    int y = rc.top - fishH + sitBottomOffset();

    SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

} // namespace pet
