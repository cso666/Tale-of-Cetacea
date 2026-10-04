#include "chat.h"
#include "config.h"
#include "state.h"
#include "http.h"
#include "jsonutils.h"
#include "textutils.h"
#include "layout.h"
#include <cstdlib>

namespace chat {

namespace {

// 模型名必须是 deepseek-flash。deepseek-chat / deepseek-coder 是旧接口的遗留
// 名字，在当前 API 上已经不存在，写错会直接 400。
const wchar_t* kModel = L"deepseek-flash";

// 思考模式默认是开启的（effort=high），对桌宠是灾难：模型会先吐一大段思维链
// 再给正文，延迟和输出 token 都翻好几倍，用户只看到气泡迟迟不弹。
// 所以显式关掉。
const wchar_t* kThinkingParam = L"\"thinking\":{\"type\":\"disabled\"},";

const int kTimeoutMs = 60000;
const int kMaxFavorDelta = 25;
const int kSummaryAfterRounds = 5;

std::string buildBodyPrefix() {
    std::string s = "{\"model\":\"" + textutils::toUtf8(kModel) + "\",";
    s += textutils::toUtf8(kThinkingParam);
    s += "\"messages\":[";
    return s;
}

} // namespace

int extractFavorTag(std::string& reply) {
    const char* key = "[favor";
    size_t fp = reply.find(key);
    if (fp == std::string::npos) return 0;
    size_t fe = reply.find(']', fp);
    if (fe == std::string::npos) return 0;

    const char* p = reply.c_str() + fp + 6;
    // 容忍 [favor+5] / [favor-5] / [favor 5] / [favor: 5] 几种写法
    while (*p && *p != ']' && (*p == ' ' || *p == ':' || *p == '=')) ++p;

    char* end = nullptr;
    long v = strtol(p, &end, 10);
    if (end == p) return 0;   // 根本没解析出数字，别瞎加

    if (v > kMaxFavorDelta)  v = kMaxFavorDelta;
    if (v < -kMaxFavorDelta) v = -kMaxFavorDelta;

    reply.erase(fp, fe - fp + 1);
    return (int)v;
}

void summarizeHistory(const std::string& key) {
    std::string snapshot;
    {
        std::lock_guard<std::mutex> lk(state::g_mutex);
        if (state::chatHistory.empty()) return;
        snapshot = state::chatHistory;
    }

    std::string body = buildBodyPrefix();
    body += "{\"role\":\"system\",\"content\":\"你是对话摘要器。"
            "只输出两句话的客观摘要，使用第三人称，不模仿任何角色语气，不加动作描写。\"},";
    body += "{\"role\":\"user\",\"content\":\"" + jsonutils::escape(snapshot) + "\"}";
    body += "],\"stream\":false,\"max_tokens\":512}";

    http::Result r = http::postJson(body, key, kTimeoutMs);
    if (!r.ok || r.status != 200) return;

    std::string summary;
    if (!jsonutils::findString(r.body, "content", summary)) return;
    summary = textutils::trimAscii(summary);
    if (summary.empty()) return;

    std::lock_guard<std::mutex> lk(state::g_mutex);
    state::chatHistory = "[对话摘要]\n" + summary + "\n\n[最近对话]\n";
    state::chatRound = 0;
    state::saveHistory();
}

void ask(HWND hwnd, std::string key, std::string userMsg) {
    // 用户消息在 ui 层发送时就已经写入历史，这里只做快照
    std::string historySnapshot;
    std::string systemPrompt;
    int favorSnapshot = 0;
    bool needSummary = false;

    {
        std::lock_guard<std::mutex> lk(state::g_mutex);
        favorSnapshot = state::g_favor;
        needSummary = (state::chatRound >= kSummaryAfterRounds && !state::chatHistory.empty());
    }

    // 网络请求一律在锁外，否则会卡住 UI 线程的 WM_TIMER 和存档写入
    if (needSummary) summarizeHistory(key);

    {
        std::lock_guard<std::mutex> lk(state::g_mutex);
        historySnapshot = state::chatHistory;
    }

    // FIX: 原实现把好感度提示和用户消息拼进同一条 user message，模型分不清
    //      "谁在说话"。现在好感度写进 system，历史单独作为一条 user 消息。
    std::string favorLine = "[system]当前好感度：" + std::to_string(favorSnapshot) +
        "。请在回复的最末尾加上 [favor+X] 或 [favor-X]（X 为 -25 到 +25 的整数），"
        "表示这次对话让你好感度的变化。除了这个标记，不要在正文里提到好感度系统。";
    systemPrompt = config::personality + "\n\n" + favorLine;

    std::string body = buildBodyPrefix();
    body += "{\"role\":\"system\",\"content\":\"" + jsonutils::escape(systemPrompt) + "\"}";
    if (!historySnapshot.empty()) {
        body += ",{\"role\":\"user\",\"content\":\"[以下是此前的对话记录，仅供你参考上下文]\\n" +
                jsonutils::escape(historySnapshot) + "\"}";
    }
    body += ",{\"role\":\"user\",\"content\":\"" + jsonutils::escape(userMsg) + "\"}";
    body += "],\"stream\":false,\"max_tokens\":2048}";

    http::Result r = http::postJson(body, key, kTimeoutMs);

    std::wstring display;
    if (!r.ok) {
        display = L"（连不上服务器：" + textutils::fromUtf8(r.error) + L"）";
    } else {
        std::string content;
        if (r.status == 200 && jsonutils::findString(r.body, "content", content)) {
            // 解析放锁外，锁内只做赋值和存盘
            int delta = extractFavorTag(content);
            content = textutils::trimAscii(content);
            if (content.empty()) content = "……";

            {
                std::lock_guard<std::mutex> lk(state::g_mutex);
                state::addFavor(delta);
                state::appendTurn("assistant", content);   // 已剥掉 favor 标记
                state::chatRound++;
                state::saveHistory();
            }
            display = textutils::fromUtf8(content);
        } else {
            // FIX: 原来 401 / 402 / 400 全被吞掉，只显示"请求失败"。
            //      现在把 HTTP 状态码和接口返回的 error.message 带给用户。
            std::string detail;
            jsonutils::findString(r.body, "message", detail);
            if (detail.empty()) detail = "HTTP " + std::to_string(r.status);
            display = L"（接口返回错误：" + textutils::fromUtf8(detail) + L"）";
        }
    }

    std::wstring* pReply = new std::wstring(display);
    PostMessage(hwnd, WM_SHOW_BUBBLE, 0, (LPARAM)pReply);
}

} // namespace chat
