#include "framework.h"
#include "globals.h"

HMODULE g_hSelf = nullptr;
volatile LONGLONG g_lastRenderTick = 0;
volatile long     g_colorRes    = -1;
volatile LONGLONG g_colorCalls  = 0;
volatile LONGLONG g_menuPollTick = 0;
volatile long     g_menuPollVk   = 0;
volatile LONGLONG g_fileOpens    = 0;
volatile LONGLONG g_lastOpenTick = 0;
wchar_t           g_lastFile[260] = L"";
volatile DWORD    g_trkId0 = 0, g_trkId1 = 0;
volatile long     g_gameWndActive = 1;

void GetSelfDir(wchar_t* out, size_t cch)
{
    if (cch) out[0] = L'\0';

    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(g_hSelf, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return;

    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash)
        return;
    slash[1] = L'\0';

    wcscpy_s(out, cch, path);
}
