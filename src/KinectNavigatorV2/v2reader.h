#pragma once
#include "v2_framework.h"

// The passive co-reader. Opens a SECOND IBodyFrameReader on the same
// IKinectSensor the game is already using (v2 explicitly allows multiple
// readers per sensor, per-process, unlike v1) and converts each body frame to a
// KN_RichFrame. Never touches anything the game calls -- see plan doc 4a.
//
// Each converted frame is published to RichBuffer (the recogniser's input) and a few
// joints / body acquire-lose events are logged.
namespace V2Reader
{
    // Starts the polling thread against `sensor`. Idempotent -- a second
    // call while already running is a no-op. Takes over the caller's
    // AddRef'd reference to `sensor` (released by Stop()).
    void Start(IKinectSensor* sensor);

    // Signals the thread to stop and joins it. Safe to call from
    // DLL_PROCESS_DETACH when the process is NOT terminating; skipped
    // entirely when it is (see dllmain.cpp).
    void Stop();
}
