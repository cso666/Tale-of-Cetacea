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
    // 正文字号（96 DPI 下的设计值）。
    // 实际显示值 = 本值 × (当前DPI / 96)，即走 dpi::fixedAssetSize()。
    // 例如本值 12.7 在 150% 缩放下约等于 19px；若想微调只改这一处。
    constexpr float kWindowTextSize = 12.7f;

    // 自绘竖向滑块（内容超出可视区时出现）
    constexpr int kScrollBarW = 10;   // 滑块条宽度
    constexpr int kScrollBarInset = 4; // 距窗口右/上/下边缘的距离
    constexpr int kScrollThumbMinH = 28; // 滑块最短高度，太短就抓不住了
    constexpr int kWheelScrollStep = 60; // 滚轮一格滚多少像素

    // ---- 左侧页签栏 ----
    // 两栏文字，对应两个界面：0 = 显示 window_text.txt，1 = 空白
    constexpr int kTabCount = 2;
    constexpr int kSidebarW = 100;      // 栏宽（96 DPI 设计值，150% 下约 150px）
    constexpr int kTabTop = 60;         // 第一条页签距窗口顶部
    constexpr int kTabH = 46;           // 每条页签高度
    constexpr int kTabGap = 6;          // 页签之间的空隙
    constexpr int kSidebarTextPad = 18; // 页签文字距栏左边
    constexpr float kSidebarFontSize = 12.0f; // 页签字号（96 DPI 设计值）
    constexpr float kSidebarMarkW = 3.0f;     // 选中页签的左侧竖条宽度

    // ---- 第二个页签里的开关 ----
    constexpr int kSwitchX = 32;         // 开关距内容区左边
    constexpr int kSwitchY = 64;         // 开关距内容区顶部
    constexpr int kSwitchW = 56;         // 开关宽度
    constexpr int kSwitchH = 28;         // 开关高度
    constexpr int kSwitchLabelGap = 20;  // 开关到下面说明文字的距离
    constexpr int kSwitchTextW = 260;    // 说明文字的排版宽度
    constexpr float kSwitchFontSize = 11.0f; // 说明文字字号（96 DPI 设计值）

    // ---- 桌宠的右键菜单命令 ID ----
    constexpr int kMenuOpenMain = 1001;  // 打开主窗口
    constexpr int kMenuQuit     = 1002;  // 关闭
}

// 正文与页签使用的字体。字号见 layout::kWindowTextSize / kSidebarFontSize
#define DSF_TEXT_FONT L"Microsoft YaHei"

// 自定义消息：网络线程拿到回复后，通知 UI 线程弹气泡
#define WM_SHOW_BUBBLE (WM_APP + 1)
