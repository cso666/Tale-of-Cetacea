#include "wintext.h"
#include "apppath.h"
#include "textutils.h"
#include "layout.h"
#include "dpi.h"

#include <gdiplus.h>
#include <cstdio>
#include <algorithm>

using namespace Gdiplus;

namespace wintext {

const wchar_t* const kFileName = L"window_text.txt";

namespace {

std::wstring g_text;
int g_scroll = 0;      // 当前滚动偏移（像素，>= 0）
int g_viewW = 0;       // 最近一次绘制时的客户区尺寸
int g_viewH = 0;

int clampScroll(int v, int maxS) {
    if (v < 0) return 0;
    if (v > maxS) return maxS;
    return v;
}

std::string readWholeFileUtf8(const std::wstring& absPath) {
    FILE* f = _wfopen(absPath.c_str(), L"rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string buf;
    if (len > 0) {
        buf.resize((size_t)len);
        size_t got = fread(&buf[0], 1, (size_t)len, f);
        buf.resize(got);
    }
    fclose(f);

    // 容错：如果文件是记事本存出来的"UTF-8 带 BOM"，去掉这三字节，
    // 否则 BOM 会被当成正文开头，显示成一个乱码方块。
    if (buf.size() >= 3 &&
        (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF) {
        buf.erase(0, 3);
    }
    return buf;
}

int visibleHeight() {
    int h = g_viewH - dpi::scale(layout::kWindowTextPad) * 2;
    return h < 1 ? 1 : h;
}

// 量出正文在给定宽度下需要多高
int measureContentHeight(int width) {
    if (g_text.empty() || width <= 0) return 0;

    HDC screenDC = GetDC(nullptr);
    if (!screenDC) return 0;
    HDC memDC = CreateCompatibleDC(screenDC);

    Graphics g(memDC);
    FontFamily ff(DSF_TEXT_FONT);
    Font font(&ff, dpi::scale(layout::kWindowTextSize), FontStyleRegular, UnitPixel);
    RectF box(0, 0, (REAL)width, 100000.0f);
    RectF bounds;
    g.MeasureString(g_text.c_str(), -1, &font, box, nullptr, &bounds);

    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    int h = (int)(bounds.Height + 0.5f);
    return h < 0 ? 0 : h;
}

// 不考虑滑块占位时的文字宽度
int textWidthRaw() {
    int w = g_viewW - dpi::scale(layout::kWindowTextPad) * 2;
    return w < 1 ? 1 : w;
}

// 是否需要滑块。
// 必须先按"未扣滑块"的宽度量一次 —— 只在需要时才让出滑块的位置，
// 否则会和 textWidth() 互相调用绕成死循环。
bool needsScrollBar() {
    if (g_text.empty() || g_viewW <= 0 || g_viewH <= 0) return false;
    return measureContentHeight(textWidthRaw()) > visibleHeight();
}

// 文字实际可用宽度：需要滑块时右侧留出滑块的位置
int textWidth() {
    int w = textWidthRaw();
    if (needsScrollBar())
        w -= dpi::scale(layout::kScrollBarW) + dpi::scale(layout::kScrollBarInset) * 2;
    return w < 1 ? 1 : w;
}

} // namespace

bool load() {
    std::string utf8 = readWholeFileUtf8(apppath::of(kFileName));
    g_text = textutils::fromUtf8(utf8);
    g_scroll = 0;
    return !g_text.empty();
}

const std::wstring& get() {
    return g_text;
}

int maxScroll() {
    if (g_text.empty() || g_viewW <= 0 || g_viewH <= 0) return 0;
    int content = measureContentHeight(textWidth());
    int visible = visibleHeight();
    int over = content - visible;
    return over > 0 ? over : 0;
}

bool scrollBy(int delta) {
    int maxS = maxScroll();
    int before = g_scroll;
    g_scroll = clampScroll(g_scroll + delta, maxS);
    return g_scroll != before;
}

ScrollBar scrollBar() {
    ScrollBar sb;
    int maxS = maxScroll();
    if (maxS <= 0 || g_viewW <= 0 || g_viewH <= 0) return sb;

    int barW = dpi::scale(layout::kScrollBarW);
    int inset = dpi::scale(layout::kScrollBarInset);
    int trackX = g_viewW - inset - barW;
    if (trackX < 0) return sb;
    int trackTop = inset;
    int trackBottom = g_viewH - inset;
    if (trackBottom - trackTop < dpi::scale(layout::kScrollThumbMinH)) return sb;

    sb.visible = true;
    sb.track.left = trackX;
    sb.track.top = trackTop;
    sb.track.right = trackX + barW;
    sb.track.bottom = trackBottom;

    int trackH = trackBottom - trackTop;
    int visible = visibleHeight();
    int content = visible + maxS;   // 由定义：content - visible == maxS
    if (content <= 0) content = 1;

    int thumbH = (int)((double)trackH * visible / content + 0.5);
    if (thumbH < dpi::scale(layout::kScrollThumbMinH)) thumbH = dpi::scale(layout::kScrollThumbMinH);
    if (thumbH > trackH) thumbH = trackH;

    int travel = trackH - thumbH;
    int thumbTop = trackTop + (int)((double)travel * g_scroll / maxS + 0.5);

    sb.thumb.left = trackX;
    sb.thumb.top = thumbTop;
    sb.thumb.right = trackX + barW;
    sb.thumb.bottom = thumbTop + thumbH;
    return sb;
}

void scrollFromThumbTop(int trackTop) {
    ScrollBar sb = scrollBar();
    if (!sb.visible) return;

    int trackH = sb.track.bottom - sb.track.top;
    int thumbH = sb.thumb.bottom - sb.thumb.top;
    int travel = trackH - thumbH;
    if (travel <= 0) return;

    int rel = trackTop - sb.track.top;
    if (rel < 0) rel = 0;
    if (rel > travel) rel = travel;

    int maxS = maxScroll();
    g_scroll = clampScroll((int)((double)rel * maxS / travel + 0.5), maxS);
}

int hitTrack(int y) {
    ScrollBar sb = scrollBar();
    if (!sb.visible) return 0;
    if (y < sb.track.top || y > sb.track.bottom) return 0;
    if (y < sb.thumb.top) return -1;
    if (y > sb.thumb.bottom) return 1;
    return 0;
}

void setViewport(int width, int height) {
    g_viewW = width;
    g_viewH = height;
    // 尺寸变了之后滚动位置可能越界，夹回去
    g_scroll = clampScroll(g_scroll, maxScroll());
}

void draw(HDC hdc, const RECT& rect) {
    if (!hdc) return;

    setViewport(rect.right - rect.left, rect.bottom - rect.top);
    if (g_viewW <= 0 || g_viewH <= 0) return;
    if (g_text.empty()) return;

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);

    FontFamily ff(DSF_TEXT_FONT);
    Font font(&ff, dpi::scale(layout::kWindowTextSize), FontStyleRegular, UnitPixel);
    SolidBrush brush(Color(255, 20, 20, 20));

    // 正文区域的左上角（客户区坐标），留白按 DPI 缩放
    int tx = dpi::scale(layout::kWindowTextPad);
    int ty = dpi::scale(layout::kWindowTextPad);

    // 先按"未滚动"的位置裁剪，再用 TranslateTransform 把整体上移。
    // 裁剪矩形和布局矩形必须同在未滚动坐标系里，这样偏移量不会影响换行结果。
    g.SetClip(RectF((REAL)tx, (REAL)ty,
                    (REAL)textWidth(), (REAL)visibleHeight()));
    g.TranslateTransform(0.0f, (REAL)(-g_scroll));

    RectF box((REAL)tx, (REAL)ty, (REAL)textWidth(), 100000.0f);
    g.DrawString(g_text.c_str(), -1, &font, box, nullptr, &brush);

    g.ResetTransform();
    g.ResetClip();

    // 滑块
    ScrollBar sb = scrollBar();
    if (!sb.visible) return;

    SolidBrush trackBrush(Color(255, 235, 235, 235));
    g.FillRectangle(&trackBrush, (REAL)sb.track.left, (REAL)sb.track.top,
                    (REAL)(sb.track.right - sb.track.left),
                    (REAL)(sb.track.bottom - sb.track.top));

    SolidBrush thumbBrush(Color(255, 170, 170, 170));
    g.FillRectangle(&thumbBrush, (REAL)sb.thumb.left, (REAL)sb.thumb.top,
                    (REAL)(sb.thumb.right - sb.thumb.left),
                    (REAL)(sb.thumb.bottom - sb.thumb.top));
}

} // namespace wintext
