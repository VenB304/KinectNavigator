#include "framework.h"
#include "backend.h"
#include "globals.h"
#include "log.h"
#include "recognizer.h"
#include "recorder.h"
#include "overlay.h"
#include "gameprobe.h"
#include "config.h"

namespace Backend
{
    Fns fn = {};

    static HMODULE   s_lib  = nullptr;
    static INIT_ONCE s_once = INIT_ONCE_STATIC_INIT;

    template <class T>
    static void Bind(T& p, const char* name)
    {
        p = reinterpret_cast<T>(GetProcAddress(s_lib, name));
        LogLine("  bind %-46s -> %p", name, reinterpret_cast<void*>(p));
    }

    static BOOL CALLBACK InitCb(PINIT_ONCE, PVOID, PVOID*)
    {
        wchar_t path[MAX_PATH];
        GetSelfDir(path, MAX_PATH);
        wcscat_s(path, MAX_PATH, L"Kinect10_backend.dll");

        LogLine("Backend: loading %ls", path);
        s_lib = LoadLibraryW(path);
        if (!s_lib)
        {
            LogLine("Backend: LoadLibrary FAILED (err=%lu) -- forwards will return E_FAIL",
                    GetLastError());
            MessageBoxW(nullptr,
                        L"KinectNavigator could not load Kinect10_backend.dll.\n\n"
                        L"Install it properly: run KinectNavigator (or "
                        L"KinectNavigator-CLI-install.bat) so the game's genuine "
                        L"Kinect10.dll is renamed to Kinect10_backend.dll.",
                        L"KinectNavigator", MB_ICONERROR | MB_OK);
            return TRUE;
        }
        LogLine("Backend: loaded at %p", reinterpret_cast<void*>(s_lib));

        Bind(fn.NuiGetSensorCount,      "NuiGetSensorCount");
        Bind(fn.NuiCreateSensorByIndex, "NuiCreateSensorByIndex");
        Bind(fn.NuiCreateSensorById,    "NuiCreateSensorById");
        Bind(fn.NuiImageGetColorPixelCoordinatesFromDepthPixel,
                                        "NuiImageGetColorPixelCoordinatesFromDepthPixel");
        Bind(fn.NuiShutdown,            "NuiShutdown");
        Bind(fn.NuiSkeletonGetNextFrame,"NuiSkeletonGetNextFrame");
        Bind(fn.NuiSetDeviceStatusCallback,
                                        "NuiSetDeviceStatusCallback");
        Bind(fn.NuiSkeletonSetTrackedSkeletons,
                                        "NuiSkeletonSetTrackedSkeletons");

        // M2: start consuming frames now that the tap can forward them.
        Cfg::Load();
        GameProbe::Install();
        Recognizer::Start();
        Recorder::Init();
        Overlay::Start();
        return TRUE;
    }

    void Ensure()
    {
        InitOnceExecuteOnce(&s_once, InitCb, nullptr, nullptr);
    }

    void Shutdown()
    {
        GameProbe::Remove();
        Overlay::Stop();
        if (s_lib)
        {
            LogLine("Backend: FreeLibrary");
            FreeLibrary(s_lib);
            s_lib = nullptr;
        }
        fn = Fns{};
    }
}
