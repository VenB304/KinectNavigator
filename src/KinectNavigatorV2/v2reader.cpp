#include "v2_framework.h"
#include "v2reader.h"
#include "rich_types.h"
#include "engine_v2.h"
#include "log.h"

// The nav_core enums are numerically identical to the SDK's; the conversion
// below copies them across as plain integers, so pin that down at compile time.
static_assert(KN_JOINT_COUNT == JointType_Count, "joint count");
static_assert(KN_J_SPINE_SHOULDER == JointType_SpineShoulder, "joint order");
static_assert(KN_J_HANDTIP_R == JointType_HandTipRight, "joint order");
static_assert((int)KN_HAND_LASSO == (int)HandState_Lasso, "hand state");
static_assert((int)KN_HAND_NOTTRACKED == (int)HandState_NotTracked, "hand state");
static_assert((int)KN_CONF_HIGH == (int)TrackingConfidence_High, "confidence");
static_assert((int)TrackingState_Tracked == 2 && (int)TrackingState_Inferred == 1, "tracking state");
static_assert(KN_RICH_MAX_BODIES == BODY_COUNT, "body count");

namespace
{
    template <class T>
    void SafeRelease(T*& p)
    {
        if (p) { p->Release(); p = nullptr; }
    }

    IKinectSensor*    s_sensor = nullptr;
    HANDLE            s_thread = nullptr;
    volatile LONG     s_stop   = 0;

    // Fills `out` from a tracked IBody. Returns false if the SDK refuses any of
    // the core reads (joints), in which case the body is skipped.
    bool ConvertBody(IBody* body, KN_RichBody& out)
    {
        Joint            joints[JointType_Count];
        JointOrientation orient[JointType_Count];
        if (FAILED(body->GetJoints(JointType_Count, joints)))
            return false;
        const bool haveOrient = SUCCEEDED(body->GetJointOrientations(JointType_Count, orient));

        memset(&out, 0, sizeof(out));
        out.tracked = 1;
        UINT64 id = 0;
        body->get_TrackingId(&id);
        out.trackingId = id;

        for (int j = 0; j < JointType_Count; ++j)
        {
            KN_RichJoint& o = out.joint[j];
            o.x = joints[j].Position.X;
            o.y = joints[j].Position.Y;
            o.z = joints[j].Position.Z;
            o.state = (unsigned char)joints[j].TrackingState;
            if (haveOrient)
            {
                o.qx = orient[j].Orientation.x;
                o.qy = orient[j].Orientation.y;
                o.qz = orient[j].Orientation.z;
                o.qw = orient[j].Orientation.w;
            }
        }

        HandState hs = HandState_Unknown;
        TrackingConfidence tc = TrackingConfidence_Low;
        body->get_HandLeftState(&hs);        out.handLeft  = (unsigned char)hs;
        body->get_HandLeftConfidence(&tc);   out.handLeftConf = (unsigned char)tc;
        hs = HandState_Unknown; tc = TrackingConfidence_Low;
        body->get_HandRightState(&hs);       out.handRight = (unsigned char)hs;
        body->get_HandRightConfidence(&tc);  out.handRightConf = (unsigned char)tc;

        PointF lean = {};
        TrackingState ls = TrackingState_NotTracked;
        body->get_Lean(&lean);
        body->get_LeanTrackingState(&ls);
        out.leanX = lean.X;
        out.leanY = lean.Y;
        out.leanState = (unsigned char)ls;

        DWORD clipped = 0;
        body->get_ClippedEdges(&clipped);
        out.clippedEdges = (unsigned char)clipped;
        return true;
    }

    const char* HandName(unsigned char s)
    {
        switch (s)
        {
        case KN_HAND_UNKNOWN:    return "Unknown";
        case KN_HAND_NOTTRACKED: return "NotTracked";
        case KN_HAND_OPEN:       return "Open";
        case KN_HAND_CLOSED:     return "Closed";
        case KN_HAND_LASSO:      return "Lasso";
        default:                 return "?";
        }
    }

    DWORD WINAPI ThreadProc(LPVOID)
    {
        LogLine("V2Reader: thread start, sensor=%p", (void*)s_sensor);

        HRESULT hr = s_sensor->Open();
        LogLine("V2Reader: IKinectSensor::Open -> hr=0x%08lX", hr);

        IBodyFrameSource* pSource = nullptr;
        hr = s_sensor->get_BodyFrameSource(&pSource);
        LogLine("V2Reader: get_BodyFrameSource -> hr=0x%08lX source=%p", hr, (void*)pSource);
        if (FAILED(hr) || !pSource)
        {
            LogLine("V2Reader: no body frame source, thread exiting");
            return 0;
        }

        IBodyFrameReader* pReader = nullptr;
        hr = pSource->OpenReader(&pReader);
        LogLine("V2Reader: OpenReader -> hr=0x%08lX reader=%p", hr, (void*)pReader);
        SafeRelease(pSource);
        if (FAILED(hr) || !pReader)
        {
            LogLine("V2Reader: could not open a second reader, thread exiting");
            return 0;
        }

        LogLine("V2Reader: polling started");

        static KN_RichFrame frame;    // ~6 bodies * ~1.2KB: keep off the thread stack
        unsigned long long accepted = 0;
        unsigned long long windowStart = 0;
        ULONGLONG windowStartTick = GetTickCount64();
        ULONGLONG lastAcceptedTick = windowStartTick;
        bool stallLogged = false;
        unsigned long long prevIds[KN_RICH_MAX_BODIES] = {};
        unsigned prevCount = 0;

        while (!InterlockedCompareExchange(&s_stop, 0, 0))
        {
            IBodyFrame* pFrame = nullptr;
            hr = pReader->AcquireLatestFrame(&pFrame);

            if (SUCCEEDED(hr) && pFrame)
            {
                ++accepted;
                lastAcceptedTick = GetTickCount64();
                stallLogged = false;

                IBody* bodies[BODY_COUNT] = {};
                HRESULT hrBodies = pFrame->GetAndRefreshBodyData(BODY_COUNT, bodies);

                memset(&frame, 0, sizeof(frame));
                frame.schema = KN_RICH_SCHEMA;
                frame.source = 1;
                frame.frameNumber = (unsigned)accepted;
                TIMESPAN rel = 0;
                if (SUCCEEDED(pFrame->get_RelativeTime(&rel)))
                    frame.timestampMs = rel / 10000;
                Vector4 floor = {};
                pFrame->get_FloorClipPlane(&floor);
                frame.floorClipPlane[0] = floor.x; frame.floorClipPlane[1] = floor.y;
                frame.floorClipPlane[2] = floor.z; frame.floorClipPlane[3] = floor.w;
                SafeRelease(pFrame);

                if (SUCCEEDED(hrBodies))
                {
                    for (UINT i = 0; i < BODY_COUNT; ++i)
                    {
                        if (!bodies[i]) continue;
                        BOOLEAN tracked = FALSE;
                        bodies[i]->get_IsTracked(&tracked);
                        if (!tracked) continue;
                        if (ConvertBody(bodies[i], frame.body[frame.bodyCount]))
                            ++frame.bodyCount;
                    }
                }
                for (UINT i = 0; i < BODY_COUNT; ++i)
                    SafeRelease(bodies[i]);

                // Log body acquire / loss (transitions only), then a status line
                // every ~5 s at 30 Hz. Enough to see the reader is alive and the
                // conversion is sane without flooding the file.
                for (unsigned i = 0; i < frame.bodyCount; ++i)
                {
                    bool known = false;
                    for (unsigned k = 0; k < prevCount; ++k)
                        if (prevIds[k] == frame.body[i].trackingId) { known = true; break; }
                    if (!known)
                        LogLine("V2Reader: body ACQUIRED id=%llu (frame #%llu)",
                                frame.body[i].trackingId, accepted);
                }
                for (unsigned k = 0; k < prevCount; ++k)
                {
                    bool still = false;
                    for (unsigned i = 0; i < frame.bodyCount; ++i)
                        if (prevIds[k] == frame.body[i].trackingId) { still = true; break; }
                    if (!still)
                        LogLine("V2Reader: body LOST id=%llu (frame #%llu)", prevIds[k], accepted);
                }
                prevCount = frame.bodyCount;
                for (unsigned i = 0; i < frame.bodyCount; ++i)
                    prevIds[i] = frame.body[i].trackingId;

                if (accepted <= 3 || accepted % 150 == 0)
                {
                    ULONGLONG now = GetTickCount64();
                    double elapsedS = (now - windowStartTick) / 1000.0;
                    double rate = elapsedS > 0.0 ? (accepted - windowStart) / elapsedS : 0.0;
                    windowStart = accepted;
                    windowStartTick = now;

                    if (frame.bodyCount == 0)
                    {
                        LogLine("[v2 frame] #%llu rate=%.1fHz bodies=0", accepted, rate);
                    }
                    else
                    {
                        const KN_RichBody& b = frame.body[0];
                        const KN_RichJoint& sh = b.joint[KN_J_SPINE_SHOULDER];
                        const KN_RichJoint& hr_ = b.joint[KN_J_HAND_R];
                        LogLine("[v2 frame] #%llu rate=%.1fHz bodies=%u id=%llu "
                                "spineShoulder=(%.3f,%.3f,%.3f) rightHand=(%.3f,%.3f,%.3f) "
                                "handStateL=%s handStateR=%s lean=(%.2f,%.2f)",
                                accepted, rate, frame.bodyCount, b.trackingId,
                                sh.x, sh.y, sh.z, hr_.x, hr_.y, hr_.z,
                                HandName(b.handLeft), HandName(b.handRight),
                                b.leanX, b.leanY);
                    }
                }
                // Hand the frame to the recogniser. Every accepted frame goes through,
                // including ones with no tracked body: the gesture layer owns body lifetime
                // and runs its dropout / LOST logic off this clock.
                EngineV2::OnFrame(frame);
            }
            else
            {
                // Not necessarily an error -- AcquireLatestFrame fails whenever no
                // new frame has arrived since the last call. Only worth a log line
                // if a full second passes with nothing (would suggest starvation
                // from the game holding its own reader open on this sensor -- R1).
                ULONGLONG now = GetTickCount64();
                if (!stallLogged && accepted > 0 && (now - lastAcceptedTick) > 1000)
                {
                    LogLine("[v2 stall] no frame for >1s (last hr=0x%08lX, accepted so far=%llu)",
                            hr, accepted);
                    stallLogged = true;
                }
                Sleep(5);
            }
        }

        LogLine("V2Reader: stop requested, %llu frames accepted total", accepted);
        SafeRelease(pReader);
        return 0;
    }
}

namespace V2Reader
{
    void Start(IKinectSensor* sensor)
    {
        if (s_thread)
            return; // already running

        s_sensor = sensor;
        s_stop = 0;
        s_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
        LogLine("V2Reader: Start (thread=%p)", (void*)s_thread);
    }

    void Stop()
    {
        if (!s_thread)
            return;

        InterlockedExchange(&s_stop, 1);
        WaitForSingleObject(s_thread, 2000);
        CloseHandle(s_thread);
        s_thread = nullptr;
        SafeRelease(s_sensor); // release our private AddRef from Start()
        LogLine("V2Reader: Stop complete");
    }
}
