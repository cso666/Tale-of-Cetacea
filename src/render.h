#pragma once
#include <windows.h>
#include <string>

// 聊天输入框和气泡的 GDI+ 绘制。
namespace render {

    void inputBox(HWND hwnd, const std::wstring& text);

    // 按文本实际高度算出气泡窗口该多高
    // FIX: 原实现把气泡写死 400x205，长回复直接被裁掉一半，用户以为模型没说完。
    int bubbleHeight(const std::wstring& text, int bubbleW);

    void bubble(HWND hwnd, const std::wstring& text, int bubbleW, int bubbleH);

} // namespace render
