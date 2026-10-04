#pragma once
#include <string>
#include <windows.h>

// 与 DeepSeek API 的 HTTP 通信。
//
// FIX: 原实现不检查任何返回值、不看 HTTP 状态码，网络失败和模型报错都被糊成
//      一句"请求失败"，用户完全不知道为什么桌宠不理人。
namespace http {

    struct Result {
        bool        ok = false;   // 是否拿到了完整响应体（不代表 HTTP 200）
        DWORD       status = 0;   // HTTP 状态码
        std::string body;         // 响应体原文
        std::string error;        // 网络层错误描述，ok==false 时有意义
    };

    // 向 api.deepseek.com 发一个 POST，超时 timeoutsMs 毫秒
    Result postJson(const std::string& body, const std::string& key, int timeoutsMs);

} // namespace http
