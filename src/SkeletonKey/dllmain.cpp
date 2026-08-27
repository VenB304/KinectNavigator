#include "framework.h"
#include "globals.h"
#include "backend.h"
#include "log.h"

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID /*reserved*/)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        g_hSelf = hInst;
        DisableThreadLibraryCalls(hInst);
        // No LoadLibrary here -- that happens lazily via Backend::Ensure() on
        // the first export call, outside the loader lock.
        LogLine("==== Skeleton Key attached  (pid=%lu) ====", GetCurrentProcessId());
        break;

    case DLL_PROCESS_DETACH:
        LogLine("==== Skeleton Key detaching ====");
        Backend::Shutdown();
        break;
    }
    return TRUE;
}
