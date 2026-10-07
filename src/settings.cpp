#include "settings.h"
#include "state.h"
#include "apppath.h"

#include <windows.h>
#include <cstdio>
#include <mutex>

namespace settings {

const char* const kKeyUseOriginalPrompt = "use_original_prompt";

namespace {

std::mutex g_mutex;
bool g_useOriginal = true;   // 默认开：保持改动前的行为

std::string readWholeFile(const std::wstring& absPath) {
    FILE* f = _wfopen(absPath.c_str(), L"rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string buf;
    if (len > 0) {
        buf.resize((size_t)len);
        size_t got = fread(&buf[0], 1, (size_t)len, f);
        buf.resize(got);
    }
    fclose(f);
    return buf;
}

void writeWholeFile(const std::wstring& absPath, const std::string& data) {
    std::wstring tmp = absPath + L".tmp";
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return;
    fwrite(data.c_str(), 1, data.size(), f);
    fclose(f);
    MoveFileExW(tmp.c_str(), absPath.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// 把开关状态同步到"当前生效模式"
void syncModeLocked() {
    state::setCurrentMode(g_useOriginal ? state::kModePet : state::kModeAssistant);
}

} // namespace

bool useOriginalPrompt() {
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_useOriginal;
}

void setUseOriginalPrompt(bool on) {
    std::lock_guard<std::mutex> lk(g_mutex);
    g_useOriginal = on;
    syncModeLocked();
    save();
}

void load() {
    std::lock_guard<std::mutex> lk(g_mutex);

    std::string raw = readWholeFile(apppath::of(L"archive\\settings.txt"));
    std::string key = std::string(kKeyUseOriginalPrompt) + "=";
    size_t k = raw.find(key);
    if (k != std::string::npos) {
        size_t v = k + key.size();
        // 只取到行尾
        size_t e = raw.find_first_of("\r\n", v);
        std::string val = raw.substr(v, (e == std::string::npos) ? std::string::npos : e - v);
        if (val == "0") g_useOriginal = false;
        else if (val == "1") g_useOriginal = true;
    }

    syncModeLocked();
}

void save() {
    // 调用方通常已持有 g_mutex；persist 不依赖它，这里直接写
    std::string out;
    out += std::string(kKeyUseOriginalPrompt) + "=";
    out += (g_useOriginal ? "1" : "0");
    out += "\n";
    writeWholeFile(apppath::of(L"archive\\settings.txt"), out);
}

} // namespace settings
