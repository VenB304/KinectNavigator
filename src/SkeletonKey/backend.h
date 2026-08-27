#pragma once
#include "framework.h"

// Thin loader for the renamed genuine Kinect runtime (Kinect10_backend.dll).
// Every function Legacy.exe imports from Kinect10.dll by ordinal is resolved here
// by name and stored as a __stdcall function pointer.
//
// M1 only forwards. The signatures below are deliberately loose (void** for the
// INuiSensor out-param, void* for NUI_SKELETON_FRAME) because M1 never inspects
// those structures -- it just passes them straight through. M2 tightens them.

namespace Backend
{
    struct Fns
    {
        HRESULT (__stdcall *NuiGetSensorCount)(int* pCount);
        HRESULT (__stdcall *NuiCreateSensorByIndex)(int index, void** ppSensor);
        HRESULT (__stdcall *NuiCreateSensorById)(const wchar_t* strInstanceId, void** ppSensor);
        HRESULT (__stdcall *NuiImageGetColorPixelCoordinatesFromDepthPixel)(
                    int eColorResolution, const void* pcViewArea,
                    long lDepthX, long lDepthY, unsigned short usDepthValue,
                    long* plColorX, long* plColorY);
        void    (__stdcall *NuiShutdown)(void);
        HRESULT (__stdcall *NuiSkeletonGetNextFrame)(DWORD dwMillisecondsToWait, void* pSkeletonFrame);
        HRESULT (__stdcall *NuiSetDeviceStatusCallback)(void* callback, void* pUserData);
        HRESULT (__stdcall *NuiSkeletonSetTrackedSkeletons)(DWORD* pTrackingIds);
    };

    extern Fns fn;

    // Idempotent. Loads Kinect10_backend.dll from this DLL's directory and binds
    // all eight functions. Safe to call from every export thunk; the real work
    // runs once via InitOnceExecuteOnce.
    void Ensure();

    // Frees the backend module. Called from our NuiShutdown thunk and on
    // DLL_PROCESS_DETACH.
    void Shutdown();
}
