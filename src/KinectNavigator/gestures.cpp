#include "framework.h"
#include <math.h>
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
    struct ExtendState
    {
        bool     have    = false;
        LONGLONG prevMs  = -1;
        int      frames  = 0;
        OneEuro  oeX, oeY;               // 1 Euro on the dominant hand position (rel dom shoulder)
        bool     parked  = true;         // hand in the park box (sticky / hysteresis)
        int      occKey  = 0;            // occupancy id: bit3 = command mode, bits0-2 = wedge (1..4)
        int      occFrames = 0;          // frames in the current occupancy
        int      grace   = 0;            // frames of jitter tolerated in a dead gap
        bool     fired   = false;        // single press already emitted for this occupancy
        bool     suppressed = false;     // mode flipped mid-wedge -> emit nothing until the wedge is left
        bool     cmdOn   = false;        // command-mode gate (sticky / hysteresis)
        LONGLONG occStartMs = 0;         // frameMs the current occupancy began (command dwell)
        bool     armed   = false;        // clutch: only responds after the hand parks at the shoulder
        LONGLONG parkSinceMs = 0;        // arming dwell
        LONGLONG restSinceMs = 0;        // idle-out-of-play timer for auto-disarm
        LONGLONG firedMs = 0;
        LONGLONG nextRepeat = 0;
        int      repeats = 0;
        LONGLONG lastTraceMs = -1;
    } eg;

    const char* DpadName(GestureAction a)
    {
        switch (a) {
        case GestureAction::Left:  return "LEFT";   case GestureAction::Right: return "RIGHT";
        case GestureAction::Up:    return "UP";     case GestureAction::Down:  return "DOWN";
        case GestureAction::Confirm: return "CONFIRM"; case GestureAction::Back: return "BACK";
        default: return "?";
        }
    }

    GestureAction ExtendUpdate(const NUI_SKELETON_DATA& nav, LONGLONG frameMs, const Config& c, float torso)
    {
        const auto st = [&](int j) { return nav.eSkeletonPositionTrackingState[j]; };
        const int shJ  = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_LEFT  : NUI_SKELETON_POSITION_SHOULDER_RIGHT;
        const int hJ0  = c.leftHanded ? NUI_SKELETON_POSITION_HAND_LEFT      : NUI_SKELETON_POSITION_HAND_RIGHT;
        const int wJ0  = c.leftHanded ? NUI_SKELETON_POSITION_WRIST_LEFT     : NUI_SKELETON_POSITION_WRIST_RIGHT;
        const int nShJ = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_RIGHT : NUI_SKELETON_POSITION_SHOULDER_LEFT;
        const int nHJ0 = c.leftHanded ? NUI_SKELETON_POSITION_HAND_RIGHT     : NUI_SKELETON_POSITION_HAND_LEFT;
        const int nWJ0 = c.leftHanded ? NUI_SKELETON_POSITION_WRIST_RIGHT    : NUI_SKELETON_POSITION_WRIST_LEFT;

        const double dt = (eg.prevMs >= 0) ? (frameMs - eg.prevMs) / 1000.0 : 0.0;
        eg.prevMs = frameMs;
        const bool resync = (!eg.have || dt <= 0.0 || dt > 0.25);

        int hJ = Usable(st(hJ0)) ? hJ0 : wJ0;
        const bool anchorOk = Usable(st(shJ));
        const bool handOk   = anchorOk && Usable(st(hJ));

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
            eg.oeX.Configure(c.filter1eMinCutoff, c.filter1eBeta, c.filter1eDCutoff);
            eg.oeY.Configure(c.filter1eMinCutoff, c.filter1eBeta, c.filter1eDCutoff);
            eg.oeX.Reset(); eg.oeY.Reset();
            eg.parked = true; eg.occKey = 0; eg.fired = false; eg.repeats = 0; eg.grace = 0;
            eg.cmdOn = false; eg.armed = false; eg.parkSinceMs = 0; eg.restSinceMs = 0;
            eg.have = true; eg.frames = 1;
            g_dbg.haveHand = handOk; g_dbg.swipeAxis = 0; g_dbg.repeatState = 0;
            g_dbg.armExtend = 0; g_dbg.confirmHeldMs = g_dbg.backHeldMs = 0; g_dbg.inGameplay = false;
            return GestureAction::None;
        }

        const float ex = handOk ? eg.oeX.Filter(rawx, (float)dt) : 0.f;
        const float ey = handOk ? eg.oeY.Filter(rawy, (float)dt) : 0.f;
        const float r  = sqrtf(ex * ex + ey * ey);

        ++eg.frames;
        g_dbg.haveHand   = handOk;
        g_dbg.dpadMode   = true;
        g_dbg.domEx = ex; g_dbg.domEy = ey; g_dbg.domVx = 0; g_dbg.domVy = 0;
        g_dbg.armExtend = r;                       // overlay: shoulder->hand distance (torso units)
        g_dbg.confirmHeldMs = g_dbg.backHeldMs = 0;
        g_dbg.inGameplay = false;
        g_dbg.dpadParked = eg.parked;
        g_dbg.dpadArmed = eg.armed;
        g_dbg.dpadCmd = false; g_dbg.dpadWedge = 0;
        g_dbg.dpadParkR = c.dpadParkRadius;

        float ndDist = 9.9f;   // non-dom hand -> non-dom shoulder (torso); filled in the cmd-gate block below

        auto trace = [&](const char* tag) {
            if (c.trace && (eg.lastTraceMs < 0 || frameMs - eg.lastTraceMs >= 66)) {
                eg.lastTraceMs = frameMs;
                LogLine("dpad[t=%.1f] ex=%+.2f ey=%+.2f r=%.2f arm=%d park=%d w=%d cmd=%d nd=%.2f cdw=%d occf=%d fired=%d rep=%d %s",
                        T(frameMs), ex, ey, r, (int)eg.armed, (int)eg.parked, eg.occKey & 7, (int)eg.cmdOn, ndDist,
                        g_dbg.dpadCmdPct, eg.occFrames, (int)eg.fired, eg.repeats, tag);
            }
        };

        if (eg.frames < c.armAfterFrames || !handOk)
        {
            g_dbg.swipeAxis = 0; g_dbg.repeatState = 0;
            trace("warmup");
            return GestureAction::None;
        }

        // ---- park box (asymmetric hysteresis) ----
        if (eg.parked) { if (r > c.dpadParkRadius * c.dpadParkExitK) eg.parked = false; }
        else           { if (r < c.dpadParkRadius)                   eg.parked = true;  }
        g_dbg.dpadParked = eg.parked;

        // ---- which wedge? (component tests; gaps between wedges + straight-down are dead) ----
        const float ax = fabsf(ex), ay = fabsf(ey);
        const float tw = tanf(c.dpadWedgeHalfDeg * 0.01745329f);
        int wedge = 0;
        if      (ex > 0 && ay <= ax * tw) wedge = 1;   // RIGHT (outward on the dominant side)
        else if (ex < 0 && ay <= ax * tw) wedge = 2;   // LEFT  (across the body)
        else if (ey > 0 && ax <= ay * tw) wedge = 3;   // UP
        else if (ey < -c.dpadDownMinDrop && ax >= c.dpadDownMinOut && ax <= ay) wedge = 4;  // DOWN-and-out

        // ---- clutch: park to arm, idle-out-of-play to disarm ----
        if (!c.dpadArm) eg.armed = true;
        else if (eg.parked)
        {
            if (eg.parkSinceMs == 0) eg.parkSinceMs = frameMs;
            eg.restSinceMs = 0;
            if (!eg.armed && frameMs - eg.parkSinceMs >= c.dpadArmDwellMs)
            {
                eg.armed = true;
                LogLine("Gesture[t=%.1f]: dpad ARMED", T(frameMs));
            }
        }
        else
        {
            eg.parkSinceMs = 0;
            if (wedge == 0)   // not parked, not in a wedge -> "not interacting"
            {
                if (eg.restSinceMs == 0) eg.restSinceMs = frameMs;
                if (eg.armed && frameMs - eg.restSinceMs >= c.dpadDisarmMs)
                {
                    eg.armed = false; eg.occKey = 0; eg.fired = false; eg.suppressed = false; eg.repeats = 0;
                    LogLine("Gesture[t=%.1f]: dpad disarmed (idle %dms)", T(frameMs), c.dpadDisarmMs);
                }
            }
            else eg.restSinceMs = 0;
        }
        g_dbg.dpadArmed = eg.armed;

        if (!eg.armed)
        {
            eg.occKey = 0; eg.fired = false; eg.suppressed = false; eg.repeats = 0; eg.grace = 0;
            g_dbg.swipeAxis = 0; g_dbg.repeatState = 0; g_dbg.dpadWedge = 0; g_dbg.dpadCmdPct = 0;
            trace("asleep");
            return GestureAction::None;
        }
        if (eg.parked)
        {
            eg.occKey = 0; eg.fired = false; eg.suppressed = false; eg.repeats = 0; eg.grace = 0;
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
        eg.cmdOn = eg.cmdOn ? (ndDist < c.dpadCmdGateRadius * c.dpadCmdGateExitK)
                            : (ndDist < c.dpadCmdGateRadius);
        const bool cmd = eg.cmdOn;
        g_dbg.dpadCmd = cmd; g_dbg.dpadWedge = wedge;
        g_dbg.dpadNdDist = ndDist; g_dbg.dpadCmdGateR = c.dpadCmdGateRadius;

        // ---- occupancy tracking (with brief dead-gap grace) ----
        int occ = wedge ? ((cmd ? 8 : 0) | wedge) : 0;
        if (occ == 0)
        {
            if (eg.occKey != 0 && ++eg.grace <= c.dpadWedgeGraceFr) occ = eg.occKey;  // ignore a jitter blip
            else
            {
                eg.occKey = 0; eg.fired = false; eg.suppressed = false; eg.repeats = 0;
                g_dbg.swipeAxis = 0; g_dbg.repeatState = 0;
                trace("gap");
                return GestureAction::None;
            }
        }
        else eg.grace = 0;

        if (occ != eg.occKey)
        {
            const bool sameWedge = ((occ & 7) == (eg.occKey & 7) && (eg.occKey & 7) != 0);
            eg.occKey = occ; eg.occFrames = 1; eg.repeats = 0; eg.occStartMs = frameMs;
            // wedge slide (R->U etc.) or fresh entry from park -> fire normally.
            // mode flip while the SAME wedge is held (left hand on/off shoulder mid-hold)
            // -> emit nothing until the wedge is actually left.
            eg.fired = eg.suppressed = sameWedge;
        }
        else ++eg.occFrames;

        const int  w     = eg.occKey & 7;
        const bool isCmd = (eg.occKey & 8) != 0;
        const bool isBack = isCmd && (w == 2 || w == 4);          // down/left in command mode -> Esc
        const int  cmdDwell = isBack ? c.dpadBackDwellMs : c.dpadCmdDwellMs;

        // command-mode dwell progress (overlay)
        g_dbg.dpadCmdPct = 0;
        if (isCmd && !eg.fired && !eg.suppressed && cmdDwell > 0)
        {
            int pct = (int)((frameMs - eg.occStartMs) * 100 / cmdDwell);
            g_dbg.dpadCmdPct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
        }

        if (eg.suppressed) { g_dbg.swipeAxis = (w == 1 || w == 2) ? 1 : 2; g_dbg.repeatState = 0; trace("mode-flip"); return GestureAction::None; }

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
        if (!eg.fired)
        {
            const bool ready = isCmd ? (frameMs - eg.occStartMs >= cmdDwell)   // deliberate hold (Esc = 3 s)
                                     : (eg.occFrames >= c.dpadEntryDebounce);  // nav stays snappy
            if (ready)
            {
                out = keyFor();
                eg.fired = true; eg.firedMs = frameMs;
                eg.nextRepeat = frameMs + (isCmd ? c.dpadCmdRepeatMs : c.dpadRepeatFirstMs);
                if (out != GestureAction::None)
                    LogLine("Gesture[t=%.1f]: dpad %s%s", T(frameMs), DpadName(out), isCmd ? " [cmd]" : "");
            }
        }
        else if (frameMs - eg.firedMs >= c.dpadRepeatDwellMs && frameMs >= eg.nextRepeat)
        {
            out = keyFor();
            ++eg.repeats;
            int iv = c.dpadCmdRepeatMs;
            if (!isCmd)
            {
                iv = c.dpadRepeatFirstMs - (eg.repeats - 1) * c.dpadRepeatAccelMs;
                if (iv < c.dpadRepeatMinMs) iv = c.dpadRepeatMinMs;
            }
            eg.nextRepeat = frameMs + iv;
            if (out != GestureAction::None)
                LogLine("Gesture[t=%.1f]: dpad %s%s (repeat %d)", T(frameMs), DpadName(out), isCmd ? " [cmd]" : "", eg.repeats);
        }

        if (out != GestureAction::None) { g_dbg.lastAction = (unsigned)out; g_dbg.lastActionMs = frameMs; }
        g_dbg.swipeAxis   = (w == 1 || w == 2) ? 1 : 2;
        g_dbg.repeatState = eg.repeats > 0 ? 2 : (eg.fired ? 1 : 0);
        trace(out != GestureAction::None ? "FIRE" : "");
        return out;
    }
}

void Gestures::GetDebug(GestureDebug& out) { out = g_dbg; }

void Gestures::Reset()
{
    const bool wasActive = g.have || eg.have;
    g = State{};
    eg = ExtendState{};
    g_epoch = -1;                 // log t= restarts on body-loss / navigator change / replay --loop
    if (wasActive) LogLine("Gesture: reset");
    g_dbg.swipeAxis = 0;
    g_dbg.repeatState = 0;
    g_dbg.confirmHeldMs = g_dbg.backHeldMs = 0;
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

    if (c.navModel != 0)                          // 1 = extend (air d-pad); 0 = swipe (below)
        return ExtendUpdate(nav, frameMs, c, torso);

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
