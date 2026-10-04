#pragma once

// 系统 DPI 缩放，支持跨显示器时的缩放比例变化。
//
// 关键点一：Windows 在进程创建【第一个窗口】时就锁定 DPI 感知模式，
//           之后再设置是无效的。所以 init() 必须是 WinMain 里第一件事。
// 关键点二：只有声明为 Per-Monitor V2 感知，窗口被拖到不同缩放的显示器上时
//           才会收到 WM_DPICHANGED。收到后要调 refresh() 更新比例，
//           并按新的比例重排界面。
namespace dpi {

    // 声明本进程为 Per-Monitor V2 DPI 感知，并记录当前 DPI。
    // 只调用一次，必须在任何 CreateWindow 之前。
    void init();

    // 重新读取系统 DPI 并更新缩放比例。
    // 在处理 WM_DPICHANGED 时调用，然后把界面按新比例重排。
    void refresh();

    // 直接指定 DPI（WM_DPICHANGED 的 wParam 里带了新 DPI，比再查一次更准）
    void setDpi(int newDpi);

    // 当前 DPI（96 = 100%，120 = 125%，144 = 150%，192 = 200%）
    int dpi();

    // 缩放比例，1.0 = 100%
    float factor();

    // 把按 96 DPI 设计的逻辑像素换算成当前 DPI 下的实际像素
    int scale(int logicalPixels);

    float scale(float logicalPixels);

} // namespace dpi
