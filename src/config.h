#pragma once
#include <string>

// 运行期配置。在 main 启动时填充，之后只读。
namespace config {
    extern std::string apiKey;      // DeepSeek API key
    extern std::string personality; // Personality.txt 的内容
}
