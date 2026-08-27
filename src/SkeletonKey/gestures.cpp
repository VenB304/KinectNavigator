#include "framework.h"
#include <math.h>
#include "gestures.h"
#include "config.h"
#include "log.h"

namespace
{
    enum class SM { NeutralWait, Armed, Cooldown };

    struct State
    {
        bool     have      = false;   // baselines valid
        LONGLONG prevMs    = -1;
        int      frames    = 0;       // frames since acquire (for arm delay)

        // smoothed body-relative right-hand position (torso units)
        float    fx = 0, fy = 0;      // fast (motion)
        float    sx = 0, sy = 0;      // slow (neutral test)
        float    fxPrev = 0, fyPrev = 0;

        SM       sm = SM::NeutralWait;
        LONGLONG neutralSinceMs = -1;
        LONGLONG cooldownUntilMs = 0;
        LONGLONG lastTraceMs = -1;

        bool     swiping   = false;
        float    swipeStartFx = 0;
        int      swipeFrames  = 0;
    } g;

    // Wall-clock-independent capture timeline for the logs. Set on the first
    // frame ever seen and never reset, so events across body flickers share
    // one axis. Seconds since that first frame.
    LONGLONG g_epoch = -1;
    double   T(LONGLONG ms) { if (g_epoch < 0) g_epoch = ms; return (ms - g_epoch) / 1000.0; }

    inline float Len3(const Vector4& a, const Vector4& b)
    {
        const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }

    bool Usable(NUI_SKELETON_POSITION_TRACKING_STATE s)
    {
        return s == NUI_SKELETON_POSITION_TRACKED || s == NUI_SKELETON_POSITION_INFERRED;
    }
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

    // right hand, fall back to wrist
    int hj = NUI_SKELETON_POSITION_HAND_RIGHT;
    if (!Usable(nav.eSkeletonPositionTrackingState[hj]))
        hj = NUI_SKELETON_POSITION_WRIST_RIGHT;
    if (!Usable(nav.eSkeletonPositionTrackingState[hj]))
        return GestureAction::None;

    const Vector4& hand = nav.SkeletonPositions[hj];
    const float hx = (hand.x - sc.x) / torso;
    const float hy = (hand.y - sc.y) / torso;

    // dt from the sensor clock
    double dt = (g.prevMs >= 0) ? (frameMs - g.prevMs) / 1000.0 : 0.0;
    g.prevMs = frameMs;

    if (!g.have || dt <= 0.0 || dt > 0.25)
    {
        // (re)seed baselines; no velocity this frame
        g.fx = g.sx = hx;
        g.fy = g.sy = hy;
        g.fxPrev = g.fx;
        g.fyPrev = g.fy;
        g.have = true;
        g.frames = 1;
        return GestureAction::None;
    }

    const float aF = c.smoothFast, aS = c.smoothSlow;
    g.fx = aF * hx + (1 - aF) * g.fx;
    g.fy = aF * hy + (1 - aF) * g.fy;
    g.sx = aS * hx + (1 - aS) * g.sx;
    g.sy = aS * hy + (1 - aS) * g.sy;

    const float vx = (float)((g.fx - g.fxPrev) / dt);   // torso/sec
    const float vy = (float)((g.fy - g.fyPrev) / dt);
    g.fxPrev = g.fx;
    g.fyPrev = g.fy;

    if (c.trace && (g.lastTraceMs < 0 || frameMs - g.lastTraceMs >= 66))
    {
        g.lastTraceMs = frameMs;
        const char* sm = g.sm == SM::NeutralWait ? "NEU" : g.sm == SM::Armed ? "ARM" : "CLD";
        LogLine("trace[t=%.1f] %s fx=%+.2f fy=%+.2f vx=%+.2f vy=%+.2f sx=%+.2f sy=%+.2f%s",
                T(frameMs), sm, g.fx, g.fy, vx, vy, g.sx, g.sy, g.swiping ? " SWIPING" : "");
    }

    ++g.frames;
    if (g.frames < c.armAfterFrames) return GestureAction::None;

    // --- state machine ---------------------------------------------------
    const bool neutralPose = (g.sy < -0.55f) && (fabsf(g.sx) < 0.75f);

    switch (g.sm)
    {
    case SM::NeutralWait:
        if (neutralPose)
        {
            if (g.neutralSinceMs < 0) g.neutralSinceMs = frameMs;
            if (frameMs - g.neutralSinceMs >= c.neutralHoldMs)
            {
                g.sm = SM::Armed;
                g.swiping = false;
                LogLine("Gesture[t=%.1f]: ARMED", T(frameMs));
            }
        }
        else g.neutralSinceMs = -1;
        return GestureAction::None;

    case SM::Cooldown:
        if (frameMs >= g.cooldownUntilMs)
        {
            g.sm = SM::NeutralWait;
            g.neutralSinceMs = -1;
            LogLine("Gesture[t=%.1f]: cooldown over -> need neutral", T(frameMs));
        }
        return GestureAction::None;

    case SM::Armed:
        break;
    }

    // Armed: horizontal right-hand swipe. mirror flips the "right" direction.
    const float evx = c.mirror ? -vx : vx;
    const bool  handUp = g.fy > -0.75f;                 // not hanging by the legs

    if (!g.swiping)
    {
        if (handUp && evx > c.swipeVelocity)
        {
            g.swiping = true;
            g.swipeStartFx = g.fx;
            g.swipeFrames = 1;
            LogLine("Gesture[t=%.1f]: swipe start  evx=%.2f fx=%.2f fy=%.2f", T(frameMs), evx, g.fx, g.fy);
        }
        return GestureAction::None;
    }

    // swiping
    ++g.swipeFrames;
    const float disp = c.mirror ? -(g.fx - g.swipeStartFx) : (g.fx - g.swipeStartFx);

    if (evx < -0.3f * c.swipeVelocity || g.swipeFrames > 45)
    {
        LogLine("Gesture[t=%.1f]: swipe abort  disp=%.2f frames=%d evx=%.2f", T(frameMs), disp, g.swipeFrames, evx);
        g.swiping = false;
        return GestureAction::None;
    }

    if (g.swipeFrames >= c.swipeMinFrames && disp >= c.swipeDistance && evx > 0.0f)
    {
        LogLine("Gesture[t=%.1f]: FIRE swipe-right  disp=%.2f frames=%d", T(frameMs), disp, g.swipeFrames);
        g.swiping = false;
        g.sm = SM::Cooldown;
        g.cooldownUntilMs = frameMs + c.cooldownMs;
        return GestureAction::Right;
    }

    if (evx < 0.3f * c.swipeVelocity)   // decelerated without reaching threshold
    {
        LogLine("Gesture[t=%.1f]: swipe fizzled  disp=%.2f frames=%d", T(frameMs), disp, g.swipeFrames);
        g.swiping = false;
    }
    return GestureAction::None;
}
