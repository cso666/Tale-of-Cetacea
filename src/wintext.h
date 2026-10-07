#pragma once
#include <windows.h>
#include <string>

// 第一个界面：启动时从 exe 目录下的 window_text.txt 读入正文，缓存在内存里。
// 内容超过可视区时自动出现一条自绘的竖向滑块。
//
// 关于坐标：所有函数都接受"客户区坐标"下的内容矩形。
// 左侧页签栏占掉的那一条由调用方排除在外，本模块不关心栏本身。
namespace wintext {

    // 要读取的文件名（相对 exe 目录）。改名只需改这一处
    extern const wchar_t* const kFileName;

    // 从文件载入全文。文件不存在时内容为空（不算错误）。
    // 返回是否成功读到非空内容。
    bool load();

    // 当前正文
    const std::wstring& get();

    // 告知内容区矩形（客户区坐标）。每次绘制前调用。
    // 尺寸变化后会把滚动偏移夹回合法范围。
    void setViewport(const RECT& contentRect);

    // 把正文画进 rect（客户区坐标）内：按宽度自动换行、按滚动偏移裁剪，
    // 需要时并在该矩形右侧画出滑块。
    // 由调用方负责背景擦除和 EndPaint，这样可以配合双缓冲。
    void draw(HDC hdc, const RECT& rect);

    // 还能往下滚多少像素；返回 0 表示内容全部可见、不需要滑块
    int maxScroll();

    // 滚动。delta 为像素增量；越界会自动夹紧。返回是否真的产生了变化
    bool scrollBy(int delta);

    // 把滚动位置设为"滑块顶端位于 trackTop"时对应的位置
    void scrollFromThumbTop(int trackTop);

    // -1 = 点在滑块上方（需要上翻一页），1 = 下方，0 = 没点在轨道上
    int hitTrack(int y);

    // 滑块条与滑块的当前位置（客户区坐标）。visible 为 false 时内容能一屏放下
    struct ScrollBar {
        bool visible = false;
        RECT track = {};
        RECT thumb = {};
    };
    ScrollBar scrollBar();

} // namespace wintext
