#pragma once
#include <windows.h>

// 启动时弹出的空白窗口：普通带边框窗口，纯白背景，里面什么都没有。
//
// 之所以"没有控件"，是为了让它真的只是一块白板 —— 不注册子窗口、不响应
// 任何命令，只有系统给的标题栏/边框/最小化/最大化/关闭。
namespace whitewin {

    // 注册并创建空白窗口。失败返回 nullptr
    HWND createWindow(HINSTANCE hInstance, int nCmdShow);

} // namespace whitewin
