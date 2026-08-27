#include "framework.h"
#include "globals.h"

HMODULE g_hSelf = nullptr;

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
