#pragma once
#include <string>
#include <windows.h>

// 与模型的对话逻辑：构造请求、解析回复、抽取好感度标记。
//
// 两种模式（见 state.h）走同一套网络与解析代码，只在两点上不同：
//   · 系统提示词不同（Personality.txt / Assistant.txt）
//   · 宠物模式带好感度系统；助手模式既不发送好感度提示，也不解析 [favor] 标记
//
// 注意：这些函数含网络请求，必须在后台线程调用，不要在 UI 线程直接调。
namespace chat {

    // 从回复里剥出 [favor+X] 标记并返回增量，同时把标记从 reply 里删掉。
    // FIX: 原实现用 atoi(tag.c_str() + 6)，换个标记格式就解析出垃圾值，而且没有
    //      范围校验 —— 模型写 [favor+999] 一句就能把好感度顶满。现在用 strtol
    //      并 clamp 到 ±25，解析不出数字就返回 0。
    int extractFavorTag(std::string& reply);

    // 把指定模式的对话压缩成两三句摘要，替换掉该模式历史的正文部分。
    // 两种模式都会走摘要，切模式不影响。
    void summarize(int mode, const std::string& key);

    // 发一轮完整对话，拿到回复后向 hwnd 投递 WM_SHOW_BUBBLE。
    // mode 决定用哪套 system prompt、读写哪一套历史。
    void ask(HWND hwnd, int mode, std::string key, std::string userMsg);

} // namespace chat
