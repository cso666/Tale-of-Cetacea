#include "jsonutils.h"

namespace jsonutils {

namespace {

// 把 Unicode 码点按 UTF-8 写进 out
void appendUtf8(std::string& out, unsigned int cp) {
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

bool hex4(const std::string& j, size_t at, unsigned int& v) {
    if (at + 4 > j.size()) return false;
    v = 0;
    for (int k = 0; k < 4; ++k) {
        char h = j[at + k];
        v <<= 4;
        if (h >= '0' && h <= '9')      v |= (unsigned int)(h - '0');
        else if (h >= 'a' && h <= 'f') v |= (unsigned int)(h - 'a' + 10);
        else if (h >= 'A' && h <= 'F') v |= (unsigned int)(h - 'A' + 10);
        else return false;
    }
    return true;
}

// 解析一个 JSON 字符串字面量。p 指向开引号，返回时 p 指向闭引号之后。
std::string parseString(const std::string& j, size_t& p) {
    std::string out;
    if (p >= j.size() || j[p] != '"') return out;
    ++p;
    while (p < j.size()) {
        char c = j[p++];
        if (c == '"') break;
        if (c != '\\') { out += c; continue; }
        if (p >= j.size()) break;
        char e = j[p++];
        switch (e) {
        case 'n':  out += '\n'; break;
        case 't':  out += '\t'; break;
        case 'r':  out += '\r'; break;
        case 'b':  out += '\b'; break;
        case 'f':  out += '\f'; break;
        case '"':  out += '"';  break;
        case '\\': out += '\\'; break;
        case '/':  out += '/';  break;
        case 'u': {
            unsigned int cp = 0;
            if (hex4(j, p, cp)) {
                p += 4;
                // 处理 UTF-16 代理对
                if (cp >= 0xD800 && cp <= 0xDBFF && p + 6 <= j.size() &&
                    j[p] == '\\' && j[p + 1] == 'u') {
                    unsigned int lo = 0;
                    if (hex4(j, p + 2, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        p += 6;
                    }
                }
                appendUtf8(out, cp);
            } else {
                out += e;   // \u 后面不是合法十六进制，原样保留
            }
            break;
        }
        default: out += e; break;
        }
    }
    return out;
}

} // namespace

std::string escape(const std::string& s) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() + 16);
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        default:
            if (c < 0x20) {
                // JSON 不允许裸控制字符，用 \u00XX 兜底
                out += "\\u00";
                out += hex[(c >> 4) & 0xF];
                out += hex[c & 0xF];
            } else {
                out += (char)c;   // UTF-8 多字节原样透传
            }
            break;
        }
    }
    return out;
}

bool findString(const std::string& j, const std::string& key, std::string& out) {
    std::string pat = "\"" + key + "\"";
    size_t k = j.find(pat);
    if (k == std::string::npos) return false;
    size_t c = j.find(':', k + pat.size());
    if (c == std::string::npos) return false;
    size_t p = c + 1;
    while (p < j.size() && (j[p] == ' ' || j[p] == '\t' || j[p] == '\n' || j[p] == '\r')) ++p;
    // "content":null 在推理类模型里很常见，遇到就当成没有，
    // 否则会把后面的文本误当内容
    if (p + 4 <= j.size() && j.compare(p, 4, "null") == 0) return false;
    if (p >= j.size() || j[p] != '"') return false;
    out = parseString(j, p);
    return true;
}

} // namespace jsonutils
