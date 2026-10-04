#pragma once
#include <string>

// UTF-8 <-> UTF-16 转换，以及 ASCII 裁剪。
// FIX: 原实现用 std::wstring(key.begin(), key.end()) 逐字节扩展来把 UTF-8 转成
//      宽字符，只要字符串里有非 ASCII 内容（中文 key、中文人设）就会变乱码。
namespace textutils {

    std::string   toUtf8(const std::wstring& w);
    std::wstring  fromUtf8(const std::string& s);

    // 只裁掉首尾的 ASCII 空白，不动中间和 UTF-8 多字节内容
    std::string   trimAscii(const std::string& s);

} // namespace textutils
