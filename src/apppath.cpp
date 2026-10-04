#include "apppath.h"
#include <windows.h>

namespace apppath {

const std::wstring& exeDir() {
    static std::wstring dir;
    static bool inited = false;
    if (!inited) {
        wchar_t buf[MAX_PATH] = {};
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring full(buf, n);
        size_t pos = full.find_last_of(L"\\/");
        dir = (pos == std::wstring::npos) ? L"" : full.substr(0, pos + 1);
        inited = true;
    }
    return dir;
}

std::wstring of(const std::wstring& relative) {
    return exeDir() + relative;
}

} // namespace apppath
