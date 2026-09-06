#include "framework.h"
#include <math.h>
#include <stdio.h>
#include "gestures.h"
#include "filter.h"
#include "globals.h"
#include "config.h"
#include "log.h"

// Both recognisers (see gestures.h): ExtendUpdate() is the default air d-pad,
// the swipe model is the fallback. All positions are body-relative to
// SHOULDER_CENTER, in torso lengths (torso = |SHOULDER_CENTER - HIP_CENTER|).
// "ex" is effective X: + = the user's right, with `mirror` flipping it.

namespace
{
    struct Hand
    {
        bool  init = false, seen = false;
        float fx = 0, fy = 0;      // fast track (velocity)
        float sx = 0, sy = 0;      // slow track (held pose)
        float fxPrev = 0, fyPrev = 0;
        float vx = 0, vy = 0;      // torso/s from the fast track
    };

    void Smooth(Hand& h, bool usable, float rx, float ry, double dt,
                float aF, float aS, bool reset)
    {
        h.seen = usable;
        if (!usable) return;
        if (!h.init || reset)
        {
            h.fx = h.sx = h.fxPrev = rx;
            h.fy = h.sy = h.fyPrev = ry;
            h.vx = h.vy = 0;
            h.init = true;
            return;
        }
        h.fx = aF * rx + (1 - aF) * h.fx;
        h.fy = aF * ry + (1 - aF) * h.fy;
        h.sx = aS * rx + (1 - aS) * h.sx;
        h.sy = aS * ry + (1 - aS) * h.sy;
        if (dt > 0.0)
        {
            h.vx = (float)((h.fx - h.fxPrev) / dt);
            h.vy = (float)((h.fy - h.fyPrev) / dt);
        }
        h.fxPrev = h.fx;
        h.fyPrev = h.fy;
    }

    enum RS { RS_IDLE = 0, RS_POST = 1, RS_HOLD = 2 };

    struct State
    {
        bool     have   = false;
        LONGLONG prevMs = -1;
        int      frames = 0;

        Hand     nd;    // non-dominant hand (Back pose) -- EMA smoothed

        // dominant hand signal conditioning: 1 Euro -> position (f1eX/f1eY),
        // 5-point Savitzky-Golay -> velocity (sgVx/sgVy). Feeds the swipe path.
        OneEuro  oeX, oeY;
        SavGol5  sgX, sgY;
        float    f1eX = 0, f1eY = 0, sgVx = 0, sgVy = 0;

        // swipe in progress -- segmented at direction reversals; only the last segment is judged
        bool     swActive   = false;
        int      swSettle   = 99;   // leaky "slow frames" count since the last swipe
        int      swAxis     = 0;    // 1 horizontal, 2 vertical
        int      swFrames   = 0;    // total frames since swActive (stale-abort)
        int      swSegs     = 0;    // number of segments (direction changes + 1)
        int      swSegDir   = 0;    // current segment direction -1/+1 (also the fired direction)
        int      swSegFrames = 0;   // frames in the current segment
        float    swSegStart = 0;    // position where the current segment began (last turnaround)
        float    swSegExt   = 0;    // furthest position reached in swSegDir this segment
        float    swSegPeak  = 0;    // peak |velocity| in the current segment
        LONGLONG swCooldownMs = 0;
        LONGLONG swSettleUntil = 0; // settle gate enforced only while frameMs < this
        bool     wasMuted  = false;
        bool     swZoneRejLogged = false;   // logged the "ended out of zone" diag once this swipe

        // Left/Right hold-to-repeat
        int      rs = RS_IDLE;
        int      rsDir = 0;         // -1 left / +1 right (effective)
        LONGLONG rsPostUntil = 0;
        LONGLONG rsNextRepeat = 0;
        int      rsRepeats = 0;
        int      rsRelaxFrames = 0;

        // Confirm / Back dwells (+ auto-repeat while the pose is held)
        LONGLONG confirmSince = -1;  int confirmGrace = 0;  bool confirmArmed = true;  LONGLONG confirmRepeatNext = 0;
        LONGLONG backSince    = -1;  int backGrace    = 0;  bool backArmed    = true;  LONGLONG backRepeatNext    = 0;
        LONGLONG gestureCooldownMs = 0;

        LONGLONG lastTraceMs = -1;
    } g;

    GestureDebug g_dbg;

    LONGLONG g_epoch = -1;
    double   T(LONGLONG ms) { if (g_epoch < 0) g_epoch = ms; return (ms - g_epoch) / 1000.0; }

    inline float Len3(const Vector4& a, const Vector4& b)
    {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }
    inline bool Usable(NUI_SKELETON_POSITION_TRACKING_STATE s)
    {
        return s == NUI_SKELETON_POSITION_TRACKED || s == NUI_SKELETON_POSITION_INFERRED;
    }

    // ================= nav_model = extend : shoulder-centred air d-pad ==================
    // Multiplayer hand-off: every tracked skeleton runs its own signal filter + clutch
    // (a DpadBody slot, keyed by tracking-id). The last body to finish arming becomes the
    // driver; only the driver's reaches run the nav-machine and emit keys. When someone
    // else arms, the old driver is forced back to ASLEEP (must re-park to drive again).

    struct DpadBody
    {
        // identity / lifetime
        DWORD    id     = 0;
        bool     inUse  = false;
        LONGLONG lastSeenMs = -1;

        // signal: 1 Euro on the dominant hand vs the dominant shoulder (torso units)
        bool     have    = false;
        LONGLONG prevMs  = -1;
        int      frames  = 0;
        OneEuro  oeX, oeY;

        // this-frame geometry (filled by ProcessBody)
        float    ex = 0, ey = 0, r = 0;
        float    torso = 0.40f;         // last good |SC-HIP| -- reused across a brief trunk glitch
        int      wedge  = 0;
        bool     parked = true;         // sticky (hysteresis)
        bool     handOk = false;

        // clutch
        bool     armed  = false;
        LONGLONG parkSinceMs = 0;       // arming dwell
        LONGLONG restSinceMs = 0;       // idle-out-of-play timer for auto-disarm
        LONGLONG armedAtMs   = 0;       // frameMs arming last completed -- "last to arm wins"
        bool     mustLeavePark = false; // set when demoted while parked: can't re-arm (re-bid for
                                        // control) until the hand actually leaves the park box once

        // whole-body motion (trunk + head) for the dance auto-disarm
        bool     motInit = false;
        bool     headWasOk = false;     // HEAD tracked last frame -> pHead is fresh, safe to diff
        Vector4  pHip{}, pSc{}, pHead{};
        float    bodyEnergy = 0.f;      // EMA of weighted trunk/head speed (torso/s)
        LONGLONG danceSinceMs = 0;
    };

    // The nav-machine. Only ever run for the driver, so it lives ONCE next to the driver pointer
    // instead of lying dormant in all four body slots. It travels with control: every driver
    // change and every driver disarm resets it.
    struct NavMachine
    {
        int      occKey = 0, occFrames = 0, grace = 0;
        bool     fired = false, suppressed = false, cmdOn = false;
        LONGLONG occStartMs = 0, firedMs = 0, nextRepeat = 0;
        int      repeats = 0;
        LONGLONG lastTraceMs = -1;
    };

    struct ExtendMulti
    {
        DWORD    driverId  = 0;
        LONGLONG handoffMs = -1000000;
        LONGLONG lastTraceMs = -1;
        LONGLONG lastFrameMs = 0;       // detect a backward sensor-clock jump (replay --loop wrap)
        bool     hadBodies   = false;   // ACQUIRED / LOST edge (this layer owns body lifetime)
        NavMachine nav;                 // the driver's occupancy / fire / repeat state
    };

    // shared dropout-tolerance window: how long a body can be absent from the candidate set (or
    // SC/HIP-glitched) and still keep its slot / clutch / driver seat. Also used recogniser-side.
    const LONGLONG kDropoutGraceMs = 400;

    DpadBody    g_db[4];
    ExtendMulti em;

    const char* DpadName(GestureAction a)
    {
        switch (a) {
        case GestureAction::Left:  return "LEFT";   case GestureAction::Right: return "RIGHT";
        case GestureAction::Up:    return "UP";     case GestureAction::Down:  return "DOWN";
        case GestureAction::Confirm: return "CONFIRM"; case GestureAction::Back: return "BACK";
        default: return "?";
        }
    }

    void ResetNav()   // clear the driver's nav-machine (on disarm / driver change)
    {
        NavMachine& n = em.nav;
        n.occKey = 0; n.occFrames = 0; n.grace = 0;
        n.fired = false; n.suppressed = false; n.cmdOn = false;
        n.occStartMs = 0; n.firedMs = 0; n.nextRepeat = 0; n.repeats = 0;
    }

    DpadBody* FindDpadSlot(DWORD id);   // fwd

    // The ONE place a body's state is transcribed into the overlay debug block. b == nullptr means
    // "no body to show" -> blank the live readouts. Callers that go on to run the nav-machine
    // overwrite wedge / cmd / ndDist with their own richer values afterwards.
    void PopulateDpadDebug(const DpadBody* b, const Config& c)
    {
        g_dbg.dpadMode      = true;
        g_dbg.dpadParkR     = c.dpadParkRadius;
        g_dbg.dpadCmdGateR  = c.dpadCmdGateRadius;
        g_dbg.domVx         = g_dbg.domVy = 0.f;
        g_dbg.confirmHeldMs = g_dbg.backHeldMs = 0;
        g_dbg.inGameplay    = false;
        g_dbg.dpadNdDist    = 9.9f;
        g_dbg.dpadCmd       = false;
        g_dbg.dpadWedge     = 0;
        g_dbg.dpadCmdPct    = 0;
        g_dbg.swipeAxis     = 0;
        g_dbg.dpadDriverId  = em.driverId;
        g_dbg.repeatState   = em.nav.repeats > 0 ? 2 : (em.nav.fired ? 1 : 0);
        g_dbg.haveHand      = b ? b->handOk     : false;
        g_dbg.domEx         = b ? b->ex         : 0.f;
        g_dbg.domEy         = b ? b->ey         : 0.f;
        g_dbg.armExtend     = b ? b->r          : 0.f;
        g_dbg.dpadParked    = b ? b->parked     : true;
        g_dbg.dpadArmed     = b ? b->armed      : false;
        g_dbg.dpadEnergy    = b ? b->bodyEnergy : 0.f;
    }

    DpadBody* FindDpadSlot(DWORD id)   // existing slot for this id, or nullptr (no allocation)
    {
        for (auto& b : g_db) if (b.inUse && b.id == id) return &b;
        return nullptr;
    }
    DpadBody* DpadSlotFor(DWORD id)
    {
        if (DpadBody* e = FindDpadSlot(id)) return e;
        for (auto& b : g_db) if (!b.inUse) { b = DpadBody{}; b.id = id; b.inUse = true; return &b; }
        // all four in use -> evict the least-recently-seen, but never the current driver's slot
        DpadBody* lru = nullptr;
        for (auto& b : g_db)
            if (b.id != em.driverId && (!lru || b.lastSeenMs <= lru->lastSeenMs)) lru = &b;
        if (!lru) lru = &g_db[0];
        *lru = DpadBody{}; lru->id = id; lru->inUse = true; return lru;
    }

    // Per-body signal + park box + wedge + clutch. Fills b.ex/ey/r/wedge/parked, and
    // stamps b.armedAtMs whenever the body completes arming (used to pick the driver).
    // geomOk == false: the skeleton is still TRACKED but SHOULDER_CENTER/HIP_CENTER glitched out
    // this frame. The hand signal is shoulder-relative and scaled by the CACHED torso, so it keeps
    // working normally; only the dance-energy metric (which reads SC/HIP) sits that frame out.
    void ProcessBody(DpadBody& b, const NUI_SKELETON_DATA& nav, float torso,
                     LONGLONG frameMs, const Config& c, bool geomOk)
    {
        const auto st = [&](int j) { return nav.eSkeletonPositionTrackingState[j]; };
        const int shJ = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_LEFT : NUI_SKELETON_POSITION_SHOULDER_RIGHT;
        const int hJ0 = c.leftHanded ? NUI_SKELETON_POSITION_HAND_LEFT     : NUI_SKELETON_POSITION_HAND_RIGHT;
        const int wJ0 = c.leftHanded ? NUI_SKELETON_POSITION_WRIST_LEFT    : NUI_SKELETON_POSITION_WRIST_RIGHT;

        const double dt = (b.prevMs >= 0) ? (frameMs - b.prevMs) / 1000.0 : 0.0;
        b.prevMs = frameMs;
        // ONE dropout window: the 1 Euro resync bound IS kDropoutGraceMs, so any gap the slot
        // prune rides through, the filter rides through too. These used to disagree (250 ms here
        // vs a 400 ms grace), silently disarming the driver on a 264-396 ms dropout.
        const bool resync = (!b.have || dt <= 0.0 || dt > kDropoutGraceMs / 1000.0);

        int hJ = Usable(st(hJ0)) ? hJ0 : wJ0;
        const bool handOk = Usable(st(shJ)) && Usable(st(hJ));
        const float m = c.mirror ? -1.f : 1.f;
        float rawx = 0.f, rawy = 0.f;
        if (handOk)
        {
            const Vector4& sh = nav.SkeletonPositions[shJ];
            const Vector4& hd = nav.SkeletonPositions[hJ];
            rawx = m * (hd.x - sh.x) / torso;
            rawy = (hd.y - sh.y) / torso;
        }

        if (resync)
        {
            b.oeX.Configure(c.filter1eMinCutoff, c.filter1eBeta, c.filter1eDCutoff);
            b.oeY.Configure(c.filter1eMinCutoff, c.filter1eBeta, c.filter1eDCutoff);
            b.oeX.Reset(); b.oeY.Reset();
            // NB: mustLeavePark is deliberately NOT cleared here -- a demoted-while-parked driver
            // that suffers a glitch must still leave the box before it can re-bid for control.
            b.parked = true; b.armed = false; b.parkSinceMs = 0; b.restSinceMs = 0;
            b.motInit = false; b.headWasOk = false; b.bodyEnergy = 0.f; b.danceSinceMs = 0;
            if (b.id == em.driverId) ResetNav();
            b.have = true; b.frames = 1; b.handOk = handOk;
            b.ex = b.ey = b.r = 0.f; b.wedge = 0;
            return;
        }

        const float ex = handOk ? b.oeX.Filter(rawx, (float)dt) : 0.f;
        const float ey = handOk ? b.oeY.Filter(rawy, (float)dt) : 0.f;
        b.ex = ex; b.ey = ey; b.r = sqrtf(ex * ex + ey * ey); b.handOk = handOk;
        ++b.frames;

        // whole-body motion (trunk + head; the dominant hand is deliberately excluded -- it moves
        // for navigation). Weighted speed, torso/s, EMA-smoothed -> b.bodyEnergy for the dance disarm.
        // Only computed when the feature is on. HEAD is optional -- it is NOT gated in the candidate
        // filter, so a HEAD dropout would otherwise feed a stale position and spike the energy.
        if (!geomOk) b.motInit = false;   // SC/HIP glitched -> re-seed, never diff across the gap
        if (c.dpadDanceDisarm && geomOk)
        {
            const bool headOk = Usable(st(NUI_SKELETON_POSITION_HEAD));
            const Vector4& hip = nav.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER];
            const Vector4& sc  = nav.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER];
            const Vector4& hd2 = nav.SkeletonPositions[NUI_SKELETON_POSITION_HEAD];
            if (!b.motInit) { b.pHip = hip; b.pSc = sc; b.pHead = hd2; b.motInit = true; b.bodyEnergy = 0.f; }
            else if (dt > 0.0)
            {
                auto spd = [&](const Vector4& a, const Vector4& p) { return Len3(a, p) / torso / (float)dt; };
                float e = 2.f * spd(hip, b.pHip) + 2.f * spd(sc, b.pSc);
                float wsum = 4.f;
                if (headOk && b.headWasOk) { e += spd(hd2, b.pHead); wsum = 5.f; }   // both frames -> safe to diff
                if (headOk) b.pHead = hd2;                                            // else recovery frame just re-seeds
                b.pHip = hip; b.pSc = sc;
                b.bodyEnergy = 0.25f * (e / wsum) + 0.75f * b.bodyEnergy;
            }
            b.headWasOk = headOk;
        }

        if (b.frames < c.armAfterFrames) { b.wedge = 0; return; }

        // "dancing" = whole-body motion sustained past a short window. ONE predicate
        // (bodyEnergy > dpadDanceEnergy) drives both the arm quiescence gate (short sustain) and
        // the auto-disarm (dpadDanceHoldMs), so the two can never disagree about the threshold.
        if (c.dpadDanceDisarm && b.bodyEnergy > c.dpadDanceEnergy)
        { if (b.danceSinceMs == 0) b.danceSinceMs = frameMs; }
        else b.danceSinceMs = 0;
        const LONGLONG danceMs = b.danceSinceMs ? (frameMs - b.danceSinceMs) : 0;
        const bool dancing = b.danceSinceMs != 0 && danceMs >= 150;   // ~5 sustained frames

        // Dance auto-disarm. Deliberately ABOVE the !handOk return: it needs no hand signal, so an
        // armed body whose wrist has dropped out still goes to sleep once the user starts dancing
        // (nothing below here could disarm it -- it would stay armed indefinitely). Clutch-only:
        // with dpad_arm = 0 the user asked for "always live", and the unconditional re-arm would
        // fight this every single frame (disarm -> re-arm -> disarm, one log line per frame).
        if (c.dpadArm && b.armed && b.danceSinceMs != 0 && danceMs >= c.dpadDanceHoldMs)
        {
            b.armed = false; if (b.id == em.driverId) ResetNav(); b.mustLeavePark = b.parked;
            LogLine("Gesture[t=%.1f]: dpad disarmed id=%lu (dancing, en=%.1f)", T(frameMs), b.id, b.bodyEnergy);
        }

        if (!handOk) { b.wedge = 0; return; }   // no hand signal -> no park box, no wedge, no arming

        // park box (asymmetric hysteresis + anisotropic reach -- Batch 1). The park-exit /
        // re-park test scales one axis so UP needs more raise (less eager) and cross-body
        // LEFT needs less (easier). Wedges and the HUD keep the raw ex/ey.
        float exs = ex, eys = ey;
        if (ey > 0.f) eys = ey * c.dpadUpReachK;
        if (ex < 0.f) exs = ex * c.dpadCrossReachK;
        const float rPark = sqrtf(exs * exs + eys * eys);
        if (b.parked) { if (rPark > c.dpadParkRadius * c.dpadParkExitK) b.parked = false; }
        else          { if (rPark < c.dpadParkRadius)                   b.parked = true;  }

        // which wedge? (component tests; gaps between wedges + straight-down are dead)
        const float ax = fabsf(ex), ay = fabsf(ey);
        const float tw = c.dpadWedgeTanHalf;   // tan(dpadWedgeHalfDeg), precomputed in Cfg::Load
        int wedge = 0;
        if      (ex > 0 && ay <= ax * tw) wedge = 1;   // RIGHT
        else if (ex < 0 && ay <= ax * tw) wedge = 2;   // LEFT
        else if (ey > 0 && ax <= ay * tw) wedge = 3;   // UP
        else if (ey < -c.dpadDownMinDrop && ax >= c.dpadDownMinOut && ax <= ay) wedge = 4;  // DOWN-and-out
        b.wedge = wedge;

        // clutch: park to arm, idle-out-of-play (and lowered) to disarm
        if (!c.dpadArm)
        {
            // clutch disabled: the body is always live. (The hand-off path is what would ping-pong
            // control between two always-armed people, and that is blocked separately -- see
            // mayTakeOver in UpdateExtend.)
            if (!b.armed) { b.armed = true; b.armedAtMs = frameMs; }
        }
        else if (b.parked)
        {
            if (b.parkSinceMs == 0) b.parkSinceMs = frameMs;
            b.restSinceMs = 0;
            // mustLeavePark: after losing control while parked, don't auto-re-arm (that would
            // ping-pong control between two people both holding a hand at their shoulder) --
            // the hand has to leave the park box and come back to make a fresh bid.
            // Quiescence gate: can't finish arming while the body is sustainedly moving (dancing).
            if (!b.armed && !b.mustLeavePark && !dancing && frameMs - b.parkSinceMs >= c.dpadArmDwellMs)
            {
                b.armed = true; b.armedAtMs = frameMs;
                LogLine("Gesture[t=%.1f]: dpad ARMED id=%lu", T(frameMs), b.id);
            }
        }
        else
        {
            b.parkSinceMs = 0;
            b.mustLeavePark = false;   // hand is outside the park box -> a fresh park can re-bid
            // idle-disarm only when the hand is out of every wedge AND lowered. A raised hand
            // in the dead gap between cones is "aiming" (UP / Confirm setup), not "done".
            if (wedge == 0 && ey < c.dpadSleepBelowY)
            {
                if (b.restSinceMs == 0) b.restSinceMs = frameMs;
                if (b.armed && frameMs - b.restSinceMs >= c.dpadDisarmMs)
                {
                    b.armed = false; if (b.id == em.driverId) ResetNav();
                    LogLine("Gesture[t=%.1f]: dpad disarmed id=%lu (idle %dms)", T(frameMs), b.id, c.dpadDisarmMs);
                }
            }
            else b.restSinceMs = 0;
        }
    }

    // The nav-machine: command-mode gate + occupancy + fire/repeat. Runs for the DRIVER only,
    // on the geometry ProcessBody already put in b.ex/ey/wedge/parked. Unchanged from the
    // single-body model apart from operating on `b` instead of a global.
    GestureAction RunNavMachine(DpadBody& b, const NUI_SKELETON_DATA& nav, float torso,
                                LONGLONG frameMs, const Config& c)
    {
        const auto st = [&](int j) { return nav.eSkeletonPositionTrackingState[j]; };
        const int nShJ = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_RIGHT : NUI_SKELETON_POSITION_SHOULDER_LEFT;
        const int nHJ0 = c.leftHanded ? NUI_SKELETON_POSITION_HAND_RIGHT     : NUI_SKELETON_POSITION_HAND_LEFT;
        const int nWJ0 = c.leftHanded ? NUI_SKELETON_POSITION_WRIST_RIGHT    : NUI_SKELETON_POSITION_WRIST_LEFT;

        NavMachine& nv = em.nav;
        const float ex = b.ex, ey = b.ey, r = b.r;
        const int wedge = b.wedge;
        float ndDist = 9.9f;

        PopulateDpadDebug(&b, c);

        auto trace = [&](const char* tag) {
            if (c.trace && (nv.lastTraceMs < 0 || frameMs - nv.lastTraceMs >= 66)) {
                nv.lastTraceMs = frameMs;
                LogLine("dpad[t=%.1f] drv=%lu ex=%+.2f ey=%+.2f r=%.2f arm=%d park=%d w=%d cmd=%d nd=%.2f cdw=%d occf=%d fired=%d rep=%d en=%.1f %s",
                        T(frameMs), b.id, ex, ey, r, (int)b.armed, (int)b.parked, nv.occKey & 7, (int)nv.cmdOn, ndDist,
                        g_dbg.dpadCmdPct, nv.occFrames, (int)nv.fired, nv.repeats, b.bodyEnergy, tag);
            }
        };

        // dominant hand (or the whole trunk) glitched this frame -- hold the occupancy / repeat
        // state and emit nothing, rather than running the machine on zeroed coordinates.
        if (!b.handOk)
        {
            g_dbg.swipeAxis = 0; g_dbg.repeatState = nv.repeats > 0 ? 2 : (nv.fired ? 1 : 0);
            trace("nohand");
            return GestureAction::None;
        }

        if (b.parked)
        {
            ResetNav();
            g_dbg.swipeAxis = 0; g_dbg.repeatState = 0; g_dbg.dpadWedge = 0;
            trace("ready");
            return GestureAction::None;
        }

        // ---- command-mode gate: non-dom hand near the non-dom shoulder (sticky) ----
        if (Usable(st(nShJ)))
        {
            int nHJ = Usable(st(nHJ0)) ? nHJ0 : nWJ0;
            if (Usable(st(nHJ)))
                ndDist = Len3(nav.SkeletonPositions[nShJ], nav.SkeletonPositions[nHJ]) / torso;
        }
        nv.cmdOn = nv.cmdOn ? (ndDist < c.dpadCmdGateRadius * c.dpadCmdGateExitK)
                          : (ndDist < c.dpadCmdGateRadius);
        const bool cmd = nv.cmdOn;
        g_dbg.dpadCmd = cmd; g_dbg.dpadWedge = wedge;
        g_dbg.dpadNdDist = ndDist;

        // ---- occupancy tracking (with brief dead-gap grace) ----
        int occ = wedge ? ((cmd ? 8 : 0) | wedge) : 0;
        if (occ == 0)
        {
            if (nv.occKey != 0 && ++nv.grace <= c.dpadWedgeGraceFr) occ = nv.occKey;
            else
            {
                ResetNav();
                g_dbg.swipeAxis = 0; g_dbg.repeatState = 0;
                trace("gap");
                return GestureAction::None;
            }
        }
        else nv.grace = 0;

        if (occ != nv.occKey)
        {
            const bool sameWedge = ((occ & 7) == (nv.occKey & 7) && (nv.occKey & 7) != 0);
            nv.occKey = occ; nv.occFrames = 1; nv.repeats = 0; nv.occStartMs = frameMs;
            nv.fired = nv.suppressed = sameWedge;
        }
        else ++nv.occFrames;

        const int  w     = nv.occKey & 7;
        const bool isCmd = (nv.occKey & 8) != 0;
        const bool isBack = isCmd && (w == 2 || w == 4);          // down/left in command mode -> Esc
        const int  cmdDwell = isBack ? c.dpadBackDwellMs : c.dpadCmdDwellMs;

        g_dbg.dpadCmdPct = 0;
        if (isCmd && !nv.fired && !nv.suppressed && cmdDwell > 0)
        {
            int pct = (int)((frameMs - nv.occStartMs) * 100 / cmdDwell);
            g_dbg.dpadCmdPct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
        }

        if (nv.suppressed) { g_dbg.swipeAxis = (w == 1 || w == 2) ? 1 : 2; g_dbg.repeatState = 0; trace("mode-flip"); return GestureAction::None; }

        auto keyFor = [&]() -> GestureAction {
            if (isCmd)
            {
                const bool pos = (w == 1 || w == 3);           // right / up  -> Confirm
                if (pos)  return c.enableConfirm ? GestureAction::Confirm : GestureAction::None;
                return           c.enableBack    ? GestureAction::Back    : GestureAction::None;
            }
            switch (w) { case 1: return GestureAction::Right; case 2: return GestureAction::Left;
                         case 3: return GestureAction::Up;    default: return GestureAction::Down; }
        };

        GestureAction out = GestureAction::None;
        if (!nv.fired)
        {
            const bool ready = isCmd ? (frameMs - nv.occStartMs >= cmdDwell)
                                     : (nv.occFrames >= c.dpadEntryDebounce);
            if (ready)
            {
                out = keyFor();
                nv.fired = true; nv.firedMs = frameMs;
                nv.nextRepeat = frameMs + (isCmd ? c.dpadCmdRepeatMs : c.dpadRepeatFirstMs);
                if (out != GestureAction::None)
                    LogLine("Gesture[t=%.1f]: dpad %s%s", T(frameMs), DpadName(out), isCmd ? " [cmd]" : "");
            }
        }
        else if (frameMs - nv.firedMs >= c.dpadRepeatDwellMs && frameMs >= nv.nextRepeat)
        {
            out = keyFor();
            ++nv.repeats;
            int iv = c.dpadCmdRepeatMs;
            if (!isCmd)
            {
                iv = c.dpadRepeatFirstMs - (nv.repeats - 1) * c.dpadRepeatAccelMs;
                if (iv < c.dpadRepeatMinMs) iv = c.dpadRepeatMinMs;
            }
            nv.nextRepeat = frameMs + iv;
            if (out != GestureAction::None)
                LogLine("Gesture[t=%.1f]: dpad %s%s (repeat %d)", T(frameMs), DpadName(out), isCmd ? " [cmd]" : "", nv.repeats);
        }

        if (out != GestureAction::None) { g_dbg.lastAction = (unsigned)out; g_dbg.lastActionMs = frameMs; }
        g_dbg.swipeAxis   = (w == 1 || w == 2) ? 1 : 2;
        g_dbg.repeatState = nv.repeats > 0 ? 2 : (nv.fired ? 1 : 0);
        trace(out != GestureAction::None ? "FIRE" : "");
        return out;
    }
}

GestureAction Gestures::UpdateExtend(const NUI_SKELETON_FRAME& f, LONGLONG frameMs)
{
    const Config& c = Cfg::Get();
    if (!c.enable) return GestureAction::None;

    // sensor clock jumped backward (replay --loop wrap, or a re-open) -> start clean.
    if (em.lastFrameMs != 0 && frameMs < em.lastFrameMs - 1000)
    {
        for (auto& b : g_db) b = DpadBody{};
        em = ExtendMulti{};
        g_epoch = -1;
    }
    em.lastFrameMs = frameMs;

    // Body lifetime lives here, not in the recogniser: UpdateExtend is called on every frame,
    // including ones with no tracked skeleton. If every live slot has now been absent for the whole
    // dropout window this is a real body-LOST -- wipe everything. (Reset() is called while the
    // slots are still inUse so it still reports itself.)
    bool anyLive = false, anyFresh = false;
    for (const auto& b : g_db)
        if (b.inUse) { anyLive = true; if (frameMs - b.lastSeenMs <= kDropoutGraceMs) anyFresh = true; }
    if (anyLive && !anyFresh)
    {
        LogLine("Recognizer: body LOST");
        Gestures::Reset();                       // clears g_db / em (incl. hadBodies) / g_epoch
        g_dbg.dpadMode = true; g_dbg.dpadNumBodies = 0; g_dbg.haveHand = false;
        return GestureAction::None;
    }
    // prune individually stale slots (before any DpadSlotFor call) so an eviction can never hit a
    // body that just went briefly absent. Same window.
    for (auto& b : g_db) if (b.inUse && frameMs - b.lastSeenMs > kDropoutGraceMs) b.inUse = false;

    // ---- gather tracked bodies ----
    // A skeleton that is TRACKED but whose SC/HIP glitched out this frame is kept (geomOk=false)
    // with its last good torso, so a brief trunk dropout doesn't evict the driver's slot.
    // A body with no good torso yet (never geomOk) is skipped entirely.
    struct Cand { const NUI_SKELETON_DATA* nav; float torso; DpadBody* b; bool geomOk; };
    Cand cand[NUI_SKELETON_COUNT]; int nc = 0;
    for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
    {
        const NUI_SKELETON_DATA& s = f.SkeletonData[i];
        if (s.eTrackingState != NUI_SKELETON_TRACKED) continue;
        const auto stt = [&](int j) { return s.eSkeletonPositionTrackingState[j]; };
        bool geomOk = Usable(stt(NUI_SKELETON_POSITION_SHOULDER_CENTER)) &&
                      Usable(stt(NUI_SKELETON_POSITION_HIP_CENTER));
        float torsoNow = 0.f;
        if (geomOk)
        {
            torsoNow = Len3(s.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER],
                            s.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER]);
            if (torsoNow < 0.15f) geomOk = false;    // degenerate pose -> not a usable torso
        }
        DpadBody* existing = FindDpadSlot(s.dwTrackingID);
        if (!geomOk && !existing) continue;          // no usable torso and no history -> skip
        DpadBody* b = existing ? existing : DpadSlotFor(s.dwTrackingID);   // only allocate once valid
        b->lastSeenMs = frameMs;
        if (geomOk) b->torso = torsoNow;
        cand[nc].nav = &s; cand[nc].torso = (geomOk ? torsoNow : b->torso);
        cand[nc].b = b; cand[nc].geomOk = geomOk;
        ++nc;
    }

    g_dbg.dpadMode = true;
    g_dbg.dpadNumBodies = nc;

    if (nc == 0)
    {
        // No visible body, but a real loss already returned above -- so we are inside the dropout
        // window. Hold the driver and its clutch; just stop emitting and blank the live readouts.
        PopulateDpadDebug(em.driverId ? FindDpadSlot(em.driverId) : nullptr, c);
        g_dbg.haveHand = false;               // nothing fresh this frame
        return GestureAction::None;
    }

    if (!em.hadBodies) { LogLine("Recognizer: bodies ACQUIRED (n=%d)", nc); em.hadBodies = true; }

    // ---- per-body signal + clutch ----
    for (int i = 0; i < nc; ++i)
        ProcessBody(*cand[i].b, *cand[i].nav, cand[i].torso, frameMs, c, cand[i].geomOk);

    // ---- driver resolution ----
    // Is the current driver still here and still armed?
    int driverIdx = -1;
    for (int i = 0; i < nc; ++i) if (cand[i].b->id == em.driverId) { driverIdx = i; break; }
    DpadBody* driver = (driverIdx >= 0) ? cand[driverIdx].b : nullptr;
    if (driver && !driver->armed) { ResetNav(); driver = nullptr; driverIdx = -1; }

    // Driver not in this frame's candidates but its slot is still alive (kDropoutGraceMs window):
    // hold its seat and emit nothing -- it's a brief full dropout, not a real hand-off.
    if (!driver && em.driverId != 0)
    {
        DpadBody* held = FindDpadSlot(em.driverId);
        if (held && (held->armed || !c.dpadArm))
        {
            PopulateDpadDebug(held, c);
            g_dbg.haveHand = false;           // still ours, just not visible this frame
            return GestureAction::None;
        }
    }
    if (!driver) { em.driverId = 0; em.handoffMs = -1000000; }   // no driver -> any armed body is eligible

    // The challenger: a non-driver body that has been armed since the last hand-off (so a body
    // that finished arming *during* the grace window still takes over the moment it lifts).
    // Latest arm wins the tie. "Old driver must re-park" holds -- a demoted driver's armedAtMs
    // is stale until it re-arms.
    DpadBody* challenger = nullptr;
    for (int i = 0; i < nc; ++i)
    {
        DpadBody* b = cand[i].b;
        if (b->id == em.driverId || !b->armed) continue;
        if (b->armedAtMs <= em.handoffMs) continue;                 // armed before the current driver took over
        if (!challenger || b->armedAtMs >= challenger->armedAtMs) challenger = b;
    }

    // With the clutch off every body is permanently armed, so allow a FIRST driver to be adopted
    // but never a hand-off between two of them (that would flip control every grace period).
    const bool mayTakeOver = c.dpadArm || em.driverId == 0;
    const bool graceOk = (em.driverId == 0) || (frameMs - em.handoffMs >= c.dpadHandoffGraceMs);
    if (challenger && graceOk && mayTakeOver)
    {
        const bool wasHandoff = (em.driverId != 0);
        if (wasHandoff && driverIdx >= 0)           // demote the outgoing driver -> re-park to drive
        {
            DpadBody* o = cand[driverIdx].b;
            o->armed = false; o->parkSinceMs = 0; ResetNav();
            o->mustLeavePark = o->parked;           // demoted with the hand still parked -> must
                                                    // leave the box once before it can re-bid
        }
        em.driverId = challenger->id; em.handoffMs = frameMs;
        ResetNav();
        driver = challenger;
        for (int i = 0; i < nc; ++i) if (cand[i].b == challenger) { driverIdx = i; break; }
        // worth a line for an actual hand-off between people, or whenever 2+ bodies are present;
        // a solo re-arm after an idle-disarm already logs "dpad ARMED id=".
        if (wasHandoff || nc > 1)
            LogLine("Gesture[t=%.1f]: dpad DRIVER -> id=%lu  (nbody=%d)", T(frameMs), challenger->id, nc);
    }
    g_dbg.dpadDriverId = em.driverId;

    // ---- multi-body diagnostic line ----
    if (c.trace && nc > 1 && (em.lastTraceMs < 0 || frameMs - em.lastTraceMs >= 66))
    {
        em.lastTraceMs = frameMs;
        char line[300]; int n = _snprintf_s(line, sizeof(line), _TRUNCATE,
            "dpad/bodies[t=%.1f] nb=%d drv=%lu", T(frameMs), nc, em.driverId);
        for (int i = 0; i < nc && n > 0 && n < (int)sizeof(line) - 56; ++i)
            n += _snprintf_s(line + n, sizeof(line) - n, _TRUNCATE, "  [id=%lu arm=%d park=%d r=%.2f en=%.1f]",
                             cand[i].b->id, (int)cand[i].b->armed, (int)cand[i].b->parked, cand[i].b->r, cand[i].b->bodyEnergy);
        LogLine("%s", line);
    }

    if (!driver || driverIdx < 0)
    {
        // nobody armed: keep the HUD tracking a body's hand so the "ASLEEP" screen still shows
        // the dot moving toward the shoulder as the user goes to wake it.
        PopulateDpadDebug(cand[0].b, c);
        g_dbg.dpadArmed = false;              // nobody holds the wheel, whatever this body's clutch says
        return GestureAction::None;
    }

    return RunNavMachine(*driver, *cand[driverIdx].nav, cand[driverIdx].torso, frameMs, c);
}

void Gestures::GetDebug(GestureDebug& out) { out = g_dbg; }

void Gestures::Reset()
{
    bool wasActive = g.have;
    for (auto& b : g_db) if (b.inUse) wasActive = true;
    g = State{};
    for (auto& b : g_db) b = DpadBody{};
    em = ExtendMulti{};
    g_epoch = -1;                 // log t= restarts on body-loss / replay --loop
    if (wasActive) LogLine("Gesture: reset");
    g_dbg.swipeAxis = 0;
    g_dbg.repeatState = 0;
    g_dbg.confirmHeldMs = g_dbg.backHeldMs = 0;
    g_dbg.dpadDriverId = 0;
    g_dbg.dpadNumBodies = 0;
}

GestureAction Gestures::Update(const NUI_SKELETON_DATA& nav, LONGLONG frameMs)
{
    const Config& c = Cfg::Get();
    if (!c.enable) return GestureAction::None;

    const auto st = [&](int j) { return nav.eSkeletonPositionTrackingState[j]; };

    const Vector4& sc = nav.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER];
    const Vector4& hc = nav.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER];
    if (!Usable(st(NUI_SKELETON_POSITION_SHOULDER_CENTER)) ||
        !Usable(st(NUI_SKELETON_POSITION_HIP_CENTER)))
        return GestureAction::None;
    const float torso = Len3(sc, hc);
    if (torso < 0.15f) return GestureAction::None;

    // extend (air d-pad) is driven by Gestures::UpdateExtend from the recognizer (it needs
    // every tracked body for the multiplayer hand-off). This path is swipe-model only.
    if (c.navModel != 0) return GestureAction::None;

    const int domHand  = c.leftHanded ? NUI_SKELETON_POSITION_HAND_LEFT   : NUI_SKELETON_POSITION_HAND_RIGHT;
    const int domWrist = c.leftHanded ? NUI_SKELETON_POSITION_WRIST_LEFT  : NUI_SKELETON_POSITION_WRIST_RIGHT;
    const int domElbow = c.leftHanded ? NUI_SKELETON_POSITION_ELBOW_LEFT  : NUI_SKELETON_POSITION_ELBOW_RIGHT;
    const int domShldr = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_LEFT : NUI_SKELETON_POSITION_SHOULDER_RIGHT;
    const int ndHand   = c.leftHanded ? NUI_SKELETON_POSITION_HAND_RIGHT  : NUI_SKELETON_POSITION_HAND_LEFT;
    const int ndWrist  = c.leftHanded ? NUI_SKELETON_POSITION_WRIST_RIGHT : NUI_SKELETON_POSITION_WRIST_LEFT;
    const int ndShldr  = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_RIGHT : NUI_SKELETON_POSITION_SHOULDER_LEFT;

    int dj = domHand; if (!Usable(st(dj))) dj = domWrist;
    int nj = ndHand;  if (!Usable(st(nj))) nj = ndWrist;
    const bool domOk = Usable(st(dj));
    const bool ndOk  = Usable(st(nj));

    const Vector4& dh = nav.SkeletonPositions[dj];
    const Vector4& nh = nav.SkeletonPositions[nj];
    const float drx = (dh.x - sc.x) / torso, dry = (dh.y - sc.y) / torso;
    const float nrx = (nh.x - sc.x) / torso, nry = (nh.y - sc.y) / torso;

    const double dt = (g.prevMs >= 0) ? (frameMs - g.prevMs) / 1000.0 : 0.0;
    g.prevMs = frameMs;

    const bool resync = (!g.have || dt <= 0.0 || dt > 0.25);
    Smooth(g.nd,  ndOk,  nrx, nry, dt, c.smoothFast, c.smoothSlow, resync);   // non-dom hand: EMA (Back pose)
    if (resync)
    {
        g.oeX.Configure(c.filter1eMinCutoff, c.filter1eBeta, c.filter1eDCutoff);
        g.oeY.Configure(c.filter1eMinCutoff, c.filter1eBeta, c.filter1eDCutoff);
        g.oeX.Reset(); g.oeY.Reset(); g.sgX.Reset(); g.sgY.Reset();
        g.f1eX = g.f1eY = g.sgVx = g.sgVy = 0.f;
        // a tracking gap (dt > 250 ms, same body -> no Gestures::Reset) must not leave a
        // swipe / dwell mid-flight across the positional discontinuity
        g.swActive = false;
        g.rs = RS_IDLE;
        g.confirmSince = g.backSince = -1;
        g_dbg.swipeAxis = 0; g_dbg.repeatState = 0;
        g_dbg.confirmHeldMs = g_dbg.backHeldMs = 0;
        g.have = true; g.frames = 1;
        return GestureAction::None;
    }

    // ---- signal conditioning: dominant hand -> 1 Euro position + Savitzky-Golay velocity
    if (domOk)
    {
        g.f1eX = g.oeX.Filter(drx, (float)dt);
        g.f1eY = g.oeY.Filter(dry, (float)dt);
        g.sgX.Push(g.f1eX, (float)dt, g.sgVx);
        g.sgY.Push(g.f1eY, (float)dt, g.sgVy);
    }

    const float m       = c.mirror ? -1.0f : 1.0f;
    const float ex      = m * g.f1eX;        // + = user's right (1 Euro filtered position)
    const float ey      = g.f1eY;            // + = up
    const float evx     = m * g.sgVx;        // Savitzky-Golay velocity
    const float evy     = g.sgVy;

    // arm extension fraction + how level the arm is (for L/R hold-to-repeat)
    float armExt = 0.f;
    float armLevel = 9.9f;   // |dy| / |dx| of hand-vs-dom-shoulder: 0 = horizontal, big = vertical
    float handOverElbow = 0.f;   // (hand.y - elbow.y)/torso, +ve = hand above the elbow
    bool  haveElbowY = false;    // (elbow tracks even when the wrist jitters -- Kinect v1)
    if (domOk && Usable(st(domShldr)))
    {
        const Vector4& sh = nav.SkeletonPositions[domShldr];
        const float adx = fabsf(dh.x - sh.x);
        const float ady = fabsf(dh.y - sh.y);
        armLevel = (adx > 0.02f) ? ady / adx : 9.9f;
        if (Usable(st(domElbow)))
        {
            const Vector4& el = nav.SkeletonPositions[domElbow];
            const float contour = Len3(sh, el) + Len3(el, dh);
            if (contour > 0.05f) armExt = Len3(sh, dh) / contour;
            handOverElbow = (dh.y - el.y) / torso;
            haveElbowY = true;
        }
    }
    const bool armLevelOk = armLevel <= c.armLevelTanMax;

    g_dbg.haveHand = domOk;
    g_dbg.dpadMode = false;
    g_dbg.domEx = ex; g_dbg.domEy = ey; g_dbg.domVx = evx; g_dbg.domVy = evy;
    g_dbg.f1eEx = m * g.f1eX; g_dbg.f1eEy = g.f1eY;
    g_dbg.sgVx  = m * g.sgVx; g_dbg.sgVy  = g.sgVy;
    g_dbg.armExtend = armExt;
    g_dbg.armLevel = armLevel;
    g_dbg.armLevelOk = armLevelOk;
    g_dbg.confirmNeedMs = c.confirmDwellMs;
    g_dbg.backNeedMs = c.backDwellMs;

    auto emit = [&](GestureAction a) {
        if (a != GestureAction::None) { g_dbg.lastAction = (unsigned)a; g_dbg.lastActionMs = frameMs; }
        return a;
    };

    ++g.frames;
    if (g.frames < c.armAfterFrames) return GestureAction::None;

    // ---- gameplay mute -------------------------------------------------
    const LONGLONG nowT = (LONGLONG)GetTickCount64();
    const bool inGameplay = c.suppressInGame && g_songLoadTick != 0
        && g_gameWndActive != 0                       // game must be up front + full-sized
        && nowT - g_songLoadTick  < c.gameplayCapMs
        && nowT - g_lastOpenTick  > c.fileIdleGameplayMs;
    if (g.wasMuted != inGameplay)
        LogLine("Gesture[t=%.1f]: gameplay-mute %s  (lastOpen %lldms ago, songArm %lldms ago, wact=%ld)",
                T(frameMs), inGameplay ? "ON" : "off",
                nowT - g_lastOpenTick, g_songLoadTick ? (nowT - g_songLoadTick) : -1, g_gameWndActive);
    // once we leave a song (file activity resumed, or the window went away), forget
    // the load marker -- a menu going quiet must NOT re-mute; only a fresh burst can.
    if (g.wasMuted && !inGameplay)
        InterlockedExchange64((volatile LONGLONG*)&g_songLoadTick, 0);
    g.wasMuted = inGameplay;
    g_dbg.inGameplay = inGameplay;
    if (inGameplay)
    {
        g.swActive = false; g.rs = RS_IDLE;
        g.confirmSince = g.backSince = -1;
        g_dbg.swipeAxis = 0; g_dbg.repeatState = 0;
        g_dbg.confirmHeldMs = g_dbg.backHeldMs = 0;
        return GestureAction::None;
    }

    const bool cooling = frameMs < g.gestureCooldownMs;

    // ---- Back: non-dominant arm down-and-out to the side (checked FIRST, and it
    //      hard-disarms Confirm while its pose is held -- the dominant hand often
    //      drifts up when you raise the other arm, which was firing Confirm instead
    //      of Back). Auto-repeats while held. ----
    bool backPoseNow = false;
    if (c.enableBack && ndOk && Usable(st(ndShldr)))
    {
        const Vector4& ns = nav.SkeletonPositions[ndShldr];
        float dx = nh.x - ns.x, dy = nh.y - ns.y, dz = nh.z - ns.z;
        const float L = sqrtf(dx * dx + dy * dy + dz * dz);
        float outFrac = 0.f, downFrac = 0.f;
        if (L > 0.05f)
        {
            const float outSign = c.leftHanded ? +1.0f : -1.0f;   // non-dom points away from body
            outFrac  = (outSign * m * dx) / L;
            downFrac = (-dy) / L;
        }
        const float ndSpeed = sqrtf(g.nd.vx * g.nd.vx + g.nd.vy * g.nd.vy);
        backPoseNow = outFrac > c.backOutMin
                      && downFrac > c.backDownMin && downFrac < c.backDownMax
                      && ndSpeed < c.backStillVel;
        if (backPoseNow && !cooling)
        {
            if (g.backSince < 0) g.backSince = frameMs;
            g.backGrace = 0;   // reset every good frame -> tolerate `grace` CONSECUTIVE bad ones
            g_dbg.backHeldMs = (int)(frameMs - g.backSince);

            const bool first  = g.backArmed && frameMs - g.backSince >= c.backDwellMs;
            const bool repeat = !g.backArmed && c.backRepeatMs > 0 && frameMs >= g.backRepeatNext;
            if (first || repeat)
            {
                g.backArmed = false;
                g.backSince = frameMs;
                g.backRepeatNext = frameMs + (c.backRepeatMs > 0
                    ? (first ? c.backRepeatFirstMs : c.backRepeatMs) : (1 << 30));
                g.gestureCooldownMs = frameMs + 500;
                g_dbg.backHeldMs = 0;
                LogLine("Gesture[t=%.1f]: FIRE Back%s", T(frameMs), repeat ? " (repeat)" : "");
                return emit(GestureAction::Back);
            }
        }
        else if (!backPoseNow && g.backSince >= 0)
        {
            if (++g.backGrace > c.dwellGraceFrames)
            { g.backSince = -1; g_dbg.backHeldMs = 0; g.backArmed = true; g.backRepeatNext = 0; }
        }
        if (outFrac < 0.15f) { g.backArmed = true; g.backRepeatNext = 0; }
    }
    else { g.backSince = -1; g_dbg.backHeldMs = 0; }

    // ---- Confirm: dominant hand raised + still (auto-repeats while held) ----
    if (c.enableConfirm)
    {
        const bool pose = domOk && ey > c.confirmRaiseFy
                          && fabsf(evx) < c.confirmStillVel && fabsf(evy) < c.confirmStillVel
                          && !g.swActive && g.rs == RS_IDLE
                          && !backPoseNow && g.backSince < 0;   // Back wins while it's being posed
        if (pose && !cooling)
        {
            if (g.confirmSince < 0) g.confirmSince = frameMs;
            g.confirmGrace = 0;   // reset every good frame -> tolerate `grace` CONSECUTIVE bad ones
            g_dbg.confirmHeldMs = (int)(frameMs - g.confirmSince);

            const bool first  = g.confirmArmed && frameMs - g.confirmSince >= c.confirmDwellMs;
            const bool repeat = !g.confirmArmed && c.confirmRepeatMs > 0 && frameMs >= g.confirmRepeatNext;
            if (first || repeat)
            {
                g.confirmArmed = false;
                g.confirmSince = frameMs;   // restart the held-ms readout for the next repeat
                g.confirmRepeatNext = frameMs + (c.confirmRepeatMs > 0
                    ? (first ? c.confirmRepeatFirstMs : c.confirmRepeatMs) : (1 << 30));
                g.gestureCooldownMs = frameMs + 500;
                g_dbg.confirmHeldMs = 0;
                LogLine("Gesture[t=%.1f]: FIRE Confirm%s", T(frameMs), repeat ? " (repeat)" : "");
                return emit(GestureAction::Confirm);
            }
        }
        else if (!pose && g.confirmSince >= 0)
        {
            if (++g.confirmGrace > c.dwellGraceFrames)
            { g.confirmSince = -1; g_dbg.confirmHeldMs = 0; g.confirmArmed = true; g.confirmRepeatNext = 0; }
        }
        if ((domOk && ey < c.confirmRaiseFy - 0.10f) || backPoseNow)
        { g.confirmArmed = true; g.confirmRepeatNext = 0; }
    }
    else g.confirmSince = -1;

    // ---- Left/Right hold-to-repeat --------------------------------
    if (g.rs == RS_POST)
    {
        if (armExt > c.armExtendFrac && armLevelOk)
        {
            g.rs = RS_HOLD;
            g.rsRepeats = 1;
            g.rsRelaxFrames = 0;
            g.rsNextRepeat = frameMs + c.repeatFirstMs;
            LogLine("Gesture[t=%.1f]: repeat HOLD %s (arm out %.2f lvl %.2f)", T(frameMs), g.rsDir > 0 ? "R" : "L", armExt, armLevel);
        }
        else if (frameMs > g.rsPostUntil)
        {
            g.rs = RS_IDLE;
        }
    }
    if (g.rs == RS_HOLD)
    {
        // stop repeating if the arm bends OR swings off horizontal (held high / hanging low).
        // exit uses a wider angle than entry so it doesn't chatter at the boundary.
        if (armExt < c.armRelaxFrac || armLevel > c.armLevelTanMax * 1.4f)
        {
            if (++g.rsRelaxFrames >= 4) { g.rs = RS_IDLE; LogLine("Gesture[t=%.1f]: repeat end (%d, ext %.2f lvl %.2f)", T(frameMs), g.rsRepeats, armExt, armLevel); }
        }
        else g.rsRelaxFrames = 0;

        if (g.rs == RS_HOLD && frameMs >= g.rsNextRepeat)
        {
            ++g.rsRepeats;
            int iv = c.repeatFirstMs - (g.rsRepeats - 1) * c.repeatAccelMs;
            if (iv < c.repeatMinMs) iv = c.repeatMinMs;
            g.rsNextRepeat = frameMs + iv;
            g_dbg.repeatState = 2;
            return emit(g.rsDir > 0 ? GestureAction::Right : GestureAction::Left);
        }
    }
    g_dbg.repeatState = g.rs;

    // ---- swipe detection --------------------------------------------
    // The "settle" gate -- hand must come to rest before a new swipe may start --
    // is enforced ONLY for swipeSettleAfterMs after a swipe fires. That's the only
    // time it earns its keep: stopping a return / recovery stroke or a perpendicular
    // flick from firing a second swipe. Outside that window there is no settle
    // requirement, so a cold swipe from rest always works (an inferred hand's
    // velocity noise -- +-2..7 torso/s on a genuinely-held hand in a cluttered
    // room -- made the old always-on gate unreachable). The counter LEAKS rather
    // than hard-resetting so jitter spikes during a real hold don't zero it.
    // Also blocked outright during the whole L/R post-swipe + repeat sequence.
    // (Evaluate the count from the previous frame, THEN update it, so the first
    // fast frame of a swipe still counts.)
    const bool settleWindow = frameMs < g.swSettleUntil;
    const bool swipeBlocked = cooling || g.rs != RS_IDLE || frameMs < g.swCooldownMs
        || (settleWindow && g.swSettle < c.swipeSettleFrames);
    const float settleSpeed = c.swipeSettleFrac * c.swipeVelocity;
    if (fabsf(evx) < settleSpeed && fabsf(evy) < settleSpeed)
    {
        if (g.swSettle < 999) ++g.swSettle;
    }
    else if (g.swSettle > 0)
    {
        g.swSettle -= 2;
        if (g.swSettle < 0) g.swSettle = 0;
    }

    if (!g.swActive && !swipeBlocked && domOk)
    {
        const bool horiz = fabsf(evx) > c.swipeVelocity && fabsf(evx) < c.swipeVxCeiling
                           && fabsf(evx) > c.swipeAxisRatio * fabsf(evy);
        const bool vert  = fabsf(evy) > c.swipeVelocity && fabsf(evy) < c.swipeVxCeiling
                           && fabsf(evy) > c.swipeAxisRatio * fabsf(evx)
                           && fabsf(ex) < c.swipeVertXBand           // hand in front, not recovering to the side
                           && ey > c.swipeYMin && ey < c.swipeYMax;  // and in the play zone
        if (horiz || vert)
        {
            g.swActive = true;
            g.swAxis   = horiz ? 1 : 2;
            const float v0 = horiz ? evx : evy;
            g.swSegDir   = v0 > 0 ? 1 : -1;
            g.swSegStart = g.swSegExt = horiz ? ex : ey;
            g.swFrames = g.swSegFrames = 1;
            g.swSegs   = 1;
            g.swSegPeak = fabsf(v0);
            g.swZoneRejLogged = false;
        }
    }
    else if (g.swActive)
    {
        ++g.swFrames;
        const float pos = (g.swAxis == 1) ? ex : ey;
        const float vel = (g.swAxis == 1) ? evx : evy;
        const float revVel = 0.35f * c.swipeVelocity;   // deadband for calling a direction change

        // Segment the motion at reversals. A counter-flick ("countersteer") then the
        // real stroke are separate segments; we always judge only the CURRENT segment,
        // measured from where the hand last turned around (swSegExt). So an opening
        // opposite flick is absorbed whatever its size/speed -- it's just segment 1,
        // which won't have travelled far in the fired direction.
        const int idir = (vel > revVel) ? 1 : (vel < -revVel) ? -1 : 0;
        if (idir != 0 && idir != g.swSegDir)
        {
            g.swSegStart  = g.swSegExt;   // new stroke starts at the turnaround point
            g.swSegDir    = idir;
            g.swSegExt    = pos;
            g.swSegFrames = 1;
            g.swSegPeak   = fabsf(vel);
            ++g.swSegs;
        }
        else
        {
            ++g.swSegFrames;
            if (fabsf(vel) > g.swSegPeak) g.swSegPeak = fabsf(vel);
            if (g.swSegDir > 0 ? (pos > g.swSegExt) : (pos < g.swSegExt)) g.swSegExt = pos;
        }

        const float progress = g.swSegDir * (pos - g.swSegStart);
        const float dirVel   = g.swSegDir * vel;
        const bool  timedOut = g.swFrames > c.swipeMaxActiveFrames;

        // Stage 2 positional invariant: the stroke must END in the command's target zone.
        // A recovery / return-to-park stroke ends near neutral -> rejected here, no latency,
        // no state. (`ex`/`ey` here are the current, i.e. end-of-stroke, hand position.)
        bool zoneOk;
        if (g.swAxis == 1)
            zoneOk = (g.swSegDir > 0) ? (ex >= c.rightEndMinX) : (ex <= c.leftEndMaxX);
        else if (g.swSegDir > 0)          // UP: hand ends high, and above the elbow (real raise,
            zoneOk = ey >= c.upEndMinY    //     not a recovery drag where the hand trails the elbow)
                     && (!haveElbowY || c.upElbowMargin > 8.f
                         || handOverElbow >= -c.upElbowMargin);
        else                             // DOWN: the segment started at/above neutral, ends low
            zoneOk = g.swSegStart >= c.downStartMinY && ey <= c.downEndMaxY;

        // commit the swipe: cooldowns, per-axis follow-ups, log, return the action
        auto fireSwipe = [&]() -> GestureAction {
            g.swActive = false; g.swSettle = 0;
            g.swCooldownMs  = frameMs + c.swipeCooldownMs;
            g.swSettleUntil = frameMs + c.swipeSettleAfterMs;
            GestureAction a;
            if (g.swAxis == 1)
            {
                a = g.swSegDir > 0 ? GestureAction::Right : GestureAction::Left;
                g.rs = RS_POST; g.rsDir = g.swSegDir; g.rsPostUntil = frameMs + c.postSwipeMs;
            }
            else
                a = g.swSegDir > 0 ? GestureAction::Up : GestureAction::Down;
            LogLine("Gesture[t=%.1f]: swipe %s  prog=%.2f f=%d seg=%d",
                    T(frameMs), a == GestureAction::Left ? "LEFT" : a == GestureAction::Right ? "RIGHT" :
                                a == GestureAction::Up ? "UP" : "DOWN", progress, g.swSegFrames, g.swSegs);
            return a;
        };

        if (timedOut || g.swSegs > c.swipeMaxSegments
            || (g.swAxis == 2 && (ey < c.swipeYMin - 0.15f || ey > c.swipeYMax + 0.15f
                                  || fabsf(ex) > c.swipeVertXBand + 0.20f)))
        {
            g.swActive = false;   // too long, too much fiddling, or left the play zone
        }
        else if (g.swSegFrames >= c.swipeMinFrames && progress >= c.swipeDistance && dirVel > 0.0f
                 && zoneOk                                             // Stage 2: ended in the target zone
                 && g.swSegPeak >= c.swipeVelocity * c.swipePeakRatio  // ballistic: a real stroke has a
                                                                      // velocity peak; a slow drift does not
                 && (g.swAxis == 1 ||
                     (ey > c.swipeYMin && ey < c.swipeYMax && fabsf(ex) < c.swipeVertXBand)))
        {
            return emit(fireSwipe());
        }
        else if (!g.swZoneRejLogged && g.swSegFrames >= c.swipeMinFrames
                 && progress >= c.swipeDistance && dirVel > 0.0f && !zoneOk
                 && g.swSegPeak >= c.swipeVelocity * c.swipePeakRatio)
        {
            // a real stroke (distance + direction + ballistic peak) that ended outside the
            // command's target zone -- log once so the *EndY / *EndX can be tuned in-game
            g.swZoneRejLogged = true;
            LogLine("Gesture[t=%.1f]: swipe %s rejected -- ended out of zone (ex=%+.2f ey=%+.2f seg0=%+.2f)",
                    T(frameMs),
                    g.swAxis == 1 ? (g.swSegDir > 0 ? "RIGHT" : "LEFT")
                                  : (g.swSegDir > 0 ? "UP" : "DOWN"),
                    ex, ey, g.swSegStart);
        }
        else if (g.swSegs == 1 && g.swSegFrames > c.swipeMinFrames + 3
                 && progress < c.swipeDistance && fabsf(vel) < revVel)
        {
            g.swActive = false;   // first segment stalled out without ever reversing -> not a swipe
        }
    }
    g_dbg.swipeAxis = g.swActive ? g.swAxis : 0;

    if (c.trace && (g.lastTraceMs < 0 || frameMs - g.lastTraceMs >= 66))
    {
        g.lastTraceMs = frameMs;
        LogLine("trace[t=%.1f] ex=%+.2f ey=%+.2f evx=%+.2f evy=%+.2f ext=%.2f lvl=%.2f hoe=%+.2f sw=%d sd=%+d seg=%d pk=%.1f rs=%d stl=%d blk=%d swin=%d cf=%d bk=%d%s",
                T(frameMs), ex, ey, evx, evy, armExt, armLevel, haveElbowY ? handOverElbow : 9.99f,
                g.swActive ? g.swAxis : 0, g.swActive ? g.swSegDir : 0,
                g.swActive ? g.swSegs : 0, g.swActive ? g.swSegPeak : 0.f, g.rs,
                g.swSettle, (int)swipeBlocked, (int)settleWindow,
                g_dbg.confirmHeldMs, g_dbg.backHeldMs,
                inGameplay ? " GAME" : "");
    }

    return GestureAction::None;
}
