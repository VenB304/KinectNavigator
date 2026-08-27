#include "framework.h"
#include "log.h"
#include "globals.h"

void LogLine(const char* fmt, ...)
{
    wchar_t path[MAX_PATH];
    GetSelfDir(path, MAX_PATH);
    if (path[0] == L'\0')
        return;
    wcscat_s(path, MAX_PATH, L"SkeletonKey.log");

    HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    char line[1200];
    int n = sprintf_s(line, sizeof(line), "%02d:%02d:%02d.%03d  tid=%-5lu  ",
                      st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                      GetCurrentThreadId());
    if (n < 0) { CloseHandle(h); return; }

    va_list ap;
    va_start(ap, fmt);
    int m = vsprintf_s(line + n, sizeof(line) - n, fmt, ap);
    va_end(ap);
    if (m < 0) { CloseHandle(h); return; }
    n += m;

    int t = sprintf_s(line + n, sizeof(line) - n, "\r\n");
    if (t < 0) { CloseHandle(h); return; }
    n += t;

    DWORD written = 0;
    SetFilePointer(h, 0, nullptr, FILE_END);
    WriteFile(h, line, (DWORD)n, &written, nullptr);
    CloseHandle(h);
}
