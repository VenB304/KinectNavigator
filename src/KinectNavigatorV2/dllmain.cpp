#include "v2_framework.h"
#include "globals.h"
#include "v2reader.h"
#include "engine_v2.h"
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
        // GetDefaultKinectSensor, outside the loader lock.
        LogLine("==== KinectNavigator v%s Kinect v2 proxy attached  (pid=%lu) ====",
                KN_VER_STRING, GetCurrentProcessId());
        break;

    case DLL_PROCESS_DETACH:
        if (reserved != nullptr)
        {
            // Process terminating -- other threads are already gone (same
            // reasoning as the v1 proxy's dllmain.cpp). Let the OS reclaim.
            break;
        }
        LogLine("==== KinectNavigator Kinect v2 proxy detaching (FreeLibrary) ====");
        V2Reader::Stop();
        EngineV2::Stop();
        break;
    }
    return TRUE;
}
