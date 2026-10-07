#pragma once
#include <string>

// 界面设置。目前只有一项：第二个页签上的开关。
//
// 语义：
//   开（true）  → 使用 Personality.txt，即原来的角色 system prompt，带好感度系统
//   关（false） → 使用 Assistant.txt，即新的通用助手 system prompt，不发送好感度
//
// 开关切换的是"当前生效的模式"，两种模式的对话历史与好感度各自独立保存。
namespace settings {

    // 文件的键名。值 "1" = 开，"0" = 关
    extern const char* const kKeyUseOriginalPrompt;

    // 从 archive\settings.txt 读取。文件不存在时用默认值（开）。
    void load();

    // 写回 archive\settings.txt
    void save();

    // 开关状态：true = 用原来的 system prompt
    bool useOriginalPrompt();

    // 设置开关。会同步更新 state::currentMode() 并落盘。
    void setUseOriginalPrompt(bool on);

} // namespace settings
