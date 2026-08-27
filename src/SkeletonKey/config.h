#pragma once
#include "framework.h"

// M3 config. Hardcoded defaults, overridable by a "kinectnav.ini" next to the
// module (key = value, '#' or ';' comments). M4 replaces this with the full
// JSON schema; the field names line up with kinectnav.default.json.

struct Config
{
    bool   enable            = true;
    bool   mirror            = false;   // flip swipe left/right
    bool   requireForeground = true;    // only emit keys when the game window is foreground

    // swipe detector (units: torso-lengths, torso-lengths/sec, frames)
    float  swipeVelocity     = 1.6f;
    float  swipeDistance     = 0.28f;
    int    swipeMinFrames    = 3;

    // debounce
    int    cooldownMs        = 450;
    int    neutralHoldMs     = 200;
    int    armAfterFrames    = 15;

    // smoothing
    float  smoothFast        = 0.5f;
    float  smoothSlow        = 0.15f;

    // output
    unsigned keyRight        = 0x27;    // VK_RIGHT
    int    keyPressMs        = 40;

    // debug: periodic hand-signal trace to the log (5 Hz of capture time)
    bool   trace             = false;
};

namespace Cfg
{
    // Loads kinectnav.ini from the module directory over the defaults. Idempotent.
    const Config& Load();
    const Config& Get();      // last loaded (defaults if never loaded)
}
