#include "framework.h"
#include "skeleton_render.h"

namespace SkelRender
{
    const COLORREF cPanel = RGB(0x16, 0x1B, 0x22), cBord = RGB(0x2C, 0x33, 0x3D);
    const COLORREF cTxt = RGB(0xE6, 0xEC, 0xF2), cDim = RGB(0x87, 0x94, 0xA0), cFnt = RGB(0x5F, 0x6B, 0x78);
    const COLORREF cGrn = RGB(0x5C, 0xE0, 0x8A), cAmb = RGB(0xF0, 0xC3, 0x46);
    const COLORREF cCyn = RGB(0x46, 0xD2, 0xEB), cMag = RGB(0xE6, 0x78, 0xEB);
    const COLORREF cGrid = RGB(0x2A, 0x32, 0x3C), cTrk = RGB(0x23, 0x2A, 0x33);
    const COLORREF cAsleep = RGB(0x5A, 0x62, 0x6C), cDemoted = RGB(0x8A, 0x52, 0x52);
    const COLORREF cHandDot = RGB(0xFF, 0xD9, 0x66);

    void PText(HDC dc, HFONT f, int x, int y, const wchar_t* s, COLORREF col, UINT al)
    {
        SelectObject(dc, f); SetTextAlign(dc, al); SetTextColor(dc, col);
        TextOutW(dc, x, y, s, (int)wcslen(s));
    }
    void FillRectC(HDC dc, int x, int y, int w, int h, COLORREF col)
    {
        HBRUSH br = CreateSolidBrush(col); RECT r = { x, y, x + w, y + h };
        FillRect(dc, &r, br); DeleteObject(br);
    }

    namespace
    {
        inline bool JointOk(NUI_SKELETON_POSITION_TRACKING_STATE s) { return s != NUI_SKELETON_POSITION_NOT_TRACKED; }

        // Bone chain: hip->spine->shoulder->head, both arms off the shoulder centre, both legs
        // off the hip centre. Matches NUI_SKELETON_POSITION_* order in nui_types.h.
        const int kBones[][2] = {
            { NUI_SKELETON_POSITION_HIP_CENTER,      NUI_SKELETON_POSITION_SPINE },
            { NUI_SKELETON_POSITION_SPINE,            NUI_SKELETON_POSITION_SHOULDER_CENTER },
            { NUI_SKELETON_POSITION_SHOULDER_CENTER,  NUI_SKELETON_POSITION_HEAD },
            { NUI_SKELETON_POSITION_SHOULDER_CENTER,  NUI_SKELETON_POSITION_SHOULDER_LEFT },
            { NUI_SKELETON_POSITION_SHOULDER_LEFT,    NUI_SKELETON_POSITION_ELBOW_LEFT },
            { NUI_SKELETON_POSITION_ELBOW_LEFT,       NUI_SKELETON_POSITION_WRIST_LEFT },
            { NUI_SKELETON_POSITION_WRIST_LEFT,       NUI_SKELETON_POSITION_HAND_LEFT },
            { NUI_SKELETON_POSITION_SHOULDER_CENTER,  NUI_SKELETON_POSITION_SHOULDER_RIGHT },
            { NUI_SKELETON_POSITION_SHOULDER_RIGHT,   NUI_SKELETON_POSITION_ELBOW_RIGHT },
            { NUI_SKELETON_POSITION_ELBOW_RIGHT,      NUI_SKELETON_POSITION_WRIST_RIGHT },
            { NUI_SKELETON_POSITION_WRIST_RIGHT,      NUI_SKELETON_POSITION_HAND_RIGHT },
            { NUI_SKELETON_POSITION_HIP_CENTER,       NUI_SKELETON_POSITION_HIP_LEFT },
            { NUI_SKELETON_POSITION_HIP_LEFT,         NUI_SKELETON_POSITION_KNEE_LEFT },
            { NUI_SKELETON_POSITION_KNEE_LEFT,        NUI_SKELETON_POSITION_ANKLE_LEFT },
            { NUI_SKELETON_POSITION_ANKLE_LEFT,       NUI_SKELETON_POSITION_FOOT_LEFT },
            { NUI_SKELETON_POSITION_HIP_CENTER,       NUI_SKELETON_POSITION_HIP_RIGHT },
            { NUI_SKELETON_POSITION_HIP_RIGHT,        NUI_SKELETON_POSITION_KNEE_RIGHT },
            { NUI_SKELETON_POSITION_KNEE_RIGHT,       NUI_SKELETON_POSITION_ANKLE_RIGHT },
            { NUI_SKELETON_POSITION_ANKLE_RIGHT,      NUI_SKELETON_POSITION_FOOT_RIGHT },
        };
        const int kNumBones = sizeof(kBones) / sizeof(kBones[0]);
    }

    void ProjectBody(const BodyDebug& b, bool mirror, int cx, int cyHip, float scale,
                     POINT pos[NUI_SKELETON_POSITION_COUNT])
    {
        const float m = mirror ? -1.f : 1.f;
        const Vector4& hip = b.joints[NUI_SKELETON_POSITION_HIP_CENTER];
        const float torso = b.torso > 0.05f ? b.torso : 0.40f;
        for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j)
        {
            const float ox = m * (b.joints[j].x - hip.x) / torso;
            const float oy = (b.joints[j].y - hip.y) / torso;
            pos[j].x = cx + (int)(ox * scale);
            pos[j].y = cyHip - (int)(oy * scale);
        }
    }

    namespace
    {
        void Dot(HDC dc, POINT p, int r, COLORREF fill, COLORREF outline)
        {
            HBRUSH br = CreateSolidBrush(fill);
            HPEN   pn = CreatePen(PS_SOLID, 1, outline);
            HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pn);
            Ellipse(dc, p.x - r, p.y - r, p.x + r, p.y + r);
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br); DeleteObject(pn);
        }

        // Every joint except the head (drawn separately, bigger) and hands (drawn separately,
        // highlighted) -- the plain structural points that make the figure read as a body with
        // real joints instead of a silhouette of lines.
        const int kDotJoints[] = {
            NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_SHOULDER_CENTER,
            NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_WRIST_LEFT,
            NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_WRIST_RIGHT,
            NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_HIP_RIGHT,
            NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_KNEE_RIGHT,
            NUI_SKELETON_POSITION_ANKLE_LEFT, NUI_SKELETON_POSITION_ANKLE_RIGHT,
            NUI_SKELETON_POSITION_FOOT_LEFT, NUI_SKELETON_POSITION_FOOT_RIGHT,
        };
        const int kNumDotJoints = sizeof(kDotJoints) / sizeof(kDotJoints[0]);
    }

    void DrawSkeleton(HDC dc, const BodyDebug& b, const POINT pos[NUI_SKELETON_POSITION_COUNT],
                      COLORREF col, int weight)
    {
        for (int i = 0; i < kNumBones; ++i)
        {
            const int a = kBones[i][0], z = kBones[i][1];
            const bool ok = JointOk(b.jointState[a]) && JointOk(b.jointState[z]);
            HPEN p = CreatePen(PS_SOLID, weight, ok ? col : cFnt);
            HGDIOBJ op = SelectObject(dc, p);
            MoveToEx(dc, pos[a].x, pos[a].y, nullptr);
            LineTo(dc, pos[z].x, pos[z].y);
            SelectObject(dc, op); DeleteObject(p);
        }

        // joint points -- reads as an actual body, not just a stick silhouette, and pins down
        // exactly where the elbow/shoulder/etc. is instead of leaving it to the eye.
        const COLORREF outline = RGB(0x0B, 0x0F, 0x14);
        for (int i = 0; i < kNumDotJoints; ++i)
        {
            const int j = kDotJoints[i];
            if (!JointOk(b.jointState[j])) continue;
            Dot(dc, pos[j], weight, col, outline);
        }

        // hands: bigger + a fixed warm highlight, always -- they're what every gesture is
        // actually about, so they should pop regardless of the body's role colour.
        const int hrHand = weight + 3;
        if (JointOk(b.jointState[NUI_SKELETON_POSITION_HAND_LEFT]))
            Dot(dc, pos[NUI_SKELETON_POSITION_HAND_LEFT], hrHand, cHandDot, outline);
        if (JointOk(b.jointState[NUI_SKELETON_POSITION_HAND_RIGHT]))
            Dot(dc, pos[NUI_SKELETON_POSITION_HAND_RIGHT], hrHand, cHandDot, outline);

        const int hr = weight + 3;
        const POINT& hd = pos[NUI_SKELETON_POSITION_HEAD];
        HBRUSH hb = CreateSolidBrush(col);
        HGDIOBJ ohb = SelectObject(dc, hb), ohp = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, hd.x - hr, hd.y - hr, hd.x + hr, hd.y + hr);
        SelectObject(dc, ohb); SelectObject(dc, ohp); DeleteObject(hb);
    }

    void DrawDpadOverlay(HDC dc, const POINT pos[NUI_SKELETON_POSITION_COUNT],
                         const GestureDebug& d, const Config& c, float scale, HFONT fLb, int lineW)
    {
        const int domShJ = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_LEFT  : NUI_SKELETON_POSITION_SHOULDER_RIGHT;
        const int ndShJ   = c.leftHanded ? NUI_SKELETON_POSITION_SHOULDER_RIGHT : NUI_SKELETON_POSITION_SHOULDER_LEFT;
        const int gx = pos[domShJ].x, gy = pos[domShJ].y;
        const COLORREF live = d.dpadCmd ? cMag : cCyn;

        const float upK    = d.dpadUpReachK    > 0.01f ? d.dpadUpReachK    : 1.f;
        const float crossK = d.dpadCrossReachK > 0.01f ? d.dpadCrossReachK : 1.f;
        const int prR = (int)(d.dpadParkR * scale), prD = prR;
        const int prU = (int)(d.dpadParkR / upK * scale);
        const int prL = (int)(d.dpadParkR / crossK * scale);

        HPEN pp = CreatePen(PS_SOLID, lineW, d.dpadParked ? RGB(0x6E, 0x7C, 0x8A) : RGB(0x4A, 0x55, 0x60));
        HGDIOBJ opp = SelectObject(dc, pp), opb = SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, gx - prL, gy - prU, gx + prR, gy + prD, 6, 6);
        SelectObject(dc, opp); SelectObject(dc, opb); DeleteObject(pp);

        auto chev = [&](int w, const wchar_t* g, int cxk, int cyk, UINT al) {
            PText(dc, fLb, cxk, cyk, g, d.dpadWedge == w ? live : cFnt, al);
        };
        chev(3, L"\x25B2", gx,               gy - prU - 17, TA_CENTER | TA_TOP);
        chev(4, L"\x25BC", gx,               gy + prD + 3,  TA_CENTER | TA_TOP);
        chev(1, L"\x25B6", gx + prR + 6,      gy - 8,        TA_LEFT   | TA_TOP);
        chev(2, L"\x25C0", gx - prL - 6,      gy - 8,        TA_RIGHT  | TA_TOP);

        // dominant hand: emphasize the joint the skeleton already drew, coloured by state
        const int hJ = c.leftHanded ? NUI_SKELETON_POSITION_HAND_LEFT : NUI_SKELETON_POSITION_HAND_RIGHT;
        const COLORREF hc = d.dpadParked ? RGB(0x96, 0xA0, 0xAA) : d.dpadWedge ? live : RGB(0xD2, 0xB4, 0x5A);
        const int hr = 6;
        HBRUSH hbr = CreateSolidBrush(hc);
        HPEN   hpn = CreatePen(PS_SOLID, lineW, RGB(0x0B, 0x0F, 0x14));
        HGDIOBJ oh1 = SelectObject(dc, hbr), oh2 = SelectObject(dc, hpn);
        Ellipse(dc, pos[hJ].x - hr, pos[hJ].y - hr, pos[hJ].x + hr, pos[hJ].y + hr);
        SelectObject(dc, oh1); SelectObject(dc, oh2); DeleteObject(hbr); DeleteObject(hpn);

        // command gate at the non-dominant shoulder
        const int ngx = pos[ndShJ].x, ngy = pos[ndShJ].y;
        const int gr = (int)(d.dpadCmdGateR * scale);
        HPEN gp = CreatePen(PS_SOLID, lineW, d.dpadCmd ? cMag : RGB(0x4A, 0x55, 0x60));
        HGDIOBJ ogp = SelectObject(dc, gp), ogb = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Ellipse(dc, ngx - gr, ngy - gr, ngx + gr, ngy + gr);
        SelectObject(dc, ogp); SelectObject(dc, ogb); DeleteObject(gp);
    }

    void RoleStyle(int role, COLORREF& col, const wchar_t*& label)
    {
        switch (role)
        {
        case 1: col = cAmb;     label = L"WAKING";  break;   // Arming
        case 2: col = cGrn;     label = L"READY";   break;   // Armed (bystander, could take over)
        case 3: col = cCyn;     label = L"DRIVING"; break;   // Driving (recoloured cyan/magenta by caller)
        case 4: col = cDemoted; label = L"WAIT";     break;   // Demoted
        default: col = cAsleep; label = L"ASLEEP";  break;   // Asleep
        }
    }
}
