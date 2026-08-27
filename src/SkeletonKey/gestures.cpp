#include "framework.h"
#include <math.h>
#include "gestures.h"
#include "config.h"
#include "log.h"

// M3: horizontal right-hand swipes -> VK_RIGHT / VK_LEFT.
//   quick swipe                         = one press
//   swipe, then leave the hand out to that side (any height) = auto-repeat,
//                                         accelerating, until the hand comes back
//   a wave (hand crossing back toward centre) re-arms -- no arms-at-sides
//   "neutral" needed between gestures. Full neutral is only the FIRST arm and
//   after the sensor loses the body.
//
// All positions are body-relative to the SHOULDER_CENTRE / HIP_CENTRE anchor,
// in torso lengths. "efx" is the fast-smoothed signed distance the hand is to
// the user's RIGHT (mirror flips it); "esx" is the slow-smoothed version.

namespace
{
    enum class SM { InitialArm, Armed, PostSwipe, Holding, ReArm };

    const char* SmTag(SM s)
    {
        switch (s) {
        case SM::InitialArm: return "INIT";
        case SM::Armed:      return "ARM";
        case SM::PostSwipe:  return "PSW";
        case SM::Holding:    return "HLD";
        case SM::ReArm:      return "RARM";
        }
        return "?";
    }

    struct State
    {
        bool     have   = false;
        LONGLONG prevMs = -1;
        int      frames = 0;

        float    fx = 0, fy = 0;
        float    sx = 0, sy = 0;
        float    fxPrev = 0, fyPrev = 0;

        SM       sm = SM::InitialArm;
        LONGLONG neutralSinceMs = -1;
        LONGLONG stateMs = 0;
        LONGLONG nextRepeatMs = 0;
        int      repeats = 0;
        int      holdMiss = 0;

        bool     swiping = false;
        int      swipeDir = 0;          // +1 right, -1 left (effective space)
        float    swipeStartEfx = 0;
        int      swipeFrames = 0;
        int      cand = 0;              // candidate direction, pending confirmation
        int      candFrames = 0;

        int      holdDir = 0;           // direction being repeated

        LONGLONG lastTraceMs = -1;
    } g;

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
    inline void Enter(SM s, LONGLONG now) { g.sm = s; g.stateMs = now; }
    inline GestureAction DirAction(int dir) { return dir > 0 ? GestureAction::Right : GestureAction::Left; }
}

void Gestures::Reset()
{
    const bool wasActive = g.have;
    g = State{};
    if (wasActive) LogLine("Gesture: reset");
}

GestureAction Gestures::Update(const NUI_SKELETON_DATA& nav, LONGLONG frameMs)
{
    const Config& c = Cfg::Get();
    if (!c.enable) return GestureAction::None;

    const Vector4& sc = nav.SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER];
    const Vector4& hc = nav.SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER];
    if (!Usable(nav.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER]) ||
        !Usable(nav.eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HIP_CENTER]))
        return GestureAction::None;

    const float torso = Len3(sc, hc);
    if (torso < 0.15f) return GestureAction::None;

    int hj = NUI_SKELETON_POSITION_HAND_RIGHT;
    if (!Usable(nav.eSkeletonPositionTrackingState[hj]))
        hj = NUI_SKELETON_POSITION_WRIST_RIGHT;
    if (!Usable(nav.eSkeletonPositionTrackingState[hj]))
        return GestureAction::None;

    const Vector4& hand = nav.SkeletonPositions[hj];
    const float hx = (hand.x - sc.x) / torso;
    const float hy = (hand.y - sc.y) / torso;

    const double dt = (g.prevMs >= 0) ? (frameMs - g.prevMs) / 1000.0 : 0.0;
    g.prevMs = frameMs;

    if (!g.have || dt <= 0.0 || dt > 0.25)
    {
        g.fx = g.sx = hx;  g.fy = g.sy = hy;
        g.fxPrev = g.fx;   g.fyPrev = g.fy;
        g.have = true;      g.frames = 1;
        return GestureAction::None;
    }

    const float aF = c.smoothFast, aS = c.smoothSlow;
    g.fx = aF * hx + (1 - aF) * g.fx;
    g.fy = aF * hy + (1 - aF) * g.fy;
    g.sx = aS * hx + (1 - aS) * g.sx;
    g.sy = aS * hy + (1 - aS) * g.sy;

    const float vx = (float)((g.fx - g.fxPrev) / dt);
    const float vy = (float)((g.fy - g.fyPrev) / dt);
    g.fxPrev = g.fx;  g.fyPrev = g.fy;

    const float evx = c.mirror ? -vx  : vx;
    const float efx = c.mirror ? -g.fx : g.fx;
    const float esx = c.mirror ? -g.sx : g.sx;

    if (c.trace && (g.lastTraceMs < 0 || frameMs - g.lastTraceMs >= 66))
    {
        g.lastTraceMs = frameMs;
        LogLine("trace[t=%.1f] %-4s efx=%+.2f fy=%+.2f evx=%+.2f esx=%+.2f sy=%+.2f%s rp=%d",
                T(frameMs), SmTag(g.sm), efx, g.fy, evx, esx, g.sy,
                g.swiping ? " SWIPING" : "", g.repeats);
    }

    ++g.frames;
    if (g.frames < c.armAfterFrames) return GestureAction::None;

    const bool handUp = g.fy > -0.75f;

    switch (g.sm)
    {
    // ---------------------------------------------------------------
    case SM::InitialArm:
        // hand somewhere near the body and not swinging
        if (fabsf(esx) < c.armCenterFx && fabsf(evx) < c.swipeVelocity)
        {
            if (g.neutralSinceMs < 0) g.neutralSinceMs = frameMs;
            if (frameMs - g.neutralSinceMs >= c.neutralHoldMs)
            {
                Enter(SM::Armed, frameMs);
                g.swiping = false;
                LogLine("Gesture[t=%.1f]: ARMED", T(frameMs));
            }
        }
        else g.neutralSinceMs = -1;
        return GestureAction::None;

    // ---------------------------------------------------------------
    case SM::Armed:
    {
        if (!g.swiping)
        {
            const bool fast = fabsf(evx) > c.swipeVelocity && fabsf(evx) < c.swipeVxCeiling;
            const int  dir  = evx > 0 ? 1 : -1;
            if (handUp && fast)
            {
                if (g.cand == dir) ++g.candFrames;
                else { g.cand = dir; g.candFrames = 1; }

                if (g.candFrames >= c.swipeConfirmFrames)
                {
                    g.swiping = true;
                    g.swipeDir = dir;
                    g.swipeStartEfx = efx;
                    g.swipeFrames = g.candFrames;
                    g.cand = 0; g.candFrames = 0;
                    LogLine("Gesture[t=%.1f]: swipe start  dir=%s evx=%.2f efx=%.2f",
                            T(frameMs), g.swipeDir > 0 ? "R" : "L", evx, efx);
                }
            }
            else if (fabsf(evx) < 0.3f * c.swipeVelocity)
            {
                g.cand = 0; g.candFrames = 0;   // motion died, forget the candidate
            }
            return GestureAction::None;
        }

        ++g.swipeFrames;
        const float progress = g.swipeDir * (efx - g.swipeStartEfx);   // + = toward the swiped side
        const float dirVel   = g.swipeDir * evx;

        if (dirVel < -0.3f * c.swipeVelocity || g.swipeFrames > 45)
        {
            LogLine("Gesture[t=%.1f]: swipe abort  prog=%.2f frames=%d", T(frameMs), progress, g.swipeFrames);
            g.swiping = false;
            return GestureAction::None;
        }
        if (g.swipeFrames >= c.swipeMinFrames && progress >= c.swipeDistance && dirVel > 0.0f)
        {
            LogLine("Gesture[t=%.1f]: FIRE swipe-%s  prog=%.2f frames=%d",
                    T(frameMs), g.swipeDir > 0 ? "right" : "left", progress, g.swipeFrames);
            g.swiping = false;
            g.repeats = 1;
            g.holdDir = g.swipeDir;
            Enter(SM::PostSwipe, frameMs);
            return DirAction(g.swipeDir);
        }
        if (dirVel < 0.3f * c.swipeVelocity)
        {
            LogLine("Gesture[t=%.1f]: swipe fizzled  prog=%.2f frames=%d", T(frameMs), progress, g.swipeFrames);
            g.swiping = false;
        }
        return GestureAction::None;
    }

    // ---------------------------------------------------------------
    case SM::PostSwipe:
    {
        if (frameMs - g.stateMs > c.postSwipeMs)
        {
            const float dirPos = g.holdDir * efx;      // how far out on the swiped side
            if (dirPos > c.holdEnterFx)
            {
                Enter(SM::Holding, frameMs);
                g.holdMiss = 0;
                g.nextRepeatMs = frameMs + c.firstRepeatMs;
                LogLine("Gesture[t=%.1f]: HOLD begin  dir=%s efx=%.2f",
                        T(frameMs), g.holdDir > 0 ? "R" : "L", efx);
            }
            else
            {
                Enter(SM::ReArm, frameMs);
                LogLine("Gesture[t=%.1f]: single step", T(frameMs));
            }
        }
        return GestureAction::None;
    }

    // ---------------------------------------------------------------
    case SM::Holding:
    {
        const float dirPosSlow = g.holdDir * esx;      // sustained distance out on the swiped side
        const bool  out = (dirPosSlow > c.holdExitFx) && (g.sy > c.holdDropFy);
        if (!out)
        {
            if (++g.holdMiss >= c.holdExitFrames)
            {
                Enter(SM::ReArm, frameMs);
                LogLine("Gesture[t=%.1f]: HOLD end  (repeats=%d, esx=%.2f sy=%.2f)",
                        T(frameMs), g.repeats, esx, g.sy);
                return GestureAction::None;
            }
        }
        else g.holdMiss = 0;

        if (frameMs >= g.nextRepeatMs)
        {
            ++g.repeats;
            int interval = c.firstRepeatMs - (g.repeats - 1) * c.repeatAccelMs;
            if (interval < c.minRepeatMs) interval = c.minRepeatMs;
            g.nextRepeatMs = frameMs + interval;
            LogLine("Gesture[t=%.1f]: repeat #%d %s", T(frameMs), g.repeats, g.holdDir > 0 ? "R" : "L");
            return DirAction(g.holdDir);
        }
        return GestureAction::None;
    }

    // ---------------------------------------------------------------
    case SM::ReArm:
    {
        if (frameMs - g.stateMs >= c.cooldownMs && fabsf(efx) < c.recenterFx)
        {
            Enter(SM::Armed, frameMs);
            g.swiping = false;
            LogLine("Gesture[t=%.1f]: re-armed", T(frameMs));
        }
        else if (frameMs - g.stateMs > c.reArmTimeoutMs)
        {
            Enter(SM::InitialArm, frameMs);
            g.neutralSinceMs = -1;
            LogLine("Gesture[t=%.1f]: re-arm timeout -> initial arm", T(frameMs));
        }
        return GestureAction::None;
    }
    }

    return GestureAction::None;
}
