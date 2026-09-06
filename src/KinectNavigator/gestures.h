#pragma once
#include "nui_types.h"

// Two recognisers behind Gestures::Update, selected by cfg.navModel:
//   extend (default) : ExtendUpdate() -- an air d-pad on the dominant shoulder.
//   swipe (fallback) : the older motion model -- L/R + U/D hand swipes, an
//                      overhead-raise for Confirm, an arm-down-and-out for Back.
// Both emit the same GestureAction set below.

enum class GestureAction
{
    None = 0,
    Left,
    Right,
    Up,
    Down,
    Confirm,
    Back,
};

// Snapshot of the recognizer internals, for the dev overlay.
struct GestureDebug
{
    bool     haveHand     = false;
    bool     inGameplay   = false;    // muted: in a song
    unsigned lastAction   = 0;        // GestureAction of the most recent emit
    LONGLONG lastActionMs = -1;       // frame-clock ms of that emit

    float    domEx = 0, domEy = 0;    // dominant hand pos (torso, from SHOULDER_CENTER; +x=user right, +y=up)
    float    domVx = 0, domVy = 0;    // dominant hand velocity (torso/s)
    int      swipeAxis    = 0;        // mid-swipe: 0 none, 1 horizontal, 2 vertical
    int      repeatState  = 0;        // 0 idle, 1 post-swipe window, 2 holding (arm extended)
    float    armExtend    = 0;        // |hand-shoulder| / arm-contour-length
    float    armLevel     = 9.9f;     // |dy|/|dx| of hand-vs-shoulder: small = arm horizontal
    bool     armLevelOk   = false;    // arm within +-armLevelTanMax of horizontal (repeat allowed)

    int      confirmHeldMs = 0, confirmNeedMs = 2000;
    int      backHeldMs    = 0, backNeedMs    = 2000;

    // Phase 1 parallel signal conditioning (not consumed by the recogniser yet)
    float    f1eEx = 0, f1eEy = 0;    // 1-Euro filtered dominant hand pos (torso, from SC; mirror-applied)
    float    sgVx  = 0, sgVy  = 0;    // Savitzky-Golay velocity off the 1-Euro output (torso/s; mirror-applied)

    // nav_model = extend (air d-pad). domEx/domEy = hand vs dominant shoulder (torso),
    // armExtend = radial distance r.
    bool     dpadMode   = false;      // the extend recogniser is the active one
    bool     dpadArmed  = false;      // clutch engaged (park to arm; idle to disarm) -- the DRIVER
    bool     dpadParked = false;      // hand in the park box
    bool     dpadCmd    = false;      // non-dominant hand on the non-dom shoulder (command mode)
    int      dpadWedge  = 0;          // 0 none, 1 RIGHT, 2 LEFT, 3 UP, 4 DOWN
    float    dpadParkR  = 0.32f;      // park-box radius (torso units) -- for the overlay to scale
    int      dpadCmdPct = 0;          // command-mode dwell progress 0..100 (before Enter/Esc fires)
    float    dpadNdDist = 9.9f;       // non-dom hand -> non-dom shoulder (torso); < dpadCmdGateR => command mode
    float    dpadCmdGateR = 0.42f;    // the gate radius, for the overlay
    unsigned long dpadDriverId = 0;   // tracking-id of the body currently driving (0 = nobody armed)
    int      dpadNumBodies = 0;       // tracked skeletons the d-pad is watching this frame (0..2 on v1)
    float    dpadEnergy = 0.f;        // driver's whole-body motion (trunk+head, torso/s EMA) -- dance-disarm
};

namespace Gestures
{
    // swipe model: one navigator body, picked by the recognizer.
    GestureAction Update(const NUI_SKELETON_DATA& nav, LONGLONG frameMs);

    // extend model (air d-pad): every tracked body runs its own clutch; the last
    // one to arm (park its hand at its shoulder) becomes the driver and the only
    // one whose reaches emit keys. Old driver must re-arm. Called once per frame
    // with the whole skeleton frame.
    GestureAction UpdateExtend(const NUI_SKELETON_FRAME& frame, LONGLONG frameMs);

    // Clear all state -- on body lost (no tracked skeletons at all).
    void Reset();

    void GetDebug(GestureDebug& out);
}
