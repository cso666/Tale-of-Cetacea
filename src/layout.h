#pragma once
#include <windows.h>
#include <string>

// 窗口与气泡的几何常量，集中放这里，免得各文件里散落魔数。
namespace layout {
    // 桌宠窗口尺寸（需与 blink/sit/eatfile/token 的帧图一致）
    constexpr int kFishW = 180;
    constexpr int kFishH = 316;

    // 聊天气泡
    constexpr int kBubbleW = 400;
    constexpr int kBubbleMinH = 120;
    constexpr int kBubbleMaxH = 600;
    constexpr int kBubblePadX = 25;
    constexpr int kBubblePadY = 20;

    // 桌宠旁边弹出的聊天输入框（一个 400x100 的贴图窗口）
    constexpr int kChatInputW = 400;
    constexpr int kChatInputH = 100;

    // 聊天输入框里"发送"按钮的命中区域
    constexpr RECT kSendRect = {295, 30, 365, 70};

    // 动画定时器间隔（毫秒）
    constexpr UINT kTimerAnim = 1;
    constexpr UINT kTimerSitWatch = 2;
    constexpr int  kAnimIntervalMs = 250;
    constexpr int  kSitWatchIntervalMs = 50;

    // ---- 启动时弹出的文字窗口 ----
    // 注意：这是"客户区"尺寸（真正能画字的像素点），
    // 不含标题栏和边框。建窗时用 AdjustWindowRect 反推外框。
    // 用 const int 而不是 constexpr：要给 AdjustWindowRect 传地址取 RECT，
    // constexpr 会导致绑定到非 const 引用时触发窄化/限定转换问题。
    const int kMainW = 800;
    const int kMainH = 900;

    // 白窗口上正文的排版
    // 注意用 float 而不是 GDI+ 的 REAL：REAL 定义在 <gdiplus.h> 里，
    // 而本头文件只依赖 <windows.h>，不该把 GDI+ 拖进来。
    // GDI+ 里 REAL 就是 float，赋值时自动转换。
    constexpr int   kWindowTextPad = 24;      // 四边留白
    constexpr float kWindowTextSize = 14.0f;  // 字号（像素）

    // 自绘竖向滑块（内容超出可视区时出现）
    constexpr int kScrollBarW = 10;   // 滑块条宽度
    constexpr int kScrollBarInset = 4; // 距窗口右/上/下边缘的距离
    constexpr int kScrollThumbMinH = 28; // 滑块最短高度，太短就抓不住了
    constexpr int kWheelScrollStep = 60; // 滚轮一格滚多少像素
}

// 正文使用的字体。字号在 layout::kWindowTextSize
#define DSF_TEXT_FONT L"Microsoft YaHei"

// 自定义消息：网络线程拿到回复后，通知 UI 线程弹气泡
#define WM_SHOW_BUBBLE (WM_APP + 1)
