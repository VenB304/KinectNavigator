#pragma once
#include "framework.h"
#include "gestures.h"   // BodyDebug, GestureDebug
#include "config.h"     // Config

// Shared GDI rendering primitives for a full-skeleton stick figure + the anatomically-placed
// d-pad ring / command gate. Used by the in-game overlay (overlay.cpp, real tracked bodies) and
// by KinectNavigatorTutorial (both a real live-tracked body AND a synthetic "ghost" pose fed from
// a hand-authored keyframe table) -- neither caller-specific, so it lives on its own.

namespace SkelRender
{
    // ---- shared palette ----
    extern const COLORREF cPanel, cBord;
    extern const COLORREF cTxt, cDim, cFnt;
    extern const COLORREF cGrn, cAmb;
    extern const COLORREF cCyn, cMag;
    extern const COLORREF cGrid, cTrk;
    extern const COLORREF cAsleep, cDemoted;
    extern const COLORREF cHandDot;   // hand joint markers -- always this colour so hands read
                                       // at a glance regardless of the body's role colour

    void PText(HDC dc, HFONT f, int x, int y, const wchar_t* s, COLORREF col, UINT al);
    void FillRectC(HDC dc, int x, int y, int w, int h, COLORREF col);

    // Projects every joint into pixel space, anchored on HIP_CENTER and mirrored the same way
    // the recognizer mirrors hand offsets (gestures.cpp: `m = mirror ? -1 : 1` on x) -- so the
    // drawn figure always agrees with what's actually driving the d-pad. Works for a live body
    // (real tracking states) or a synthetic/ghost BodyDebug (a hand-authored pose, tracking
    // states all TRACKED).
    void ProjectBody(const BodyDebug& b, bool mirror, int cx, int cyHip, float scale,
                     POINT pos[NUI_SKELETON_POSITION_COUNT]);

    // Draws the stick figure from an already-projected joint set. A bone with either endpoint
    // untracked draws dim rather than guessing a position.
    void DrawSkeleton(HDC dc, const BodyDebug& b, const POINT pos[NUI_SKELETON_POSITION_COUNT],
                      COLORREF col, int weight);

    // Anatomically-placed d-pad ring (at the dominant shoulder) + command-mode gate (at the
    // non-dominant shoulder). Same park-box/gate math the recognizer uses (dpadParkR /
    // dpadUpReachK / dpadCrossReachK / dpadCmdGateR), re-centred on the real joint.
    void DrawDpadOverlay(HDC dc, const POINT pos[NUI_SKELETON_POSITION_COUNT],
                         const GestureDebug& d, const Config& c, float scale, HFONT fLb, int lineW);

    // Role -> figure colour + plain-English label. `role` mirrors gestures.cpp's DpadState
    // (0 Asleep, 1 Arming, 2 Armed, 3 Driving, 4 Demoted -- see BodyDebug's comment in gestures.h).
    void RoleStyle(int role, COLORREF& col, const wchar_t*& label);
}
