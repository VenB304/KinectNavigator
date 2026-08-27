#include "framework.h"
#include "globals.h"
#include "backend.h"
#include "recognizer.h"
#include "recorder.h"
#include "log.h"

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        g_hSelf = hInst;
        DisableThreadLibraryCalls(hInst);
        // No LoadLibrary / thread creation here -- that happens lazily via
        // Backend::Ensure() on the first export call, outside the loader lock.
        LogLine("==== Skeleton Key attached  (pid=%lu) ====", GetCurrentProcessId());
        break;

    case DLL_PROCESS_DETACH:
        if (reserved != nullptr)
        {
            // Process is terminating (M1 showed Legacy.exe does exactly this):
            // the loader is tearing everything down, other threads are already
            // gone. Touching them or the loader lock here risks a hang -- let
            // the OS reclaim everything.
            break;
        }
        LogLine("==== Skeleton Key detaching (FreeLibrary) ====");
        Recognizer::Stop();
        Recorder::Close();
        Backend::Shutdown();
        break;
    }
    return TRUE;
}
