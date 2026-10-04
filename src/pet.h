#pragma once
#include <windows.h>
#include <string>

// 桌宠本体：帧资源、动画推进、坐姿状态、绘制、任务栏吸附。
//
// FIX: 原来是四个全局 vector 加一堆散落的索引，任何一处越界访问都是 UB
//      （帧数组为空时 frames[0] 直接崩）。现在索引只在 pet.cpp 内改，并且
//      每次访问前都有空判断。
namespace pet {

    // 载入 blink / sit / eatfile / token 的帧文件名（只填路径，不读图片）
    void init(int blinkCount, int sitCount, int eatCount, int tokenCount);

    // 当前应该显示的那张图的路径；无可用帧时返回空串
    const std::wstring& currentFrame();

    // 叠加式绘制到 hwnd（透明分层窗口）
    void draw(HWND hwnd);

    // 每 kAnimIntervalMs 调一次，推进当前动画
    void tick(HWND hwnd);

    // 每 kSitWatchIntervalMs 调一次，检查是否该坐到任务栏上
    void watchTaskbar(HWND hwnd);

    // 直接坐到任务栏右下角
    void sitOnTaskbar(HWND hwnd);

    // 用户开始拖拽：立刻离开坐姿，并暂停看门狗
    void beginDrag(HWND hwnd);
    void endDrag();

    // 播放"被投喂"动画。isToken 决定用哪一套帧。文件删不掉时返回 false
    bool startEating(const std::wstring& filePath);

} // namespace pet
