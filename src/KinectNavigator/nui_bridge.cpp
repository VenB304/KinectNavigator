#include "framework.h"
#include "nui_bridge.h"
#include "globals.h"

namespace NuiBridge
{
    namespace
    {
        typedef HRESULT (__stdcall *PFN_NuiInitialize)(DWORD dwFlags);
        typedef HRESULT (__stdcall *PFN_NuiSkeletonTrackingEnable)(HANDLE hNextFrameEvent, DWORD dwFlags);
        typedef HRESULT (__stdcall *PFN_NuiSkeletonGetNextFrame)(DWORD dwMillisecondsToWait, NUI_SKELETON_FRAME* pFrame);
        typedef void    (__stdcall *PFN_NuiShutdown)(void);

        const DWORD kUsesSkeleton = 0x00000008;   // NUI_INITIALIZE_FLAG_USES_SKELETON

        HMODULE                       g_lib   = nullptr;
        PFN_NuiInitialize             g_pInit = nullptr;
        PFN_NuiSkeletonTrackingEnable g_pTrackEnable = nullptr;
        PFN_NuiSkeletonGetNextFrame   g_pGetFrame    = nullptr;
        PFN_NuiShutdown               g_pShutdown    = nullptr;

        HANDLE g_frameEvent = nullptr;
        bool   g_trackingOn = false;

        // Load the GENUINE Kinect runtime, by absolute path. A bare LoadLibrary(L"Kinect10.dll")
        // is wrong here: our own proxy Kinect10.dll (8 ordinals, no flat API) would shadow it
        // if it's anywhere on the search path. Order: a renamed backend next to the exe (the
        // installer leaves one in a game folder as Kinect10_backend.dll), then the
        // system-installed runtime (SysWOW64 for this Win32 build, when the Kinect for Windows
        // Runtime/SDK 1.8 is present).
        HMODULE LoadGenuineKinect(wchar_t* whichOut, size_t whichCch)
        {
            wchar_t path[MAX_PATH];

            GetSelfDir(path, MAX_PATH);
            if (path[0] != L'\0')
            {
                wcscat_s(path, MAX_PATH, L"Kinect10_backend.dll");
                HMODULE h = LoadLibraryW(path);
                if (h) { if (whichOut) wcscpy_s(whichOut, whichCch, path); return h; }
            }

            UINT n = GetSystemDirectoryW(path, MAX_PATH);
            if (n > 0 && n < MAX_PATH)
            {
                wcscat_s(path, MAX_PATH, L"\\Kinect10.dll");
                HMODULE h = LoadLibraryW(path);
                if (h) { if (whichOut) wcscpy_s(whichOut, whichCch, path); return h; }
            }

            if (whichOut && whichCch) whichOut[0] = L'\0';
            return nullptr;
        }
    }

    Status Bind(wchar_t* outWhich, size_t outWhichCch)
    {
        if (g_lib) return Status::Ok;   // already bound

        g_lib = LoadGenuineKinect(outWhich, outWhichCch);
        if (!g_lib) return Status::NoRuntime;

        g_pInit        = (PFN_NuiInitialize)             GetProcAddress(g_lib, "NuiInitialize");
        g_pTrackEnable = (PFN_NuiSkeletonTrackingEnable) GetProcAddress(g_lib, "NuiSkeletonTrackingEnable");
        g_pGetFrame    = (PFN_NuiSkeletonGetNextFrame)   GetProcAddress(g_lib, "NuiSkeletonGetNextFrame");
        g_pShutdown    = (PFN_NuiShutdown)               GetProcAddress(g_lib, "NuiShutdown");

        if (!g_pInit || !g_pTrackEnable || !g_pGetFrame || !g_pShutdown)
        {
            FreeLibrary(g_lib); g_lib = nullptr;
            return Status::BadExports;
        }
        return Status::Ok;
    }

    Status Init()
    {
        if (!g_lib) return Status::NoRuntime;

        if (g_trackingOn) { Shutdown(); }   // clean re-init on a retry

        HRESULT hr = g_pInit(kUsesSkeleton);
        if (FAILED(hr)) return Status::InitFailed;

        if (!g_frameEvent) g_frameEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        else ResetEvent(g_frameEvent);

        hr = g_pTrackEnable(g_frameEvent, 0);
        if (FAILED(hr))
        {
            g_pShutdown();
            return Status::TrackingEnableFailed;
        }

        g_trackingOn = true;
        return Status::Ok;
    }

    int Pump(DWORD timeoutMs, void (*onFrame)(const NUI_SKELETON_FRAME&, void*), void* ctx)
    {
        if (!g_trackingOn || !g_frameEvent) return 0;

        WaitForSingleObject(g_frameEvent, timeoutMs);

        int got = 0;
        NUI_SKELETON_FRAME fr;
        while (g_pGetFrame(0, &fr) == S_OK)
        {
            if (onFrame) onFrame(fr, ctx);
            ++got;
        }
        return got;
    }

    void Shutdown()
    {
        if (g_trackingOn && g_pShutdown) g_pShutdown();
        g_trackingOn = false;
    }

    void Unbind()
    {
        Shutdown();
        if (g_frameEvent) { CloseHandle(g_frameEvent); g_frameEvent = nullptr; }
        if (g_lib) { FreeLibrary(g_lib); g_lib = nullptr; }
        g_pInit = nullptr; g_pTrackEnable = nullptr; g_pGetFrame = nullptr; g_pShutdown = nullptr;
    }
}
