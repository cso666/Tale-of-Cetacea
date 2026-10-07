#pragma once
#include <string>

// 运行期配置。在 main 启动时填充，之后只读。
namespace config {
    extern std::string apiKey;      // API_key.txt 的内容
    extern std::string personality; // Personality.txt 的内容（宠物模式的 system prompt）
    extern std::string assistant;   // Assistant.txt 的内容（助手模式的 system prompt）
}
