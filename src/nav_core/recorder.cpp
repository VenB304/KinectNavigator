#include "framework.h"
#include "recorder.h"
#include "globals.h"
#include "log.h"

namespace
{
    volatile HANDLE g_file   = INVALID_HANDLE_VALUE;
    volatile LONG   g_frames = 0;

    // Opens the capture file if record.flag sits next to the module. `prefix`/`ext` name the
    // file, `magic` is the 8-byte header tag, `frameSize` the record size to stamp.
    void OpenCapture(const wchar_t* prefix, const wchar_t* ext, const char* magic, uint32_t frameSize)
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
        swprintf_s(path, MAX_PATH, L"%s%s-%04d%02d%02d-%02d%02d%02d.%s",
                   dir, prefix, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, ext);

        HANDLE h = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            LogLine("Recorder: CreateFile failed err=%lu", GetLastError());
            return;
        }

        Recorder::Header hdr = {};
        memcpy(hdr.magic, magic, 8);
        hdr.frameSize = frameSize;
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        hdr.startFileTime = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

        DWORD w = 0;
        WriteFile(h, &hdr, sizeof(hdr), &w, nullptr);

        g_file = h;
        LogLine("Recorder: capturing to %ls", path);
    }

    void WriteRecord(const void* rec, DWORD size)
    {
        HANDLE h = g_file;                   // single writer (the sensor's frame thread)
        if (h == INVALID_HANDLE_VALUE)
            return;
        DWORD w = 0;
        if (WriteFile(h, rec, size, &w, nullptr) && w == size)
            InterlockedIncrement(&g_frames);
    }
}

void Recorder::Init()
{
    OpenCapture(L"skcap", L"skcap", "SKCAP01\n", (uint32_t)sizeof(NUI_SKELETON_FRAME));
}

void Recorder::InitRich()
{
    OpenCapture(L"skcap2", L"skcap2", "SKCAP02\n", (uint32_t)sizeof(KN_RichFrame));
}

void Recorder::Write(const NUI_SKELETON_FRAME& f)
{
    WriteRecord(&f, (DWORD)sizeof(f));
}

void Recorder::WriteRich(const KN_RichFrame& f)
{
    WriteRecord(&f, (DWORD)sizeof(f));
}

void Recorder::Close()
{
    HANDLE h = (HANDLE)InterlockedExchangePointer((volatile PVOID*)&g_file, INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE)
        return;
    LogLine("Recorder: closed (%ld frames)", g_frames);
    CloseHandle(h);
}
