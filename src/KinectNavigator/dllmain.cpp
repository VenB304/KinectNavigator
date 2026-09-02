#include "framework.h"
#include "globals.h"
#include "backend.h"
#include "recognizer.h"
#include "recorder.h"
#include "log.h"
#include "version.h"

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        g_hSelf = hInst;
        DisableThreadLibraryCalls(hInst);
        // No LoadLibrary / thread creation here -- that happens lazily via
        // Backend::Ensure() on the first export call, outside the loader lock.
        LogLine("==== KinectNavigator v%s attached  (pid=%lu) ====", KN_VER_STRING, GetCurrentProcessId());
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
        LogLine("==== KinectNavigator detaching (FreeLibrary) ====");
        Recognizer::Stop();
        Recorder::Close();
        Backend::Shutdown();
        break;
    }
    return TRUE;
}
