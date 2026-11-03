#include "v2_framework.h"
#include "backend_v2.h"
#include "globals.h"
#include "log.h"

namespace BackendV2
{
    Fns fn = {};

    static HMODULE   s_lib  = nullptr;
    static INIT_ONCE s_once = INIT_ONCE_STATIC_INIT;

    static BOOL CALLBACK InitCb(PINIT_ONCE, PVOID, PVOID*)
    {
        wchar_t path[MAX_PATH];
        GetSelfDir(path, MAX_PATH);
        wcscat_s(path, MAX_PATH, L"Kinect20_backend.dll");

        LogLine("BackendV2: loading %ls", path);
        s_lib = LoadLibraryW(path);
        if (!s_lib)
        {
            LogLine("BackendV2: LoadLibrary FAILED (err=%lu) -- GetDefaultKinectSensor "
                     "will return E_FAIL", GetLastError());
            return TRUE;
        }
        LogLine("BackendV2: loaded at %p", reinterpret_cast<void*>(s_lib));

        fn.GetDefaultKinectSensor = reinterpret_cast<HRESULT (WINAPI*)(IKinectSensor**)>(
            GetProcAddress(s_lib, "GetDefaultKinectSensor"));
        LogLine("  bind GetDefaultKinectSensor -> %p",
                reinterpret_cast<void*>(fn.GetDefaultKinectSensor));
        return TRUE;
    }

    void Ensure()
    {
        InitOnceExecuteOnce(&s_once, InitCb, nullptr, nullptr);
    }
}
