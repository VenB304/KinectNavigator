#include "framework.h"
#include "recorder.h"
#include "globals.h"
#include "log.h"

namespace
{
    volatile HANDLE g_file   = INVALID_HANDLE_VALUE;
    volatile LONG   g_frames = 0;
}

void Recorder::Init()
{
    wchar_t dir[MAX_PATH];
    GetSelfDir(dir, MAX_PATH);
    if (dir[0] == L'\0')
        return;

    wchar_t flag[MAX_PATH];
    wcscpy_s(flag, MAX_PATH, dir);
    wcscat_s(flag, MAX_PATH, L"record.flag");
    if (GetFileAttributesW(flag) == INVALID_FILE_ATTRIBUTES)
    {
        LogLine("Recorder: no record.flag -- not capturing");
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t path[MAX_PATH];
    swprintf_s(path, MAX_PATH, L"%sskcap-%04d%02d%02d-%02d%02d%02d.skcap",
               dir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        LogLine("Recorder: CreateFile failed err=%lu", GetLastError());
        return;
    }

    Header hdr = {};
    memcpy(hdr.magic, "SKCAP01\n", 8);
    hdr.frameSize = (uint32_t)sizeof(NUI_SKELETON_FRAME);
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    hdr.startFileTime = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

    DWORD w = 0;
    WriteFile(h, &hdr, sizeof(hdr), &w, nullptr);

    g_file = h;
    LogLine("Recorder: capturing to %ls", path);
}

void Recorder::Write(const NUI_SKELETON_FRAME& f)
{
    HANDLE h = g_file;                       // single writer (game's Kinect thread)
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD w = 0;
    if (WriteFile(h, &f, sizeof(f), &w, nullptr) && w == sizeof(f))
        InterlockedIncrement(&g_frames);
}

void Recorder::Close()
{
    HANDLE h = (HANDLE)InterlockedExchangePointer((volatile PVOID*)&g_file, INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE)
        return;
    LogLine("Recorder: closed (%ld frames)", g_frames);
    CloseHandle(h);
}
