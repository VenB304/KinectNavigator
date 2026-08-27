// Skeleton Key -- Milestone 1 (passthrough proxy)
//
// The eight functions Legacy.exe imports from Kinect10.dll by ordinal.
// Each one lazily loads the renamed genuine runtime, forwards the call
// unchanged, and appends a line to SkeletonKey.log. Nothing is intercepted
// or modified yet -- M1 exists only to prove the DLL-replacement hook and
// to capture the real call sequence.
//
// Ordinals are pinned in exports.def and must stay 5/6/9/16/17/21/22/26.

#include "framework.h"
#include "backend.h"
#include "framebuffer.h"
#include "recognizer.h"
#include "recorder.h"
#include "log.h"

namespace
{
    // Cheap per-function call counter for the hot paths, so a 30 Hz stream
    // does not flood the log. Returns true when this call should be logged.
    bool Sample(volatile long& counter, long everyN)
    {
        long n = InterlockedIncrement(&counter);
        return n <= 3 || (n % everyN) == 0;
    }
}

extern "C" {

HRESULT __stdcall NuiGetSensorCount(int* pCount)
{
    Backend::Ensure();
    HRESULT hr = Backend::fn.NuiGetSensorCount
               ? Backend::fn.NuiGetSensorCount(pCount)
               : E_FAIL;
    LogLine("[call] NuiGetSensorCount -> hr=0x%08lX count=%d",
            hr, pCount ? *pCount : -1);
    return hr;
}

HRESULT __stdcall NuiCreateSensorByIndex(int index, void** ppSensor)
{
    Backend::Ensure();
    HRESULT hr = Backend::fn.NuiCreateSensorByIndex
               ? Backend::fn.NuiCreateSensorByIndex(index, ppSensor)
               : E_FAIL;
    LogLine("[call] NuiCreateSensorByIndex(%d) -> hr=0x%08lX INuiSensor*=%p",
            index, hr, ppSensor ? *ppSensor : nullptr);
    return hr;
}

HRESULT __stdcall NuiCreateSensorById(const wchar_t* strInstanceId, void** ppSensor)
{
    Backend::Ensure();
    HRESULT hr = Backend::fn.NuiCreateSensorById
               ? Backend::fn.NuiCreateSensorById(strInstanceId, ppSensor)
               : E_FAIL;
    LogLine("[call] NuiCreateSensorById(%ls) -> hr=0x%08lX INuiSensor*=%p",
            strInstanceId ? strInstanceId : L"(null)",
            hr, ppSensor ? *ppSensor : nullptr);
    return hr;
}

HRESULT __stdcall NuiImageGetColorPixelCoordinatesFromDepthPixel(
        int eColorResolution, const void* pcViewArea,
        long lDepthX, long lDepthY, unsigned short usDepthValue,
        long* plColorX, long* plColorY)
{
    Backend::Ensure();
    HRESULT hr = Backend::fn.NuiImageGetColorPixelCoordinatesFromDepthPixel
               ? Backend::fn.NuiImageGetColorPixelCoordinatesFromDepthPixel(
                     eColorResolution, pcViewArea, lDepthX, lDepthY,
                     usDepthValue, plColorX, plColorY)
               : E_FAIL;

    static volatile long s_n = 0;
    if (Sample(s_n, 5000))
        LogLine("[call] NuiImageGetColorPixelCoordinatesFromDepthPixel #%ld -> hr=0x%08lX",
                s_n, hr);
    return hr;
}

void __stdcall NuiShutdown(void)
{
    LogLine("[call] NuiShutdown");
    Recognizer::Stop();
    Recorder::Close();
    if (Backend::fn.NuiShutdown)
        Backend::fn.NuiShutdown();
    Backend::Shutdown();
}

HRESULT __stdcall NuiSkeletonGetNextFrame(DWORD dwMillisecondsToWait, NUI_SKELETON_FRAME* pSkeletonFrame)
{
    Backend::Ensure();
    HRESULT hr = Backend::fn.NuiSkeletonGetNextFrame
               ? Backend::fn.NuiSkeletonGetNextFrame(dwMillisecondsToWait, pSkeletonFrame)
               : E_FAIL;

    // M2: hand the frame to the recognizer (de-dup happens inside Publish).
    // Harness: also append it to the capture file when record.flag is set.
    if (SUCCEEDED(hr) && pSkeletonFrame)
    {
        FrameBuffer::Publish(*pSkeletonFrame);
        Recorder::Write(*pSkeletonFrame);
    }

    static volatile long s_n = 0;
    if (Sample(s_n, 300))
        LogLine("[call] NuiSkeletonGetNextFrame #%ld wait=%lu -> hr=0x%08lX frame=%p accepted=%llu",
                s_n, dwMillisecondsToWait, hr, (void*)pSkeletonFrame, FrameBuffer::Count());
    return hr;
}

HRESULT __stdcall NuiSetDeviceStatusCallback(void* callback, void* pUserData)
{
    Backend::Ensure();
    HRESULT hr = Backend::fn.NuiSetDeviceStatusCallback
               ? Backend::fn.NuiSetDeviceStatusCallback(callback, pUserData)
               : E_FAIL;
    LogLine("[call] NuiSetDeviceStatusCallback(cb=%p) -> hr=0x%08lX", callback, hr);
    return hr;
}

HRESULT __stdcall NuiSkeletonSetTrackedSkeletons(DWORD* pTrackingIds)
{
    Backend::Ensure();
    HRESULT hr = Backend::fn.NuiSkeletonSetTrackedSkeletons
               ? Backend::fn.NuiSkeletonSetTrackedSkeletons(pTrackingIds)
               : E_FAIL;

    // Called every frame by Legacy.exe -- sample it like the other hot paths.
    static volatile long s_n = 0;
    if (Sample(s_n, 600))
        LogLine("[call] NuiSkeletonSetTrackedSkeletons #%ld (ids=[%lu,%lu]) -> hr=0x%08lX",
                s_n,
                pTrackingIds ? pTrackingIds[0] : 0u,
                pTrackingIds ? pTrackingIds[1] : 0u,
                hr);
    return hr;
}

} // extern "C"
