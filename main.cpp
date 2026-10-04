// DeepSeek Fish - desktop pet
// 程序入口：载入配置与存档 → 初始化 GDI+ → 建窗 → 消息循环 → 退出时存盘。
//
// 模块划分：
//   layout.h      几何常量与自定义消息
//   apppath       资源路径解析（基于 exe 目录）
//   textutils     UTF-8 <-> UTF-16、ASCII trim
//   jsonutils     极简 JSON 转义 / 取值
//   http          WinHTTP 请求
//   state         好感度、对话历史、互斥量、存档读写
//   chat          与模型对话、摘要、好感度标记解析
//   pet           帧资源、动画推进、坐姿、绘制、任务栏吸附
//   render        输入框与气泡的 GDI+ 绘制
//   ui            窗口过程、拖放、气泡、输入框
#include "config.h"
#include "state.h"
#include "pet.h"
#include "ui.h"
#include "whitewin.h"
#include "wintext.h"
#include "apppath.h"
#include "textutils.h"
#include "layout.h"
#include "dpi.h"

#include <gdiplus.h>
#include <windows.h>
#include <string>

using namespace Gdiplus;

namespace config {
    std::string apiKey;
    std::string personality;
}

namespace {

// 帧数量，需与磁盘上的图片数量一致
const int kBlinkFrames = 40;
const int kSitFrames = 24;
const int kEatFrames = 24;
const int kTokenFrames = 24;

std::string readTextFile(const std::wstring& absPath) {
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

// 载入 API key。
// 优先读环境变量 DEEPSEEK_API_KEY，这样可以把明文 key 从仓库里拿掉。
// FIX: 原实现读不到 API_key.txt 就 return 0，程序一声不响直接退出，
//      用户完全不知道发生了什么。现在给出明确提示。
bool loadApiKey() {
    wchar_t envBuf[512] = {};
    DWORD n = GetEnvironmentVariableW(L"DEEPSEEK_API_KEY", envBuf, 512);
    if (n > 0 && n < 512) {
        config::apiKey = textutils::toUtf8(std::wstring(envBuf, n));
        config::apiKey = textutils::trimAscii(config::apiKey);
    }

    if (config::apiKey.empty()) {
        config::apiKey = textutils::trimAscii(readTextFile(apppath::of(L"API_key.txt")));
    }

    if (config::apiKey.empty()) {
        MessageBoxW(nullptr,
            L"找不到可用的 API key。\n\n"
            L"请把 DeepSeek 的 key 放进程序目录下的 API_key.txt，\n"
            L"或者设置环境变量 DEEPSEEK_API_KEY。",
            L"DeepSeek Fish", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}

void loadPersonality() {
    config::personality = readTextFile(apppath::of(L"Personality.txt"));
    if (config::personality.empty()) {
        MessageBoxW(nullptr, L"Personality.txt 读取失败或为空，将使用空白人设。",
            L"DeepSeek Fish", MB_OK | MB_ICONWARNING);
    }
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 必须是第一件事：Windows 在进程创建第一个窗口时就锁定 DPI 感知模式，
    // 之后再调 SetProcessDPIAware 不会报错但也不会生效。所有界面尺寸都依赖
    // dpi::scale()，所以这里必须先跑。
    dpi::init();

    CreateDirectoryW(apppath::of(L"archive").c_str(), nullptr);

    if (!loadApiKey()) return 1;
    loadPersonality();
    state::load();

    // 白窗口上要显示的正文。必须在建窗之前读好，否则第一次 WM_PAINT 会是空白。
    if (!wintext::load()) {
        MessageBoxW(nullptr,
            L"没读到 window_text.txt（文件不存在或内容为空），\n"
            L"白窗口将只显示一片纯白。",
            L"DeepSeek Fish", MB_OK | MB_ICONINFORMATION);
    }

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        MessageBoxW(nullptr, L"GDI+ 初始化失败，程序无法启动。",
            L"DeepSeek Fish", MB_OK | MB_ICONERROR);
        return 1;
    }

    pet::init(kBlinkFrames, kSitFrames, kEatFrames, kTokenFrames);

    HWND hwnd = ui::createPetWindow(hInstance);
    if (!hwnd) {
        GdiplusShutdown(gdiplusToken);
        MessageBoxW(nullptr, L"创建窗口失败。", L"DeepSeek Fish", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (pet::currentFrame().empty()) {
        MessageBoxW(nullptr, L"blink 帧加载失败，桌宠没有可显示的图像。",
            L"DeepSeek Fish", MB_OK | MB_ICONWARNING);
    }

    SetTimer(hwnd, layout::kTimerAnim, layout::kAnimIntervalMs, nullptr);
    SetTimer(hwnd, layout::kTimerSitWatch, layout::kSitWatchIntervalMs, nullptr);

    ShowWindow(hwnd, nCmdShow);
    pet::sitOnTaskbar(hwnd);
    pet::draw(hwnd);

    // 双击启动时另外弹一个普通带边框的空白窗口（纯白、无控件）。
    // 它在自己 WM_CLOSE 里退出消息循环，所以关掉它等于退出程序。
    if (!whitewin::createWindow(hInstance, SW_SHOWNORMAL)) {
        MessageBoxW(nullptr, L"创建空白窗口失败，桌宠仍会继续运行。",
            L"DeepSeek Fish", MB_OK | MB_ICONWARNING);
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    {
        std::lock_guard<std::mutex> lk(state::g_mutex);
        state::saveFavor();
        state::saveHistory();
    }

    GdiplusShutdown(gdiplusToken);
    return 0;
}
