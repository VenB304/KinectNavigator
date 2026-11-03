#pragma once
#include "v2_framework.h"

// Thin loader for the renamed genuine v2 runtime (Kinect20_backend.dll).
// Mirrors src/KinectNavigator/backend.h, scoped to the one function the
// game actually imports from Kinect20.dll (see the plan doc, section 2).
namespace BackendV2
{
    struct Fns
    {
        HRESULT (WINAPI *GetDefaultKinectSensor)(IKinectSensor** ppSensor);
    };

    extern Fns fn;

    // Idempotent. Loads Kinect20_backend.dll from this DLL's directory.
    void Ensure();
}
