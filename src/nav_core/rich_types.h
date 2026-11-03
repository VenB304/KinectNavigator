#pragma once

// Sensor-agnostic "rich" body frame: the Kinect v2 body shape (25 joints, hand
// state, lean). The v2 proxy fills it natively; the v1 path will derive it by
// mapping v1's 20 joints up into the same slots. Plain data, no SDK headers, so
// every project (proxies, Replay, tools) can include it.
//
// Joint index order is the v2 JointType order. NOTE the v1 trap: v1's
// SHOULDER_CENTER (index 2) maps to slot 20 (SPINE_SHOULDER), not slot 2 (NECK).
// See docs/notes/kinect-v2-joint-mapping-2026-09-19.md.

#pragma pack(push, 8)

enum KN_HandState  { KN_HAND_UNKNOWN = 0, KN_HAND_NOTTRACKED, KN_HAND_OPEN, KN_HAND_CLOSED, KN_HAND_LASSO };
enum KN_Confidence { KN_CONF_LOW = 0, KN_CONF_HIGH };

enum KN_JointIndex
{
    KN_J_SPINE_BASE = 0, KN_J_SPINE_MID, KN_J_NECK, KN_J_HEAD,
    KN_J_SHOULDER_L, KN_J_ELBOW_L, KN_J_WRIST_L, KN_J_HAND_L,
    KN_J_SHOULDER_R, KN_J_ELBOW_R, KN_J_WRIST_R, KN_J_HAND_R,
    KN_J_HIP_L, KN_J_KNEE_L, KN_J_ANKLE_L, KN_J_FOOT_L,
    KN_J_HIP_R, KN_J_KNEE_R, KN_J_ANKLE_R, KN_J_FOOT_R,
    KN_J_SPINE_SHOULDER,
    KN_J_HANDTIP_L, KN_J_THUMB_L, KN_J_HANDTIP_R, KN_J_THUMB_R,
    KN_JOINT_COUNT
};

struct KN_RichJoint
{
    float x, y, z;               // camera space, metres (v2 CameraSpacePoint)
    float qx, qy, qz, qw;        // orientation quaternion (zero for derived-from-v1)
    unsigned char state;         // 0 not tracked / 1 inferred / 2 tracked
};

struct KN_RichBody
{
    unsigned long long trackingId;      // full v2 width
    unsigned char      tracked;
    KN_RichJoint       joint[KN_JOINT_COUNT];
    unsigned char      handLeft,  handLeftConf;
    unsigned char      handRight, handRightConf;
    float              leanX, leanY;    // -1..1
    unsigned char      leanState;       // 0 not tracked / 1 inferred / 2 tracked
    unsigned char      clippedEdges;    // v2 FrameEdges bitmask
};

#define KN_RICH_SCHEMA 1
#define KN_RICH_MAX_BODIES 6

struct KN_RichFrame
{
    unsigned int schema;                // = KN_RICH_SCHEMA
    unsigned int source;                // 0 = derived-from-v1, 1 = native-v2
    long long    timestampMs;
    unsigned int frameNumber;
    float        floorClipPlane[4];
    unsigned int bodyCount;             // tracked bodies in body[], 0..6
    KN_RichBody  body[KN_RICH_MAX_BODIES];
};

#pragma pack(pop)
