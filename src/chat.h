#pragma once
#include <string>
#include <windows.h>

// 与模型的对话逻辑：构造请求、解析回复、抽取好感度标记。
// 注意：AskDeepSeek 会发网络请求，必须在后台线程调用，不要在 UI 线程直接调。
namespace chat {

    // 从回复里剥出 [favor+X] 标记并返回增量，同时把标记从 reply 里删掉。
    // FIX: 原实现用 atoi(tag.c_str() + 6)，换个标记格式就解析出垃圾值，而且没有
    //      范围校验 —— 模型写 [favor+999] 一句就能把好感度顶满。现在用 strtol
    //      并 clamp 到 ±25，解析不出数字就返回 0。
    int extractFavorTag(std::string& reply);

    // 把当前对话压缩成两三句摘要，替换掉 chatHistory 的正文部分
    void summarizeHistory(const std::string& key);

    // 发一轮完整对话，拿到回复后向 hwnd 投递 WM_SHOW_BUBBLE。
    // 必须在后台线程调用（含网络请求）。
    void ask(HWND hwnd, std::string key, std::string userMsg);

} // namespace chat
