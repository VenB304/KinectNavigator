// KinectNavigatorTutorial -- a live, standalone "try it out" teacher for the air d-pad.
//
// Launched from the GUI/Console app (KinectNavigator.Gui.ps1 / .Console.ps1), NOT from inside
// the game: exclusive fullscreen (the common case) hides the in-game overlay entirely, so a
// tutorial that only lived there would miss most players. This tool needs no game at all --
// just the sensor -- so exclusive fullscreen is a non-issue.
//
// It runs the SAME recognizer the shim does (Gestures::UpdateExtend, single-threaded here --
// no FrameBuffer/Recognizer-thread needed since this reads a frame and reacts to it in one
// place), draws the player's live skeleton next to a semi-transparent "ghost" reference figure
// performing the target motion (SkelRender -- shared with the in-game overlay), and advances a
// short step sequence when the REAL gesture fires, not a scripted fake or a timer.
//
// Never sends keystrokes: Output.cpp / Recognizer.cpp / GameProbe.cpp aren't even linked into
// this project, so there is no code path that could inject a keypress into whatever window has
// focus -- a stronger guarantee than a dry-run flag.

#include "framework.h"
#include <math.h>
#include <string>
#include <vector>
#include "nui_types.h"
#include "nui_bridge.h"
#include "gestures.h"
#include "skeleton_render.h"
#include "tutorial_strings.h"
#include "config.h"
#include "globals.h"
#include "log.h"

using namespace SkelRender;
using TutStr::S;

namespace
{
    // ============ neutral standing pose (metres, camera-space-ish; z is unused by
    // ProjectBody so any constant works) -- torso (SHOULDER_CENTER-HIP_CENTER) = 0.40 m,
    // matching BodyDebug's default scale. ============
    Vector4 NeutralJoint(int j)
    {
        static const float T = 0.40f;
        switch (j)
        {
        case NUI_SKELETON_POSITION_HIP_CENTER:     return { 0.00f,  0.00f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_SPINE:          return { 0.00f,  0.50f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_SHOULDER_CENTER:return { 0.00f,  1.00f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_HEAD:           return { 0.00f,  1.62f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_SHOULDER_LEFT:  return { -0.50f * T,  0.95f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_SHOULDER_RIGHT: return { 0.50f * T,  0.95f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_ELBOW_LEFT:     return { -0.72f * T,  0.30f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_ELBOW_RIGHT:    return { 0.72f * T,  0.30f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_WRIST_LEFT:     return { -0.78f * T, -0.35f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_WRIST_RIGHT:    return { 0.78f * T, -0.35f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_HAND_LEFT:      return { -0.80f * T, -0.62f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_HAND_RIGHT:     return { 0.80f * T, -0.62f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_HIP_LEFT:       return { -0.30f * T, -0.05f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_HIP_RIGHT:      return { 0.30f * T, -0.05f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_KNEE_LEFT:      return { -0.33f * T, -1.35f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_KNEE_RIGHT:     return { 0.33f * T, -1.35f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_ANKLE_LEFT:     return { -0.35f * T, -2.45f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_ANKLE_RIGHT:    return { 0.35f * T, -2.45f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_FOOT_LEFT:      return { -0.35f * T, -2.58f * T, 2.5f, 1 };
        case NUI_SKELETON_POSITION_FOOT_RIGHT:     return { 0.35f * T, -2.58f * T, 2.5f, 1 };
        }
        return { 0, 0, 2.5f, 1 };
    }

    BodyDebug BuildGhostPose(const Config& c)
    {
        BodyDebug g{};
        g.inUse = true; g.id = 0; g.role = 3 /* draw bright, like the driver */;
        g.torso = 0.40f;
        for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j)
        {
            g.joints[j] = NeutralJoint(j);
            g.jointState[j] = NUI_SKELETON_POSITION_TRACKED;
        }
        return g;
    }

    // Poses one arm by blending BOTH the hand and the elbow, each from its own fixed "from"
    // offset to its own fixed "to" offset (torso units, recognizer's ex/ey convention -- +x =
    // the user's right, +y = up). No IK: an elbow solved fresh each frame from a moving hand
    // target has two valid solutions, and picking between them (even by a consistent rule like
    // "lower one wins") flips discontinuously as the hand sweeps through certain angles -- that
    // is what read as the elbow "flapping" / "possessed". Keying the elbow explicitly, the same
    // way the hand target already is, means every frame is just two fixed points blending
    // toward two other fixed points -- nothing to solve, nothing to flip.
    // `from == to` on both hand and elbow (Wake, command mode) renders one fixed pose regardless
    // of `t`, for free.
    void PoseArm(BodyDebug& g, const Config& c, bool dominant,
                float fromHandEx, float fromHandEy, float fromElbowEx, float fromElbowEy,
                float toHandEx, float toHandEy, float toElbowEx, float toElbowEy, float t)
    {
        const int shJ = (dominant == !c.leftHanded) ? NUI_SKELETON_POSITION_SHOULDER_RIGHT : NUI_SKELETON_POSITION_SHOULDER_LEFT;
        const int elJ = (dominant == !c.leftHanded) ? NUI_SKELETON_POSITION_ELBOW_RIGHT    : NUI_SKELETON_POSITION_ELBOW_LEFT;
        const int wrJ = (dominant == !c.leftHanded) ? NUI_SKELETON_POSITION_WRIST_RIGHT    : NUI_SKELETON_POSITION_WRIST_LEFT;
        const int hJ  = (dominant == !c.leftHanded) ? NUI_SKELETON_POSITION_HAND_RIGHT     : NUI_SKELETON_POSITION_HAND_LEFT;

        const Vector4 sh = g.joints[shJ];
        // pre-compensate for ProjectBody's own mirror flip so the ghost always reaches the
        // semantically-correct way (screen-right for a "RIGHT" cue) regardless of `mirror`.
        // Every offset table below is authored from the DOMINANT arm's point of view (+ex =
        // toward that arm's own outward side); armSide flips it for the non-dominant arm so its
        // pose is a true left-right mirror instead of both arms bending the same absolute way.
        const float m = c.mirror ? -1.f : 1.f;
        const float armSide = dominant ? 1.f : -1.f;
        auto toWorld = [&](float ex, float ey) {
            Vector4 v; v.x = sh.x + m * armSide * ex * g.torso; v.y = sh.y + ey * g.torso; v.z = sh.z; v.w = 1;
            return v;
        };
        auto blend = [&](float fx, float fy, float tx, float ty) {
            const Vector4 a = toWorld(fx, fy), b = toWorld(tx, ty);
            Vector4 v; v.x = a.x + (b.x - a.x) * t; v.y = a.y + (b.y - a.y) * t; v.z = sh.z; v.w = 1;
            return v;
        };

        const Vector4 hand  = blend(fromHandEx, fromHandEy, toHandEx, toHandEy);
        const Vector4 elbow = blend(fromElbowEx, fromElbowEy, toElbowEx, toElbowEy);

        g.joints[hJ] = hand;
        g.joints[elJ] = elbow;
        g.joints[wrJ] = { elbow.x + (hand.x - elbow.x) * 0.55f, elbow.y + (hand.y - elbow.y) * 0.55f, sh.z, 1 };
    }

    // Hand + matching elbow offset for each pose (torso units from the shoulder). Tuned live
    // against a real sensor during development (a since-removed in-app tuning mode) -- Command
    // mode / Confirm / Back ended up with their own distinct values rather than sharing
    // Wake/Right-Up/Down's, so each gets its own named constant.
    const float kParkEx = 0.150f,  kParkEy = 0.030f,  kParkElbowEx = 0.200f,  kParkElbowEy = -0.600f;
    const float kRightEx = 1.030f, kRightEy = -0.020f,kRightElbowEx = 0.450f, kRightElbowEy = -0.200f;
    const float kLeftEx  = -1.130f,kLeftEy  = -0.040f,kLeftElbowEx = -0.510f, kLeftElbowEy = -0.160f;
    const float kUpEx    = 0.060f, kUpEy    = 1.010f, kUpElbowEx = 0.180f,   kUpElbowEy = 0.420f;
    const float kDownEx  = 0.770f, kDownEy  = -1.150f,kDownElbowEx = 0.305f, kDownElbowEy = -0.555f;
    const float kCmdEx   = 0.110f, kCmdEy   = 0.050f, kCmdElbowEx = 0.200f,  kCmdElbowEy = -0.600f;
    const float kConfirmEx = 0.160f, kConfirmEy = 0.970f, kConfirmElbowEx = 0.180f, kConfirmElbowEy = 0.420f;
    const float kBackEx  = 0.770f, kBackEy  = -0.890f,kBackElbowEx = 0.285f, kBackElbowEy = -0.495f;

    // ============ step sequence ============
    enum class StepKind { Intro, Wake, Nav, CmdGate, Confirm, Back, Done };

    struct Step
    {
        StepKind kind;
        S titleId, bodyId;
        float toEx, toEy;           // dominant-hand ghost target (torso units)
        float toElbowEx, toElbowEy; // matching elbow keyframe
        bool needCmd;                // also pose the non-dominant hand at ITS park position (static)
    };

    const Step kSteps[] = {
        { StepKind::Intro,   S::WelcomeTitle, S::WelcomeBody, 0, 0, 0, 0, false },
        { StepKind::Wake,    S::WakeTitle,    S::WakeBody,    kParkEx, kParkEy, kParkElbowEx, kParkElbowEy, false },
        { StepKind::Nav,     S::RightTitle,   S::RightBody,   kRightEx, kRightEy, kRightElbowEx, kRightElbowEy, false },
        { StepKind::Nav,     S::LeftTitle,    S::LeftBody,    kLeftEx, kLeftEy, kLeftElbowEx, kLeftElbowEy, false },
        { StepKind::Nav,     S::UpTitle,      S::UpBody,      kUpEx, kUpEy, kUpElbowEx, kUpElbowEy, false },
        { StepKind::Nav,     S::DownTitle,    S::DownBody,    kDownEx, kDownEy, kDownElbowEx, kDownElbowEy, false },
        { StepKind::CmdGate, S::CmdTitle,     S::CmdBody,     kCmdEx, kCmdEy, kCmdElbowEx, kCmdElbowEy, true },
        { StepKind::Confirm, S::ConfirmTitle, S::ConfirmBody, kConfirmEx, kConfirmEy, kConfirmElbowEx, kConfirmElbowEy, true },
        { StepKind::Back,    S::BackTitle,    S::BackBody,    kBackEx, kBackEy, kBackElbowEx, kBackElbowEy, true },
        { StepKind::Done,    S::DoneTitle,    S::DoneBody,    0, 0, 0, 0, false },
    };
    const int kNumSteps = sizeof(kSteps) / sizeof(kSteps[0]);

    int FindStepIndex(StepKind kind)
    {
        for (int i = 0; i < kNumSteps; ++i) if (kSteps[i].kind == kind) return i;
        return 0;
    }
    const int g_wakeIdx = FindStepIndex(StepKind::Wake);   // Wake's target IS the shared park pose

    // ============ app state ============
    enum class ConnState { Connecting, Waiting, Ready };

    HWND       g_hwnd = nullptr;
    ConnState  g_conn = ConnState::Connecting;
    NuiBridge::Status g_lastBindErr = NuiBridge::Status::Ok;
    LONGLONG   g_nextRetryMs = 0;
    LONGLONG   g_lastFrameTickMs = 0;
    bool       g_everConnected = false;

    int        g_step = 0;
    LONGLONG   g_stepStartMs = 0;
    LONGLONG   g_lastActionSeenMs = -1;

    Config     g_cfg{};

    LONGLONG NowMs() { return (LONGLONG)GetTickCount64(); }

    void OnFrame(const NUI_SKELETON_FRAME& fr, void* /*ctx*/)
    {
        g_lastFrameTickMs = NowMs();
        Gestures::UpdateExtend(fr, fr.liTimeStamp.QuadPart);
    }

    // The recognizer only computes gd.dpadCmd once the DOMINANT hand has left the park box --
    // RunNavMachine returns early on `b.parked`, before it ever checks the non-dominant gate
    // (see gestures.cpp). That's fine for real play (command mode only matters while you're
    // mid-reach), but this step's instruction only asks the player to move their OTHER hand, so
    // if the dominant hand stays parked, gd.dpadCmd never updates and the step can't complete.
    // Measure the same non-dominant-hand-to-shoulder distance directly from the live skeleton
    // instead, independent of whether the dominant arm happens to be parked this frame.
    bool CmdGateReached(const GestureDebug& gd, const Config& c)
    {
        if (gd.bodyCount == 0) return false;
        const BodyDebug& b = gd.bodies[0];
        const int shJ = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_RIGHT : NUI_SKELETON_POSITION_SHOULDER_LEFT;
        const int hJ0 = c.leftHanded ? NUI_SKELETON_POSITION_HAND_RIGHT    : NUI_SKELETON_POSITION_HAND_LEFT;
        const int wJ0 = c.leftHanded ? NUI_SKELETON_POSITION_WRIST_RIGHT  : NUI_SKELETON_POSITION_WRIST_LEFT;
        if (b.jointState[shJ] == NUI_SKELETON_POSITION_NOT_TRACKED) return false;
        const int hJ = (b.jointState[hJ0] != NUI_SKELETON_POSITION_NOT_TRACKED) ? hJ0 : wJ0;
        if (b.jointState[hJ] == NUI_SKELETON_POSITION_NOT_TRACKED) return false;
        const float torso = b.torso > 0.05f ? b.torso : 0.40f;
        const float dx = b.joints[hJ].x - b.joints[shJ].x, dy = b.joints[hJ].y - b.joints[shJ].y, dz = b.joints[hJ].z - b.joints[shJ].z;
        const float dist = sqrtf(dx * dx + dy * dy + dz * dz) / torso;
        return dist < gd.dpadCmdGateR;
    }

    bool StepComplete(const Step& st, const GestureDebug& gd, LONGLONG stepStartMs, const Config& c)
    {
        const bool actedSinceStart = gd.lastActionMs >= stepStartMs;
        switch (st.kind)
        {
        case StepKind::Intro:   return gd.dpadNumBodies > 0;
        case StepKind::Wake:    return gd.dpadArmed;
        case StepKind::Nav:
            // Whichever axis has the larger magnitude is the intended direction -- a plain
            // "check ex first" order breaks for Down, whose target is down-AND-OUT (ex has to
            // be positive and > 0.4 by design), so it used to always match the Right branch
            // first and never recognize a real Down gesture.
            if (!actedSinceStart) return false;
            if (fabsf(st.toEy) > fabsf(st.toEx))
                return gd.lastAction == (st.toEy > 0.f ? 3 : 4);   // Up : Down
            return gd.lastAction == (st.toEx > 0.f ? 2 : 1);       // Right : Left
        case StepKind::CmdGate: return gd.dpadCmd || CmdGateReached(gd, c);
        case StepKind::Confirm: return actedSinceStart && gd.lastAction == 5;
        case StepKind::Back:    return actedSinceStart && gd.lastAction == 6;
        default: return true;
        }
    }

    void AdvanceStep()
    {
        if (g_step < kNumSteps - 1) ++g_step;
        g_stepStartMs = NowMs();
    }

    // ============ rendering ============
    HFONT MakeFont(int px, bool bold)
    {
        return CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                           OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_SWISS, L"Segoe UI");
    }

    // Word-wraps `text` (the currently selected font in `dc`) to fit within `maxW` pixels.
    // Some translations run much longer than English for the same sentence (German/Dutch/French
    // step instructions especially), so a single fixed-width TextOut line can run off the sides
    // of the window. Breaks on spaces; a single space-free run wider than maxW on its own (an
    // unbroken CJK sentence, say) is hard-broken character by character instead of overflowing.
    std::vector<std::wstring> WrapText(HDC dc, const wchar_t* text, int maxW)
    {
        std::vector<std::wstring> lines;
        std::wstring line, word;
        auto width = [&](const std::wstring& s) -> int {
            if (s.empty()) return 0;
            SIZE sz; GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
            return (int)sz.cx;
        };
        auto flushWord = [&]() {
            if (word.empty()) return;
            std::wstring candidate = line.empty() ? word : line + L" " + word;
            if (line.empty() || width(candidate) <= maxW) line = candidate;
            else { lines.push_back(line); line = word; }
            word.clear();
        };
        for (size_t i = 0; ; ++i)
        {
            wchar_t ch = text[i];
            bool end = (ch == L'\0');
            if (end || ch == L' ')
            {
                flushWord();
                if (end) break;
            }
            else
            {
                word += ch;
                if (line.empty() && word.size() > 1 && width(word) > maxW)
                {
                    wchar_t overflow = word.back();
                    word.pop_back();
                    lines.push_back(word);
                    word.assign(1, overflow);
                }
            }
        }
        if (!line.empty() || lines.empty()) lines.push_back(line);
        return lines;
    }

    // Wrapped, centered, multi-line PText. Returns the number of lines drawn so the caller can
    // lay out whatever comes next below the block.
    int PTextWrapped(HDC dc, HFONT f, int cx, int y, const wchar_t* s, COLORREF col, int maxW, int lineH)
    {
        SelectObject(dc, f); SetTextAlign(dc, TA_CENTER | TA_TOP); SetTextColor(dc, col);
        std::vector<std::wstring> lines = WrapText(dc, s, maxW);
        for (size_t i = 0; i < lines.size(); ++i)
            TextOutW(dc, cx, y + (int)i * lineH, lines[i].c_str(), (int)lines[i].size());
        return (int)lines.size();
    }

    int FontLineHeight(HDC dc, HFONT f)
    {
        HGDIOBJ old = SelectObject(dc, f);
        TEXTMETRICW tm; GetTextMetricsW(dc, &tm);
        SelectObject(dc, old);
        return (int)(tm.tmHeight + tm.tmExternalLeading);
    }

    void Paint(HWND hwnd)
    {
        RECT rc; GetClientRect(hwnd, &rc);
        const int W = rc.right, H = rc.bottom;

        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);
        FillRectC(mem, 0, 0, W, H, RGB(0x0E, 0x11, 0x16));
        SetBkMode(mem, TRANSPARENT);

        HFONT fH1 = MakeFont(34, true), fBody = MakeFont(20, false), fSm = MakeFont(16, false);

        if (g_conn != ConnState::Ready)
        {
            const bool needsRuntime = (g_conn == ConnState::Waiting) &&
                (g_lastBindErr == NuiBridge::Status::NoRuntime || g_lastBindErr == NuiBridge::Status::BadExports);
            const wchar_t* msg = (g_conn == ConnState::Connecting) ? TutStr::T(S::Connecting)
                                : needsRuntime ? TutStr::T(S::SoftwareNotFound)
                                : TutStr::T(S::NotDetected);
            PText(mem, fH1, W / 2, H / 2 - 60, msg, cTxt, TA_CENTER | TA_TOP);
            const wchar_t* sub = needsRuntime
                ? TutStr::T(S::SubSoftware)
                : (g_conn == ConnState::Waiting)
                    ? TutStr::T(S::SubWaiting)
                    : TutStr::T(S::SubMoment);
            PText(mem, fBody, W / 2, H / 2, sub, cDim, TA_CENTER | TA_TOP);
            PText(mem, fSm, W / 2, H - 40, TutStr::T(S::EscToClose), cFnt, TA_CENTER | TA_TOP);
        }
        else
        {
            const Step& st = kSteps[g_step];
            GestureDebug gd; Gestures::GetDebug(gd);

            const bool stale = g_everConnected && (NowMs() - g_lastFrameTickMs > 8000);

            // Title/body are word-wrapped to a margin'd width -- some translations (German,
            // Dutch, French especially) run much longer than English for the same sentence and
            // would otherwise run off the sides of a fixed single-line TextOut. Everything below
            // (progress dots, the reference/live skeleton band) is laid out relative to however
            // many lines that took, so a long translation grows the header instead of overlapping it.
            const int maxTextW = W - 160;
            const int titleLineH = FontLineHeight(mem, fH1), bodyLineH = FontLineHeight(mem, fBody);
            const int titleY = 24;
            int titleLines = PTextWrapped(mem, fH1, W / 2, titleY, TutStr::T(st.titleId), cTxt, maxTextW, titleLineH);
            const int bodyY = titleY + titleLines * titleLineH + 8;
            int bodyLines = PTextWrapped(mem, fBody, W / 2, bodyY, TutStr::T(st.bodyId), cDim, maxTextW, bodyLineH);
            const int dotsY = bodyY + bodyLines * bodyLineH + 18;

            // progress dots
            {
                const int n = kNumSteps, dr = 5, gap = 18, totalW = (n - 1) * gap;
                int x0 = W / 2 - totalW / 2;
                for (int i = 0; i < n; ++i)
                {
                    COLORREF c2 = (i < g_step) ? cGrn : (i == g_step) ? cCyn : cFnt;
                    HBRUSH b = CreateSolidBrush(c2);
                    HGDIOBJ ob = SelectObject(mem, GetStockObject(NULL_PEN)), ob2 = SelectObject(mem, b);
                    Ellipse(mem, x0 + i * gap - dr, dotsY - dr, x0 + i * gap + dr, dotsY + dr);
                    SelectObject(mem, ob); SelectObject(mem, ob2); DeleteObject(b);
                }
            }

            // ---- ghost (reference demo), left half ----
            const int bandTop = dotsY + 20, bandBot = H - 55;
            const float scale = (bandBot - bandTop) / 5.3f;
            const int cyHip = bandTop + (int)(2.6f * scale);
            const float loopT = fmodf((float)(NowMs() % 2400) / 2400.f, 1.f);
            const float ease = 0.5f - 0.5f * cosf(loopT * 2.f * 3.14159265f);   // ping-pong 0..1..0

            if (st.kind == StepKind::Nav || st.kind == StepKind::Wake || st.kind == StepKind::CmdGate ||
                st.kind == StepKind::Confirm || st.kind == StepKind::Back)
            {
                const Step& park = kSteps[g_wakeIdx];

                BodyDebug ghost = BuildGhostPose(g_cfg);
                // dominant hand+elbow: blend from the parked/armed keyframe to the step's target
                // keyframe -- Wake/CmdGate's target IS the park pose, so `from == to` and the
                // pose just sits still (see PoseArm's comment).
                PoseArm(ghost, g_cfg, true, park.toEx, park.toEy, park.toElbowEx, park.toElbowEy,
                       st.toEx, st.toEy, st.toElbowEx, st.toElbowEy, ease);
                // non-dominant hand: command-mode steps hold it at ITS park position, static.
                if (st.needCmd)
                    PoseArm(ghost, g_cfg, false, park.toEx, park.toEy, park.toElbowEx, park.toElbowEy,
                           park.toEx, park.toEy, park.toElbowEx, park.toElbowEy, 1.0f);

                POINT gpos[NUI_SKELETON_POSITION_COUNT];
                ProjectBody(ghost, g_cfg.mirror, W / 4, cyHip, scale, gpos);
                DrawSkeleton(mem, ghost, gpos, RGB(0x7A, 0x8A, 0x9C), 2);
                PText(mem, fSm, W / 4, bandBot + 8, TutStr::T(S::LabelReference), cFnt, TA_CENTER | TA_TOP);
            }

            // ---- live player, right half ----
            if (gd.bodyCount > 0)
            {
                const BodyDebug& b = gd.bodies[0];
                POINT ppos[NUI_SKELETON_POSITION_COUNT];
                ProjectBody(b, g_cfg.mirror, (W * 3) / 4, cyHip, scale, ppos);
                COLORREF col; const wchar_t* lbl; RoleStyle(b.role, col, lbl);
                if (b.role == 3) col = gd.dpadCmd ? cMag : (gd.dpadWedge && !gd.dpadParked ? cCyn : cGrn);
                DrawSkeleton(mem, b, ppos, col, 3);
                if (b.role == 3)
                {
                    HFONT fLb = MakeFont(15, false);
                    DrawDpadOverlay(mem, ppos, gd, g_cfg, scale, fLb, 2);
                    DeleteObject(fLb);
                }
                PText(mem, fSm, (W * 3) / 4, bandBot + 8, TutStr::T(S::LabelYou), cFnt, TA_CENTER | TA_TOP);
            }
            else
            {
                PText(mem, fBody, (W * 3) / 4, cyHip - 20, TutStr::T(S::LabelStepIntoView), cAmb, TA_CENTER | TA_TOP);
            }

            if (stale)
                PText(mem, fSm, W / 2, dotsY + 12, TutStr::T(S::Disconnected), cAmb, TA_CENTER | TA_TOP);

            PText(mem, fSm, W / 2, H - 30, TutStr::T(S::FooterHint), cFnt, TA_CENTER | TA_TOP);
        }

        BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        DeleteObject(fH1); DeleteObject(fBody); DeleteObject(fSm);
        SelectObject(mem, oldBmp); DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
    }

    LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        switch (m)
        {
        case WM_PAINT: Paint(h); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_KEYDOWN:
            if (w == VK_ESCAPE) { DestroyWindow(h); return 0; }
            if (g_conn != ConnState::Ready) return 0;

            if (w == VK_SPACE) { AdvanceStep(); InvalidateRect(h, nullptr, FALSE); return 0; }
            if (w == VK_BACK)  { if (g_step > 0) { --g_step; g_stepStartMs = NowMs(); } InvalidateRect(h, nullptr, FALSE); return 0; }
            return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(h, m, w, l);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int)
{
    Log::SetEcho(false);
    LogLine("Tutorial: starting");
    Cfg::Load();
    g_cfg = Cfg::Get();

    // The GUI/Console launch this with their currently-selected language code as the sole
    // argument (e.g. "zh-Hans", matching dist/app/lang/*.json's base names) so the tutorial
    // matches whatever the player already picked, with no config surface of its own. Strip
    // any wrapping quotes and stop at the first space; missing/unrecognized -> English.
    wchar_t langBuf[32] = L"";
    if (lpCmdLine && lpCmdLine[0])
    {
        const wchar_t* p = lpCmdLine;
        while (*p == L' ' || *p == L'"') ++p;
        size_t i = 0;
        while (p[i] && p[i] != L' ' && p[i] != L'"' && i < 31) { langBuf[i] = p[i]; ++i; }
        langBuf[i] = L'\0';
    }
    TutStr::SetLang(TutStr::ParseLangCode(langBuf));

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"KinectNavigatorTutorial";
    RegisterClassW(&wc);

    const int W = 960, H = 720;
    RECT wr{ 0, 0, W, H };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    const int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, TutStr::T(S::WindowTitle),
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        (sw - (wr.right - wr.left)) / 2, (sh - (wr.bottom - wr.top)) / 2,
        wr.right - wr.left, wr.bottom - wr.top, nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    wchar_t which[MAX_PATH];
    g_lastBindErr = NuiBridge::Bind(which, MAX_PATH);
    if (g_lastBindErr == NuiBridge::Status::Ok)
    {
        LogLine("Tutorial: Kinect runtime %ls", which);
        g_conn = ConnState::Connecting;
    }
    else
    {
        LogLine("Tutorial: Bind failed (%d) -- no genuine Kinect10.dll found", (int)g_lastBindErr);
        g_conn = ConnState::Waiting;
    }
    g_nextRetryMs = NowMs();

    bool running = true;
    while (running)
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        if (!running) break;

        if (g_conn != ConnState::Ready)
        {
            if (NowMs() >= g_nextRetryMs)
            {
                if (g_lastBindErr != NuiBridge::Status::Ok)
                    g_lastBindErr = NuiBridge::Bind(which, MAX_PATH);

                if (g_lastBindErr == NuiBridge::Status::Ok)
                {
                    NuiBridge::Status ist = NuiBridge::Init();
                    if (ist == NuiBridge::Status::Ok)
                    {
                        g_conn = ConnState::Ready;
                        g_everConnected = true;
                        g_lastFrameTickMs = NowMs();
                        g_step = 0; g_stepStartMs = NowMs();
                        LogLine("Tutorial: sensor ready");
                    }
                    else
                    {
                        g_conn = ConnState::Waiting;
                    }
                }
                g_nextRetryMs = NowMs() + 2000;   // retry every 2 s
            }
            InvalidateRect(g_hwnd, nullptr, FALSE);
            Sleep(50);
            continue;
        }

        NuiBridge::Pump(30, OnFrame, nullptr);

        GestureDebug gd; Gestures::GetDebug(gd);
        if (StepComplete(kSteps[g_step], gd, g_stepStartMs, g_cfg))
            AdvanceStep();

        InvalidateRect(g_hwnd, nullptr, FALSE);
    }

    LogLine("Tutorial: stopping");
    NuiBridge::Unbind();
    return 0;
}
