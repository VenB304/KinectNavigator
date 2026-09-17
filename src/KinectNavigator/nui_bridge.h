#pragma once
#include "framework.h"
#include "nui_types.h"

// Binds the GENUINE Microsoft Kinect for Windows v1 runtime by absolute path (NOT our own
// Kinect10.dll shim, which only forwards 8 ordinals) and pumps skeleton frames -- for any
// standalone tool that wants live skeleton data with no game / DLL injection involved.
// Extracted from KinectNavigatorLab (which keeps using it) so KinectNavigatorTutorial -- a
// SHIPPED, public exe, unlike Lab -- can reuse the same proven sensor bring-up instead of
// reimplementing it.
//
// Every call returns a Status instead of logging-and-exiting: KinectNavigatorLab is a dev CLI
// tool where that was fine, but KinectNavigatorTutorial is launched by a non-technical player
// who needs a friendly "plug in your Kinect" screen, not a console flash. Bind()/Init() are
// each safely retriable -- a Tutorial run typically calls Bind() once, then retries Init() on
// a timer until a sensor shows up.
namespace NuiBridge
{
    enum class Status
    {
        Ok,
        NoRuntime,               // couldn't find/load the genuine Kinect10.dll at all
        BadExports,               // loaded it, but it's missing an expected flat NUI export
        InitFailed,               // NuiInitialize failed (no sensor plugged in / service down)
        TrackingEnableFailed,     // NuiSkeletonTrackingEnable failed
    };

    // Locates + loads the genuine Kinect10.dll (a renamed backend next to the exe first, then
    // the system-installed runtime) and resolves the flat NUI entry points. Call once at
    // startup; `outWhich`, if non-null, receives the resolved path (for logging).
    Status Bind(wchar_t* outWhich = nullptr, size_t outWhichCch = 0);

    // NuiInitialize + NuiSkeletonTrackingEnable. Safe to call again to retry (sensor plugged
    // in after the fact, or replugged) without a fresh Bind() -- each call is a clean attempt.
    Status Init();

    // Waits up to timeoutMs for a frame, then drains everything currently queued, calling
    // onFrame once per frame. Returns the number of frames drained (0 is normal -- Kinect v1's
    // NuiSkeletonGetNextFrame doesn't reliably distinguish "nothing new yet" from "sensor gone"
    // through the flat API, so a real disconnect isn't a single failed call -- it's zero frames
    // for longer than a body should ever go untracked. Callers track that timeout themselves).
    int Pump(DWORD timeoutMs, void (*onFrame)(const NUI_SKELETON_FRAME&, void* ctx), void* ctx);

    void Shutdown();   // NuiShutdown + close the frame event; the DLL stays bound for a retry
    void Unbind();     // FreeLibrary -- call once at process exit
}
