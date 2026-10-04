#pragma once
#include <string>

// 所有资源路径都基于 exe 所在目录解析。
// FIX: 原来一律用相对路径，隐含假设"工作目录 == exe 目录"。从快捷方式、任务栏
//      固定项或别的目录启动时，图片全部加载失败，桌宠变成一块空白。
namespace apppath {
    // exe 所在目录，带结尾反斜杠
    const std::wstring& exeDir();

    // 拼出 exe 目录下的绝对路径
    std::wstring of(const std::wstring& relative);
}
