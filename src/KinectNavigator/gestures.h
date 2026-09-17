#pragma once
#include "nui_types.h"

// One recogniser: Gestures::UpdateExtend() -- an air d-pad on the dominant
// shoulder. (An older swipe model behind Gestures::Update was removed in v1.1.)

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

// One tracked body's skeleton + role, for the HUD's full-skeleton mirror view. `role` mirrors
// the recognizer's internal DpadState (0 Asleep, 1 Arming, 2 Armed, 3 Driving, 4 Demoted) as a
// plain int so this header doesn't need that enum. Joint positions are the raw Kinect skeleton
// (camera-space meters, NOT yet torso-normalized or mirrored -- the renderer does that, the same
// way domEx/domEy already are) so one BodyDebug can also stand in for a synthetic/ghost pose fed
// from a keyframe table instead of a live frame.
struct BodyDebug
{
    bool     inUse  = false;
    DWORD    id     = 0;
    int      role   = 0;
    float    torso  = 0.40f;      // |SHOULDER_CENTER - HIP_CENTER|, for scaling the figure
    float    ex = 0, ey = 0, r = 0;   // dominant hand vs dominant shoulder (torso units) -- same as GestureDebug's, per-body

    Vector4                              joints[NUI_SKELETON_POSITION_COUNT]{};
    NUI_SKELETON_POSITION_TRACKING_STATE jointState[NUI_SKELETON_POSITION_COUNT]{};
};

// Snapshot of the recognizer internals, for the dev overlay.
struct GestureDebug
{
    bool     haveHand     = false;
    bool     inGameplay   = false;    // "in a song" -- firmer wake hold, HUD badge (no key muting)
    unsigned lastAction   = 0;        // GestureAction of the most recent emit
    LONGLONG lastActionMs = -1;       // frame-clock ms of that emit

    float    domEx = 0, domEy = 0;    // dominant hand pos (torso, from the dominant SHOULDER; +x=user right, +y=up)
    float    domVx = 0, domVy = 0;    // dominant hand velocity (torso/s) -- 0 in the d-pad HUD
    int      swipeAxis    = 0;        // active wedge axis: 0 none, 1 horizontal (L/R), 2 vertical (U/D)
    int      repeatState  = 0;        // 0 idle, 1 fired (awaiting repeat), 2 auto-repeating
    float    armExtend    = 0;        // d-pad: radial hand distance r from the dominant shoulder (torso)

    // air d-pad HUD. domEx/domEy = hand vs dominant shoulder (torso), armExtend = radial distance r.
    bool     dpadMode   = false;      // the extend recogniser is the active one
    bool     dpadArmed  = false;      // clutch engaged (park to arm; idle to disarm) -- the DRIVER
    bool     dpadParked = false;      // hand in the park box
    bool     dpadCmd    = false;      // non-dominant hand on the non-dom shoulder (command mode)
    int      dpadWedge  = 0;          // 0 none, 1 RIGHT, 2 LEFT, 3 UP, 4 DOWN
    float    dpadParkR  = 0.32f;      // park-box radius (torso units) -- for the overlay to scale
    float    dpadUpReachK    = 0.85f; // park-exit warp on +y (up) -- see config.h dpadUpReachK
    float    dpadCrossReachK = 1.30f; // park-exit warp on -x (cross-body) -- see config.h dpadCrossReachK
    int      dpadCmdPct = 0;          // command-mode dwell progress 0..100 (before Enter/Esc fires)
    float    dpadNdDist = 9.9f;       // non-dom hand -> non-dom shoulder (torso); < dpadCmdGateR => command mode
    float    dpadCmdGateR = 0.42f;    // the gate radius, for the overlay
    unsigned long dpadDriverId = 0;   // tracking-id of the body currently driving (0 = nobody armed)
    int      dpadNumBodies = 0;       // tracked skeletons the d-pad is watching this frame (0..2 on v1)

    // full-skeleton mirror view: every candidate body this frame (0..2 on real v1 hardware;
    // 4 slots of headroom, matching the recognizer's g_db[4]). bodyCount may be less than
    // dpadNumBodies would suggest if there were ever more than 4 simultaneous candidates.
    static const int kMaxBodies = 4;
    BodyDebug bodies[kMaxBodies];
    int       bodyCount = 0;
};

namespace Gestures
{
    // air d-pad: every tracked body runs its own clutch; the last one to arm
    // (park its hand at its shoulder) becomes the driver and the only one whose
    // reaches emit keys. Old driver must re-arm. Called once per frame with the
    // whole skeleton frame.
    GestureAction UpdateExtend(const NUI_SKELETON_FRAME& frame, LONGLONG frameMs);

    // Clear all state -- on body lost (no tracked skeletons at all).
    void Reset();

    void GetDebug(GestureDebug& out);
}
