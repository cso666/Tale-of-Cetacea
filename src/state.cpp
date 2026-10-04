#include "state.h"
#include "apppath.h"
#include "textutils.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>

namespace state {

std::mutex  g_mutex;
int         g_favor = 0;
int         chatRound = 0;
std::string chatHistory;
bool        g_requestInFlight = false;

namespace {

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

} // namespace

void saveFavor() {
    if (g_favor > 100) g_favor = 100;
    if (g_favor < -100) g_favor = -100;

    std::wstring p = apppath::of(L"archive\\favor.txt");
    FILE* f = _wfopen(p.c_str(), L"wb");
    if (!f) return;
    std::string s = std::to_string(g_favor);
    fwrite(s.c_str(), 1, s.size(), f);
    fclose(f);
}

void saveHistory() {
    // FIX: 原来直接以 "wb" 打开 history.txt，一旦写失败（磁盘满、文件被占用）
    //      原历史就被截断清零了。改成先写临时文件再原子替换。
    std::wstring p = apppath::of(L"archive\\history.txt");
    std::wstring tmp = p + L".tmp";

    FILE* f = _wfopen(tmp.c_str(), L"wb");
    if (!f) return;
    fwrite(chatHistory.c_str(), 1, chatHistory.size(), f);
    fclose(f);
    MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING);
}

void load() {
    std::string fav = readWholeFile(apppath::of(L"archive\\favor.txt"));
    if (!fav.empty()) {
        g_favor = atoi(fav.c_str());
        if (g_favor > 100) g_favor = 100;
        if (g_favor < -100) g_favor = -100;
    }

    chatHistory = readWholeFile(apppath::of(L"archive\\history.txt"));
}

void addFavor(int delta) {
    g_favor += delta;
    if (g_favor > 100) g_favor = 100;
    if (g_favor < -100) g_favor = -100;
    saveFavor();
}

void appendTurn(const std::string& role, const std::string& text) {
    // FIX: 原实现把每次回复的 [favor+X] 也一起写进历史，模型看多了这种样例，
    //      最后开始自己在正文里标好感度。现在调用方已把标记剥掉。
    std::string t = textutils::trimAscii(text);
    if (t.empty()) return;
    if (!chatHistory.empty() && chatHistory.back() != '\n') chatHistory += "\n";
    chatHistory += role + ": " + t + "\n";
}

} // namespace state
