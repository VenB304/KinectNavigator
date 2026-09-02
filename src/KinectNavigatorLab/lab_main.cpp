// KinectNavigatorLab -- run the live recognizer straight off the Kinect, with no game.
//
// The game crashes every 30-120 s, which makes live gesture tuning miserable. But
// the game isn't needed to tune gestures -- only real skeleton data + the real
// recognizer + visibility. This tool opens the sensor directly and pumps frames
// into the SAME FrameBuffer + Recognizer + Gestures that ship in the DLL, so what
// it prints is what the shim would do. Iterate here; go to Legacy.exe only for a
// final "the keystroke still scrolls the carousel and the feel is right" check.
//
//   KinectNavigatorLab [--live] [--record]
//     (default)   dry-run: the recognizer logs gestures, no keys are injected
//     --live      inject keystrokes, but only while THIS console is foreground
//                 (Output's requireForeground gate keys off our own PID)
//     --record    also write skcap-<ts>.skcap next to the exe (replay corpus)
//
//   kinectnav.ini next to the exe overrides thresholds (same keys as the DLL);
//   set trace=1 there for the ~15 Hz hand-signal log. No rebuild for tuning.
//
// The flat Kinect 1.x NUI API is resolved from the genuine Kinect10.dll at
// runtime (same approach as src/KinectNavigator/backend.cpp) so this tool has no
// link-time dependency on the Kinect SDK. A Win32 build picks up the real
// Kinect10.dll from SysWOW64 when the Kinect for Windows Runtime/SDK 1.8 is
// installed.

#include "framework.h"
#include "nui_types.h"
#include "globals.h"
#include "framebuffer.h"
#include "recognizer.h"
#include "recorder.h"
#include "overlay.h"
#include "config.h"
#include "output.h"
#include "log.h"

namespace
{
    // --- flat NUI entry points we need -------------------------------------
    typedef HRESULT (__stdcall *PFN_NuiInitialize)(DWORD dwFlags);
    typedef HRESULT (__stdcall *PFN_NuiSkeletonTrackingEnable)(HANDLE hNextFrameEvent, DWORD dwFlags);
    typedef HRESULT (__stdcall *PFN_NuiSkeletonGetNextFrame)(DWORD dwMillisecondsToWait, NUI_SKELETON_FRAME* pFrame);
    typedef void    (__stdcall *PFN_NuiShutdown)(void);

    const DWORD kUsesSkeleton = 0x00000008;   // NUI_INITIALIZE_FLAG_USES_SKELETON

    PFN_NuiInitialize             pNuiInitialize             = nullptr;
    PFN_NuiSkeletonTrackingEnable pNuiSkeletonTrackingEnable = nullptr;
    PFN_NuiSkeletonGetNextFrame   pNuiSkeletonGetNextFrame   = nullptr;
    PFN_NuiShutdown               pNuiShutdown               = nullptr;

    volatile LONG g_stop = 0;

    BOOL WINAPI CtrlHandler(DWORD type)
    {
        if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT)
        {
            InterlockedExchange(&g_stop, 1);
            return TRUE;
        }
        return FALSE;
    }

    // Load the GENUINE Kinect runtime, by absolute path. A bare
    // LoadLibrary(L"Kinect10.dll") is wrong here: this exe ships in dist\ next to
    // our proxy Kinect10.dll (8 ordinals, no flat API), which would shadow it.
    // Order: the renamed backend next to the exe (install.bat leaves one in a
    // game folder), then the system-installed runtime (SysWOW64 for this Win32
    // build when the Kinect Runtime/SDK 1.8 is present).
    HMODULE LoadGenuineKinect(wchar_t* whichOut, size_t whichCch)
    {
        wchar_t path[MAX_PATH];

        GetSelfDir(path, MAX_PATH);
        if (path[0] != L'\0')
        {
            wcscat_s(path, MAX_PATH, L"Kinect10_backend.dll");
            HMODULE h = LoadLibraryW(path);
            if (h) { wcscpy_s(whichOut, whichCch, path); return h; }
        }

        UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (n > 0 && n < MAX_PATH)
        {
            wcscat_s(path, MAX_PATH, L"\\Kinect10.dll");
            HMODULE h = LoadLibraryW(path);
            if (h) { wcscpy_s(whichOut, whichCch, path); return h; }
        }

        whichOut[0] = L'\0';
        return nullptr;
    }

    bool BindKinect(HMODULE& outLib)
    {
        wchar_t which[MAX_PATH];
        HMODULE lib = LoadGenuineKinect(which, MAX_PATH);
        if (!lib)
        {
            wprintf(L"[lab] cannot find the genuine Kinect10.dll (err=%lu)\n"
                    L"      install the Kinect for Windows Runtime or SDK 1.8, or run\n"
                    L"      dist\\install.bat in a game folder so Kinect10_backend.dll exists.\n",
                    GetLastError());
            return false;
        }
        wprintf(L"[lab] Kinect runtime: %ls\n", which);
        pNuiInitialize             = (PFN_NuiInitialize)             GetProcAddress(lib, "NuiInitialize");
        pNuiSkeletonTrackingEnable = (PFN_NuiSkeletonTrackingEnable) GetProcAddress(lib, "NuiSkeletonTrackingEnable");
        pNuiSkeletonGetNextFrame   = (PFN_NuiSkeletonGetNextFrame)   GetProcAddress(lib, "NuiSkeletonGetNextFrame");
        pNuiShutdown               = (PFN_NuiShutdown)               GetProcAddress(lib, "NuiShutdown");

        if (!pNuiInitialize || !pNuiSkeletonTrackingEnable || !pNuiSkeletonGetNextFrame || !pNuiShutdown)
        {
            wprintf(L"[lab] Kinect10.dll is missing an expected flat NUI export\n");
            FreeLibrary(lib);
            return false;
        }
        outLib = lib;
        return true;
    }

    // Drop a record.flag next to the exe so Recorder::Init() opens a capture,
    // then remove the flag (the open handle keeps writing). Returns true if a
    // capture was actually opened.
    bool StartRecording()
    {
        wchar_t dir[MAX_PATH];
        GetSelfDir(dir, MAX_PATH);
        if (dir[0] == L'\0') return false;

        wchar_t flag[MAX_PATH];
        swprintf_s(flag, MAX_PATH, L"%srecord.flag", dir);

        HANDLE h = CreateFileW(flag, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);

        Recorder::Init();
        DeleteFileW(flag);
        return true;
    }
}

int wmain(int argc, wchar_t** argv)
{
    bool live = false, record = false;
    for (int i = 1; i < argc; ++i)
    {
        if      (!wcscmp(argv[i], L"--live"))    live = true;
        else if (!wcscmp(argv[i], L"--record")) record = true;
        else if (!wcscmp(argv[i], L"--help") || !wcscmp(argv[i], L"-h") || !wcscmp(argv[i], L"/?"))
        {
            wprintf(L"usage: KinectNavigatorLab [--live] [--record]\n"
                    L"  (default)  dry-run: recognizer logs gestures, no keys injected\n"
                    L"  --live     inject keystrokes while THIS console is foreground\n"
                    L"  --record   also write skcap-<ts>.skcap next to the exe\n"
                    L"  kinectnav.ini next to the exe overrides thresholds; trace=1 for the hand-signal log\n");
            return 0;
        }
        else
        {
            wprintf(L"[lab] unknown arg: %ls (try --help)\n", argv[i]);
            return 2;
        }
    }

    Log::SetEcho(true);
    Output::SetDryRun(!live);
    Cfg::Load();                       // kinectnav.ini next to the exe, if present

    HMODULE kinect = nullptr;
    if (!BindKinect(kinect)) return 1;

    HRESULT hr = pNuiInitialize(kUsesSkeleton);
    if (FAILED(hr))
    {
        wprintf(L"[lab] NuiInitialize failed hr=0x%08lX -- sensor plugged in? Kinect service running?\n", hr);
        FreeLibrary(kinect);
        return 1;
    }

    HANDLE hFrame = CreateEventW(nullptr, TRUE, FALSE, nullptr);   // manual-reset; NuiSkeletonGetNextFrame clears it
    hr = pNuiSkeletonTrackingEnable(hFrame, 0);
    if (FAILED(hr))
    {
        wprintf(L"[lab] NuiSkeletonTrackingEnable failed hr=0x%08lX\n", hr);
        pNuiShutdown();
        CloseHandle(hFrame);
        FreeLibrary(kinect);
        return 1;
    }

    bool recording = record && StartRecording();
    if (record && !recording)
        wprintf(L"[lab] --record: could not open a capture file (continuing without)\n");

    SetConsoleCtrlHandler(CtrlHandler, TRUE);
    Recognizer::Start();
    Overlay::Start(true);   // dev tool -- always show the HUD

    wprintf(L"[lab] running %ls%ls -- Ctrl+C to stop\n",
            live ? L"LIVE (keys -> foreground window)" : L"(dry-run)",
            recording ? L" +record" : L"");

    NUI_SKELETON_FRAME fr;
    unsigned long long pumped = 0;
    LONGLONG lastStatus = 0;

    while (!g_stop)
    {
        WaitForSingleObject(hFrame, 200);      // 200 ms cap so Ctrl+C is responsive

        int got = 0;
        while (pNuiSkeletonGetNextFrame(0, &fr) == S_OK)
        {
            FrameBuffer::Publish(fr);
            if (recording) Recorder::Write(fr);
            ++pumped;
            ++got;

            const LONGLONG now = fr.liTimeStamp.QuadPart;   // sensor clock, ms
            if (now - lastStatus >= 2000)
            {
                lastStatus = now;
                int bodies = 0;
                for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
                    if (fr.SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED) ++bodies;
                wprintf(L"[lab] frame=%lu ts=%lldms bodies=%d  pumped=%llu consumed=%llu\n",
                        fr.dwFrameNumber, now, bodies, pumped, FrameBuffer::ConsumedCount());
            }
        }
        if (got == 0) Sleep(5);               // nothing new -- don't hot-spin
    }

    wprintf(L"[lab] stopping\n");
    Overlay::Stop();
    Recognizer::Stop();
    if (recording) Recorder::Close();
    pNuiShutdown();
    CloseHandle(hFrame);
    FreeLibrary(kinect);

    wprintf(L"[lab] done: %llu frames pumped, %llu consumed\n",
            pumped, FrameBuffer::ConsumedCount());
    return 0;
}
