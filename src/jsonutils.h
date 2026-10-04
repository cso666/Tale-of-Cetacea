#pragma once
#include <string>

// 极简 JSON 工具。
//
// FIX: 原来的实现是 result.find("\"content\":\"") 然后找下一个引号，有两个硬伤：
//      1) 模型回复里只要出现 \" 或 \n，内容就被截断；
//      2) 思考模式下 content 可能是 null，正文在 reasoning_content 里，
//         这种响应会直接解析失败。
//      现在按 JSON 字符串规则完整解码，并且识别 null。
namespace jsonutils {

    // 转义成 JSON 字符串字面量的内容（不含首尾引号），输入是 UTF-8 原始字节
    std::string escape(const std::string& s);

    // 在 json 里找顶层 "key": "..."，解码后写入 out。
    // 值为 null、不是字符串、或 key 不存在时返回 false。
    bool findString(const std::string& json, const std::string& key, std::string& out);

} // namespace jsonutils
