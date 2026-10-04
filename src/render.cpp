#include "render.h"
#include "layout.h"
#include "apppath.h"
#include "dpi.h"
#include <gdiplus.h>
#include <cmath>

using namespace Gdiplus;

namespace render {

namespace {

// GDI+ 的文档没有说明只能通过工厂方法拿 StringFormat。MinGW 的头文件里
// StringFormat(INT formatFlags = 0, LANGID = LANG_NEUTRAL) 是 public 的，
// 默认 flags=0（允许换行、不裁剪），和传 nullptr 的行为一致。
// 测量与绘制必须用同样的 format，否则算出来的高度和实际排版对不上。
const int kTextSizePx = 16;   // 逻辑像素，用前要过 dpi::scale
const wchar_t* kBubbleFont = L"幼圆";
const wchar_t* kInputFont = L"Microsoft YaHei";

} // namespace

int bubbleHeight(const std::wstring& text, int bubbleW) {
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return dpi::scale(layout::kBubbleMinH);
    HDC memDC = CreateCompatibleDC(screenDC);

    // 字号和留白都要跟着 DPI 走，否则量出来的高度和实际排版对不上
    const int padX = dpi::scale(layout::kBubblePadX);
    const int padY = dpi::scale(layout::kBubblePadY);

    Graphics g(memDC);
    FontFamily ff(kBubbleFont);
    Font font(&ff, dpi::scale((float)kTextSizePx), FontStyleRegular, UnitPixel);
    RectF layoutRect(0, 0, (REAL)(bubbleW - padX * 2), 4000.0f);
    RectF bounds;
    g.MeasureString(text.c_str(), -1, &font, layoutRect, nullptr, &bounds);

    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    int h = (int)ceil(bounds.Height) + padY * 2;
    if (h < dpi::scale(layout::kBubbleMinH)) h = dpi::scale(layout::kBubbleMinH);
    if (h > dpi::scale(layout::kBubbleMaxH)) h = dpi::scale(layout::kBubbleMaxH);
    return h;
}

void inputBox(HWND hwnd, const std::wstring& text) {
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return;
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = dpi::scale(layout::kChatInputW);
    bmi.bmiHeader.biHeight = -dpi::scale(layout::kChatInputH);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    Image img(apppath::of(L"input\\input.png").c_str());
    if (img.GetLastStatus() == Ok)
        g.DrawImage(&img, 0, 0, dpi::scale(layout::kChatInputW), dpi::scale(layout::kChatInputH));

    FontFamily ff(kInputFont);
    Font font(&ff, dpi::scale(18.0f), FontStyleRegular, UnitPixel);
    SolidBrush brush(Color(255, 40, 40, 40));

    g.DrawString(text.c_str(), -1, &font, PointF(dpi::scale(30.0f), dpi::scale(35.0f)), &brush);

    POINT ptSrc = {0, 0};
    SIZE sz = {dpi::scale(layout::kChatInputW), dpi::scale(layout::kChatInputH)};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, screenDC, nullptr, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

void bubble(HWND hwnd, const std::wstring& text, int bubbleW, int bubbleH) {
    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return;
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bubbleW;
    bmi.bmiHeader.biHeight = -bubbleH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return;
    }
    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    Image img(apppath::of(L"output\\output.png").c_str());
    if (img.GetLastStatus() == Ok)
        g.DrawImage(&img, 0, 0, bubbleW, bubbleH);

    FontFamily ff(kBubbleFont);
    Font font(&ff, dpi::scale((float)kTextSizePx), FontStyleRegular, UnitPixel);
    SolidBrush brush(Color(255, 40, 40, 40));

    // 留白同样要按 DPI 缩放，否则高 DPI 下文字会顶到气泡边缘；
    // 而且必须和 bubbleHeight() 里的算法保持一致。
    const int padX = dpi::scale(layout::kBubblePadX);
    const int padY = dpi::scale(layout::kBubblePadY);
    StringFormat fmt;
    RectF layoutRect((REAL)padX, (REAL)padY,
                     (REAL)(bubbleW - padX * 2),
                     (REAL)(bubbleH - padY * 2));
    g.DrawString(text.c_str(), -1, &font, layoutRect, &fmt, &brush);

    POINT ptSrc = {0, 0};
    SIZE sz = {bubbleW, bubbleH};
    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(hwnd, screenDC, nullptr, &sz, memDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDC, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

} // namespace render
