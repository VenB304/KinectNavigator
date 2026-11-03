#pragma once

// Brings up the navigation engine inside the Kinect v2 proxy: the same pieces the Kinect10.dll
// proxy starts in Backend::Ensure (config, game probes, recogniser, HUD), except the recogniser
// is fed by RichBuffer. Idempotent. Nothing is shared with a Kinect10.dll proxy in the same
// process; until the cross-proxy arbitration milestone (M3) run only one of them against a
// given sensor.
#include "rich_types.h"

namespace EngineV2
{
    void Start();
    void Stop();

    // Called by the v2 reader for every accepted body frame: appends it to the capture
    // file (when record.flag is present) and hands it to the recogniser.
    void OnFrame(const KN_RichFrame& frame);
}
