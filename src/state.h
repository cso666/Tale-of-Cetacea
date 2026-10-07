#pragma once
#include <string>
#include <mutex>

// 应用运行时状态。
//
// 关键设计：对话分【两种模式】各自独立保存，互不干扰。
//   模式 0 = 宠物模式：使用 Personality.txt（鲸鱼娘），带好感度系统
//   模式 1 = 助手模式：使用 Assistant.txt（通用助手），不发送也不更新好感度
//
// 两套历史、两套好感度、两套摘要轮次计数完全分开。切换模式时互不影响，
// 这样来回切换不会把两个角色的上下文混在一起。
//
// 所有可变状态同时被 UI 线程和网络线程访问，统一用 g_mutex 保护。
namespace state {

    constexpr int kModePet = 0;         // 宠物模式
    constexpr int kModeAssistant = 1;   // 助手模式
    constexpr int kModeCount = 2;

    extern std::mutex g_mutex;
    extern bool       g_requestInFlight;   // 是否已有请求在跑
    extern int        g_move;              // 移动端兼容（预留）

    // 读取 archive\history.txt 与 archive\favor.txt
    void load();

    // 把全部模式的好感度与历史写回 archive\history.txt。
    // 调用前需持有 g_mutex 或确保无并发写。
    void persist();

    // ---- 当前界面选中的模式（由设置层的开关决定）----
    int  currentMode();
    void setCurrentMode(int mode);

    // ---- 按模式访问 ----

    // 好感度（仅宠物模式有意义，助手模式恒为 0 且不会被改动）
    int  favorOf(int mode);
    void setFavorOf(int mode, int value);

    // 好感度加减并 clamp 到 [-100, 100]，然后 persist()
    void addFavor(int mode, int delta);

    // 摘要轮次计数
    int  roundOf(int mode);
    void setRoundOf(int mode, int value);
    void incRound(int mode);

    // 对话历史（纯文本）。返回引用，调用方需持有 g_mutex。
    std::string& historyRef(int mode);

    // 往历史里追加一轮对话。内部会 trim 并跳过空文本
    void appendTurn(int mode, const std::string& role, const std::string& text);

} // namespace state
