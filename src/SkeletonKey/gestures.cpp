#include "framework.h"
#include <math.h>
#include "gestures.h"
#include "config.h"
#include "log.h"

// M3: right-hand horizontal swipe -> VK_RIGHT.
//   quick swipe            = one press
//   swipe, then keep the hand raised and out to the right = auto-repeat
//                            (accelerating), until the hand drops or comes back
// Re-arming only needs the hand to come back from the right, not a full
// arms-at-sides neutral -- that was eating rapid repeated swipes.

namespace
{
    enum class SM { NeutralWait, Armed, PostSwipe, Holding, ReArm };

    const char* SmTag(SM s)
    {
        switch (s) {
        case SM::NeutralWait: return "NEU";
        case SM::Armed:       return "ARM";
        case SM::PostSwipe:   return "PSW";
        case SM::Holding:     return "HLD";
        case SM::ReArm:       return "RARM";
        }
        return "?";
    }

    struct State
    {
        bool     have   = false;
        LONGLONG prevMs = -1;
        int      frames = 0;

        float    fx = 0, fy = 0;        // fast-smoothed body-relative right hand
        float    sx = 0, sy = 0;        // slow-smoothed (neutral test)
        float    fxPrev = 0, fyPrev = 0;

        SM       sm = SM::NeutralWait;
        LONGLONG neutralSinceMs = -1;
        LONGLONG stateMs = 0;           // when the current sm was entered
        LONGLONG nextRepeatMs = 0;
        int      repeats = 0;

        bool     swiping = false;
        float    swipeStartFx = 0;
        int      swipeFrames = 0;

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

    if (c.trace && (g.lastTraceMs < 0 || frameMs - g.lastTraceMs >= 66))
    {
        g.lastTraceMs = frameMs;
        LogLine("trace[t=%.1f] %-4s fx=%+.2f fy=%+.2f vx=%+.2f vy=%+.2f sy=%+.2f%s rp=%d",
                T(frameMs), SmTag(g.sm), g.fx, g.fy, vx, vy, g.sy,
                g.swiping ? " SWIPING" : "", g.repeats);
    }

    ++g.frames;
    if (g.frames < c.armAfterFrames) return GestureAction::None;

    const float evx = c.mirror ? -vx : vx;
    const float efx = c.mirror ? -g.fx : g.fx;      // signed "how far to the RIGHT" the hand is
    const bool  handUp     = g.fy > -0.75f;
    const bool  neutralPose = (g.sy < -0.55f) && (fabsf(g.sx) < 0.75f);

    switch (g.sm)
    {
    // ---------------------------------------------------------------
    case SM::NeutralWait:
        if (neutralPose)
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
            if (handUp && evx > c.swipeVelocity && evx < c.swipeVxCeiling)
            {
                g.swiping = true;
                g.swipeStartFx = g.fx;
                g.swipeFrames = 1;
                LogLine("Gesture[t=%.1f]: swipe start  evx=%.2f fx=%.2f fy=%.2f",
                        T(frameMs), evx, g.fx, g.fy);
            }
            return GestureAction::None;
        }

        ++g.swipeFrames;
        const float disp = c.mirror ? -(g.fx - g.swipeStartFx) : (g.fx - g.swipeStartFx);

        if (evx < -0.3f * c.swipeVelocity || g.swipeFrames > 45)
        {
            LogLine("Gesture[t=%.1f]: swipe abort  disp=%.2f frames=%d", T(frameMs), disp, g.swipeFrames);
            g.swiping = false;
            return GestureAction::None;
        }
        if (g.swipeFrames >= c.swipeMinFrames && disp >= c.swipeDistance && evx > 0.0f)
        {
            LogLine("Gesture[t=%.1f]: FIRE swipe-right  disp=%.2f frames=%d", T(frameMs), disp, g.swipeFrames);
            g.swiping = false;
            g.repeats = 1;
            Enter(SM::PostSwipe, frameMs);
            return GestureAction::Right;
        }
        if (evx < 0.3f * c.swipeVelocity)
        {
            LogLine("Gesture[t=%.1f]: swipe fizzled  disp=%.2f frames=%d", T(frameMs), disp, g.swipeFrames);
            g.swiping = false;
        }
        return GestureAction::None;
    }

    // ---------------------------------------------------------------
    case SM::PostSwipe:
    {
        const bool inHold = (g.fy > c.holdRaisedFy) && (efx > c.holdEnterFx);
        if (inHold)
        {
            Enter(SM::Holding, frameMs);
            g.nextRepeatMs = frameMs + c.firstRepeatMs;
            LogLine("Gesture[t=%.1f]: HOLD begin", T(frameMs));
            return GestureAction::None;
        }
        if (frameMs - g.stateMs > c.postSwipeMs)
        {
            Enter(SM::ReArm, frameMs);
            LogLine("Gesture[t=%.1f]: single step", T(frameMs));
        }
        return GestureAction::None;
    }

    // ---------------------------------------------------------------
    case SM::Holding:
    {
        const bool stillHold = (g.fy > c.holdExitFy) && (efx > c.holdExitFx);
        if (!stillHold)
        {
            Enter(SM::ReArm, frameMs);
            LogLine("Gesture[t=%.1f]: HOLD end  (repeats=%d)", T(frameMs), g.repeats);
            return GestureAction::None;
        }
        if (frameMs >= g.nextRepeatMs)
        {
            ++g.repeats;
            int interval = c.firstRepeatMs - (g.repeats - 1) * c.repeatAccelMs;
            if (interval < c.minRepeatMs) interval = c.minRepeatMs;
            g.nextRepeatMs = frameMs + interval;
            LogLine("Gesture[t=%.1f]: repeat #%d", T(frameMs), g.repeats);
            return GestureAction::Right;
        }
        return GestureAction::None;
    }

    // ---------------------------------------------------------------
    case SM::ReArm:
    {
        const bool recentered = (efx < c.recenterFx) || (g.fy < c.recenterFy);
        if (frameMs - g.stateMs >= c.cooldownMs && recentered)
        {
            Enter(SM::Armed, frameMs);
            g.swiping = false;
            LogLine("Gesture[t=%.1f]: re-armed", T(frameMs));
        }
        else if (frameMs - g.stateMs > 2500)
        {
            Enter(SM::NeutralWait, frameMs);
            g.neutralSinceMs = -1;
            LogLine("Gesture[t=%.1f]: re-arm timeout -> neutral", T(frameMs));
        }
        return GestureAction::None;
    }
    }

    return GestureAction::None;
}
