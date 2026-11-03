#include "framework.h"
#include "rich_convert.h"

const int RichConvert::kV1ToRich[NUI_SKELETON_POSITION_COUNT] = {
    KN_J_SPINE_BASE,      // 0  HIP_CENTER
    KN_J_SPINE_MID,       // 1  SPINE
    KN_J_SPINE_SHOULDER,  // 2  SHOULDER_CENTER  (NOT KN_J_NECK)
    KN_J_HEAD,            // 3  HEAD
    KN_J_SHOULDER_L,      // 4
    KN_J_ELBOW_L,         // 5
    KN_J_WRIST_L,         // 6
    KN_J_HAND_L,          // 7
    KN_J_SHOULDER_R,      // 8
    KN_J_ELBOW_R,         // 9
    KN_J_WRIST_R,         // 10
    KN_J_HAND_R,          // 11
    KN_J_HIP_L,           // 12
    KN_J_KNEE_L,          // 13
    KN_J_ANKLE_L,         // 14
    KN_J_FOOT_L,          // 15
    KN_J_HIP_R,           // 16
    KN_J_KNEE_R,          // 17
    KN_J_ANKLE_R,         // 18
    KN_J_FOOT_R,          // 19
};

void RichConvert::DeriveRich(const NUI_SKELETON_FRAME& f, KN_RichFrame& out)
{
    memset(&out, 0, sizeof(out));
    out.schema = KN_RICH_SCHEMA;
    out.source = 0;
    out.timestampMs = f.liTimeStamp.QuadPart;
    out.frameNumber = f.dwFrameNumber;
    out.floorClipPlane[0] = f.vFloorClipPlane.x;
    out.floorClipPlane[1] = f.vFloorClipPlane.y;
    out.floorClipPlane[2] = f.vFloorClipPlane.z;
    out.floorClipPlane[3] = f.vFloorClipPlane.w;

    for (int i = 0; i < NUI_SKELETON_COUNT; ++i)
    {
        const NUI_SKELETON_DATA& s = f.SkeletonData[i];
        if (s.eTrackingState != NUI_SKELETON_TRACKED) continue;

        KN_RichBody& b = out.body[out.bodyCount++];
        b.trackingId = s.dwTrackingID;
        b.tracked = 1;
        for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j)
        {
            KN_RichJoint& o = b.joint[kV1ToRich[j]];
            o.x = s.SkeletonPositions[j].x;
            o.y = s.SkeletonPositions[j].y;
            o.z = s.SkeletonPositions[j].z;
            o.state = (unsigned char)s.eSkeletonPositionTrackingState[j];
        }
        b.handLeft = b.handRight = KN_HAND_UNKNOWN;
    }
}

void RichConvert::RichBodyToV1Joints(const KN_RichBody& b, Vector4 pos[NUI_SKELETON_POSITION_COUNT],
                                     NUI_SKELETON_POSITION_TRACKING_STATE st[NUI_SKELETON_POSITION_COUNT])
{
    for (int j = 0; j < NUI_SKELETON_POSITION_COUNT; ++j)
    {
        const KN_RichJoint& s = b.joint[kV1ToRich[j]];
        pos[j].x = s.x; pos[j].y = s.y; pos[j].z = s.z; pos[j].w = 1.0f;
        st[j] = (NUI_SKELETON_POSITION_TRACKING_STATE)s.state;
    }
}
