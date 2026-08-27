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
    float  swipeVelocity     = 1.6f;    // min hand speed to start a swipe
    float  swipeVxCeiling    = 8.0f;    // above this = inferred-joint jitter, ignore
    float  swipeDistance     = 0.28f;
    int    swipeMinFrames    = 3;

    // hold-to-repeat: after a swipe, if the hand stays raised + out to the side
    float  holdEnterFx       = 0.75f;   // hand extended past here (raised) -> start repeating
    float  holdRaisedFy      = -0.35f;  // "raised" = hand y above this (rel. shoulder-centre)
    float  holdExitFx        = 0.40f;   // hand pulled in past here -> stop repeating
    float  holdExitFy        = -0.50f;  // or hand dropped below here -> stop
    int    firstRepeatMs     = 400;
    int    minRepeatMs       = 180;
    int    repeatAccelMs     = 25;      // each repeat shortens the interval by this
    int    postSwipeMs       = 250;     // window after a swipe to decide single vs hold

    // debounce / re-arm
    int    cooldownMs        = 200;     // min settle in ReArm before the next swipe
    float  recenterFx        = 0.35f;   // hand back inside here -> re-armed
    float  recenterFy        = -0.45f;  // or hand dropped below here -> re-armed
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
