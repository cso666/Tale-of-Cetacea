#include "http.h"
#include "textutils.h"
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace http {

namespace {
    const wchar_t* kHost = L"api.deepseek.com";
    const wchar_t* kPath = L"/chat/completions";
    const wchar_t* kAgent = L"DeepSeekFish/1.0";
}

Result postJson(const std::string& body, const std::string& key, int timeoutsMs) {
    Result r;

    HINTERNET hSession = WinHttpOpen(kAgent,
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        r.error = "WinHttpOpen 失败";
        return r;
    }
    WinHttpSetTimeouts(hSession, timeoutsMs, timeoutsMs, timeoutsMs, timeoutsMs);

    HINTERNET hConnect = WinHttpConnect(hSession, kHost, 443, 0);
    if (!hConnect) {
        r.error = "连接失败，错误码 " + std::to_string(GetLastError());
        WinHttpCloseHandle(hSession);
        return r;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", kPath,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        r.error = "创建请求失败，错误码 " + std::to_string(GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return r;
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Authorization: Bearer " + textutils::fromUtf8(key) + L"\r\n";

    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1L,
        (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
    if (!sent) {
        r.error = "发送请求失败，错误码 " + std::to_string(GetLastError());
    } else if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        r.error = "接收响应失败，错误码 " + std::to_string(GetLastError());
    } else {
        DWORD status = 0, slen = sizeof(status);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &slen, WINHTTP_NO_HEADER_INDEX);
        r.status = status;

        std::string data;
        DWORD avail = 0;
        do {
            avail = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &avail)) break;
            if (avail == 0) break;
            size_t old = data.size();
            data.resize(old + avail);
            DWORD read = 0;
            if (!WinHttpReadData(hRequest, &data[old], avail, &read)) {
                data.resize(old);
                break;
            }
            data.resize(old + read);
        } while (avail > 0);

        r.body = data;
        r.ok = true;
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return r;
}

} // namespace http
