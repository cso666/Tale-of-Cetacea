#pragma once

// DPI 缩放。
//
// 【本项目为什么需要两个方向相反的换算】
// 本程序声明为 Per-Monitor V2 DPI 感知，因此 Windows 不再替我们做任何拉伸，
// 我们提交多少物理像素，屏幕上就是多少物理像素。而不同的东西想要的物理量不同：
//
//   · 窗口客户区 —— 想要"逻辑尺寸不变"，所以 100%→150% 时要把物理尺寸缩小：
//                   800 逻辑 × (96/144) = 533 物理   → 用 windowSize()
//
//   · 桌宠帧图 / 字号 —— 想要"看起来的物理大小不变"。这些是按 96 DPI 画的固定
//                   像素图像和字形，要维持原有观感就得把物理尺寸放大：
//                   180 × (144/96) = 270            → 用 fixedAssetSize()
//
// 注意：在改动 DPI 感知之前，进程是"不感知"的，Windows 会自动把整个窗口放大
// 1.5 倍。改成感知之后这个自动放大就没了，所以桌宠必须用 fixedAssetSize()
// 自己补回来，否则会显得突然变小。
//
// 关键点一：Windows 在进程创建【第一个窗口】时就锁定 DPI 感知模式，
//           之后再设置是无效的。所以 init() 必须是 WinMain 里第一件事。
// 关键点二：只有声明为 Per-Monitor V2 感知，窗口被拖到不同缩放的显示器上时
//           才会收到 WM_DPICHANGED。收到后要调 setDpi() 更新比例，
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

    // 窗口尺寸用的换算因子 = 96 / 当前DPI。
    // 100% → 1.0，150% → 0.6667，200% → 0.5（小于 1，即缩小）
    float factor();

    // 【窗口尺寸】把按 96 DPI 设计的窗口尺寸换算成实际物理像素。
    // 150% 下 windowSize(800) == 533，也就是"窗口变小"。
    int windowSize(int designPixels);

    // 【固定像素资源】把按 96 DPI 设计的图像/字号尺寸换算成实际物理像素。
    // 150% 下 fixedAssetSize(180) == 270，也就是"维持原有观感大小"。
    // 桌宠帧尺寸、正文字号、气泡字号该用它。
    int fixedAssetSize(int designPixels);

    float fixedAssetSize(float designPixels);

} // namespace dpi
