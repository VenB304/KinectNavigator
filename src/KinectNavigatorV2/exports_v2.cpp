// KinectNavigatorV2 -- the one function Legacy.exe imports by name from
// Kinect20.dll (plan doc section 2). Forwards unchanged, keeps a private
// AddRef'd copy of the real sensor, and starts our own co-reader on it -- the
// exact analogue of the v1 proxy's ordinal thunks calling Backend::Ensure().

#include "v2_framework.h"
#include "backend_v2.h"
#include "v2reader.h"
#include "engine_v2.h"
#include "log.h"

namespace { volatile LONG s_started = 0; }

extern "C" HRESULT WINAPI GetDefaultKinectSensor(IKinectSensor** ppSensor)
{
    BackendV2::Ensure();

    HRESULT hr = BackendV2::fn.GetDefaultKinectSensor
               ? BackendV2::fn.GetDefaultKinectSensor(ppSensor)
               : E_FAIL;

    LogLine("[v2] GetDefaultKinectSensor -> hr=0x%08lX sensor=%p",
            hr, (ppSensor && SUCCEEDED(hr)) ? (void*)*ppSensor : nullptr);

    // Start our co-reader at most once, on our own AddRef'd copy -- the game
    // may call this more than once (R5 in the risk register).
    if (SUCCEEDED(hr) && ppSensor && *ppSensor &&
        InterlockedCompareExchange(&s_started, 1, 0) == 0)
    {
        (*ppSensor)->AddRef(); // our own private copy; game's pointer returned unwrapped
        EngineV2::Start();     // consumer first, so the first published frame has a listener
        V2Reader::Start(*ppSensor);
    }

    return hr;
}
