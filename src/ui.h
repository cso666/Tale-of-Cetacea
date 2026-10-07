#pragma once
#include <windows.h>

// 窗口层：桌宠窗口本身、聊天输入框、回复气泡，以及拖放投喂。
namespace ui {

    // 注册并创建桌宠窗口。失败返回 nullptr
    HWND createPetWindow(HINSTANCE hInstance);

    // 桌宠窗口句柄，未创建或已销毁时返回 nullptr
    HWND petWindow();

    // 处理发送/投喂的公共逻辑，供拖放和输入框复用
    void submitUserMessage(HWND petWnd, const char* utf8Msg);

} // namespace ui
