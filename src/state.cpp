#include "state.h"
#include "apppath.h"
#include "textutils.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>

namespace state {

std::mutex g_mutex;
bool       g_requestInFlight = false;
int        g_move = 0;

namespace {

int  g_currentMode = kModePet;
int  g_favor[kModeCount] = {0, 0};
int  g_round[kModeCount] = {0, 0};
std::string g_history[kModeCount];

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

// 原子写：先写临时文件再替换，避免写失败时把原文件截断清零
void writeWholeFileAtomic(const std::wstring& absPath, const std::string& data) {
    std::wstring tmp = absPath + L".tmp";
    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return;
    fwrite(data.c_str(), 1, data.size(), f);
    fclose(f);
    MoveFileExW(tmp.c_str(), absPath.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// atoi 不会抛异常，这里只做范围保护
std::string clampFavorStr(int v) {
    if (v > 100) v = 100;
    if (v < -100) v = -100;
    return std::to_string(v);
}

} // namespace

int currentMode() {
    return g_currentMode;
}

void setCurrentMode(int mode) {
    if (mode >= 0 && mode < kModeCount) g_currentMode = mode;
}

int favorOf(int mode) {
    if (mode < 0 || mode >= kModeCount) return 0;
    return g_favor[mode];
}

void setFavorOf(int mode, int value) {
    if (mode < 0 || mode >= kModeCount) return;
    if (value > 100) value = 100;
    if (value < -100) value = -100;
    g_favor[mode] = value;
}

int roundOf(int mode) {
    if (mode < 0 || mode >= kModeCount) return 0;
    return g_round[mode];
}

void setRoundOf(int mode, int value) {
    if (mode < 0 || mode >= kModeCount) return;
    g_round[mode] = value;
}

void incRound(int mode) {
    if (mode < 0 || mode >= kModeCount) return;
    g_round[mode]++;
}

std::string& historyRef(int mode) {
    if (mode < 0 || mode >= kModeCount) mode = kModePet;
    return g_history[mode];
}

void addFavor(int mode, int delta) {
    if (mode < 0 || mode >= kModeCount) return;
    setFavorOf(mode, g_favor[mode] + delta);
    persist();
}

void appendTurn(int mode, const std::string& role, const std::string& text) {
    // FIX: 原实现把每次回复的 [favor+X] 也一起写进历史，模型看多了这种样例，
    //      最后开始自己在正文里标好感度。现在调用方已把标记剥掉。
    std::string t = textutils::trimAscii(text);
    if (t.empty()) return;

    std::string& h = historyRef(mode);
    if (!h.empty() && h.back() != '\n') h += "\n";
    h += role + ": " + t + "\n";
}

void persist() {
    // 文件格式（纯文本，行式）：
    //   [favor:0]
    //   42
    //   [favor:1]
    //   0
    //   [chat:0]
    //   user: ...
    //   assistant: ...
    //   [chat:1]
    //   ...
    std::string out;
    for (int m = 0; m < kModeCount; m++) {
        out += "[favor:" + std::to_string(m) + "]\n";
        out += clampFavorStr(g_favor[m]) + "\n";
    }
    for (int m = 0; m < kModeCount; m++) {
        out += "[chat:" + std::to_string(m) + "]\n";
        out += g_history[m];
        if (!g_history[m].empty() && g_history[m].back() != '\n') out += "\n";
    }
    writeWholeFileAtomic(apppath::of(L"archive\\history.txt"), out);
}

void load() {
    std::string raw = readWholeFile(apppath::of(L"archive\\history.txt"));

    // 逐行解析，按段标记把内容分派到对应模式
    int curFavor = -1, curChat = -1;
    std::string line;
    for (size_t i = 0; i <= raw.size(); i++) {
        if (i == raw.size() || raw[i] == '\n') {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line.compare(0, 7, "[favor:") == 0) {
                curChat = -1;
                curFavor = atoi(line.c_str() + 7);
                if (curFavor < 0 || curFavor >= kModeCount) curFavor = -1;
            } else if (line.compare(0, 6, "[chat:") == 0) {
                curFavor = -1;
                curChat = atoi(line.c_str() + 6);
                if (curChat < 0 || curChat >= kModeCount) curChat = -1;
            } else if (curFavor >= 0) {
                setFavorOf(curFavor, atoi(line.c_str()));
            } else if (curChat >= 0) {
                g_history[curChat] += line;
                g_history[curChat] += "\n";
            }
            // 不属于任何段的旧格式内容在下面统一兼容

            line.clear();
        } else {
            line += raw[i];
        }
    }

    // 兼容旧存档：早期版本把历史整段裸存，没有 [chat:N] 标记，
    // 好感度则单独放在 archive\favor.txt。这里把它们并入宠物模式。
    bool hasSections = (raw.find("[chat:") != std::string::npos);
    if (!hasSections && !raw.empty() && g_history[kModePet].empty()) {
        g_history[kModePet] = raw;
        if (g_history[kModePet].back() != '\n') g_history[kModePet] += "\n";
    }
    if (g_favor[kModePet] == 0) {
        std::string oldFavor = readWholeFile(apppath::of(L"archive\\favor.txt"));
        if (!oldFavor.empty()) setFavorOf(kModePet, atoi(oldFavor.c_str()));
    }
}

} // namespace state
