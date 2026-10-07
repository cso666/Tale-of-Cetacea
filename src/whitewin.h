#pragma once
#include <windows.h>

// 启动时弹出的文字窗口：普通带边框窗口（标题留空），左边一栏两个页签。
//
// 之所以没有子窗口控件，是为了让它保持一块干净的白板 —— 页签、开关、滑块
// 全部是自绘的。
namespace whitewin {

    // 注册并创建窗口。失败返回 nullptr
    HWND createWindow(HINSTANCE hInstance, int nCmdShow);

    // 当前窗口句柄；已被关闭时返回 nullptr
    HWND window();

    // 窗口已被关掉时重新建一个（用保存的 HINSTANCE），已经存在则直接置前。
    // 返回是否成功。
    bool openOrFocus();

    // 保存 HINSTANCE，供 openOrFocus() 重建窗口时使用。
    // 由 createWindow() 自动调用，不必手动。
    void rememberInstance(HINSTANCE hInstance);

    // 真正退出整个程序：销毁本窗口与桌宠，并结束消息循环。
    void requestQuit();

} // namespace whitewin
