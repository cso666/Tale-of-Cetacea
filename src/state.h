#pragma once
#include <string>
#include <mutex>

// 应用运行时状态：好感度、对话历史、请求在途标记。
// 这些变量同时被 UI 线程和网络线程访问，所以统一在这里定义，并把互斥量一并导出，
// 由调用方用 lock_guard 保护"读-改-写"序列。
namespace state {

    extern std::mutex g_mutex;            // 保护下面所有变量
    extern int         g_favor;           // 好感度 -100 ~ 100
    extern int         chatRound;         // 距上次摘要经过了多少轮
    extern std::string chatHistory;       // 对话历史（纯文本）
    extern bool        g_requestInFlight; // 是否已有请求在跑

    // 读取 archive\favor.txt 与 archive\history.txt
    void load();

    // 把好感度 / 历史写回存档。调用前需持有 g_mutex 或确保无并发写
    void saveFavor();
    void saveHistory();

    // 好感度加减并 clamp 到 [-100, 100]，然后存盘
    void addFavor(int delta);

    // 往历史里追加一轮对话。内部会 trim 并跳过空文本
    void appendTurn(const std::string& role, const std::string& text);

} // namespace state
