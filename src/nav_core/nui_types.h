#pragma once
#include "framework.h"

// Minimal vendored subset of the Kinect for Windows SDK 1.8 skeleton types.
// We do not include <NuiSkeleton.h> directly: it also declares NuiSkeletonGetNextFrame
// / NuiShutdown / etc. as __declspec(dllimport), which would clash with the exports we
// define in exports.cpp. These structs are ABI-frozen (SDK 1.8 is end-of-life); the
// static_asserts at the bottom guarantee the layout matches what the game fills in.
//
// Layout reference: SDK 1.8 NuiSkeleton.h -- the whole NUI API is #pragma pack(8),
// NUI_SKELETON_FRAME is separately pack(16); both resolve to identical offsets here
// because the largest member alignment is 8 (LARGE_INTEGER).

#pragma pack(push, 8)

typedef struct _Vector4 {
    FLOAT x;
    FLOAT y;
    FLOAT z;
    FLOAT w;
} Vector4;

typedef enum _NUI_SKELETON_POSITION_INDEX {
    NUI_SKELETON_POSITION_HIP_CENTER = 0,
    NUI_SKELETON_POSITION_SPINE,
    NUI_SKELETON_POSITION_SHOULDER_CENTER,
    NUI_SKELETON_POSITION_HEAD,
    NUI_SKELETON_POSITION_SHOULDER_LEFT,
    NUI_SKELETON_POSITION_ELBOW_LEFT,
    NUI_SKELETON_POSITION_WRIST_LEFT,
    NUI_SKELETON_POSITION_HAND_LEFT,
    NUI_SKELETON_POSITION_SHOULDER_RIGHT,
    NUI_SKELETON_POSITION_ELBOW_RIGHT,
    NUI_SKELETON_POSITION_WRIST_RIGHT,
    NUI_SKELETON_POSITION_HAND_RIGHT,
    NUI_SKELETON_POSITION_HIP_LEFT,
    NUI_SKELETON_POSITION_KNEE_LEFT,
    NUI_SKELETON_POSITION_ANKLE_LEFT,
    NUI_SKELETON_POSITION_FOOT_LEFT,
    NUI_SKELETON_POSITION_HIP_RIGHT,
    NUI_SKELETON_POSITION_KNEE_RIGHT,
    NUI_SKELETON_POSITION_ANKLE_RIGHT,
    NUI_SKELETON_POSITION_FOOT_RIGHT,
    NUI_SKELETON_POSITION_COUNT
} NUI_SKELETON_POSITION_INDEX;

typedef enum _NUI_SKELETON_POSITION_TRACKING_STATE {
    NUI_SKELETON_POSITION_NOT_TRACKED = 0,
    NUI_SKELETON_POSITION_INFERRED,
    NUI_SKELETON_POSITION_TRACKED
} NUI_SKELETON_POSITION_TRACKING_STATE;

typedef enum _NUI_SKELETON_TRACKING_STATE {
    NUI_SKELETON_NOT_TRACKED = 0,
    NUI_SKELETON_POSITION_ONLY,
    NUI_SKELETON_TRACKED
} NUI_SKELETON_TRACKING_STATE;

#define NUI_SKELETON_COUNT 6
#define NUI_SKELETON_MAX_TRACKED_COUNT 2
#define NUI_SKELETON_INVALID_TRACKING_ID 0

typedef struct _NUI_SKELETON_DATA {
    NUI_SKELETON_TRACKING_STATE          eTrackingState;
    DWORD                               dwTrackingID;
    DWORD                               dwEnrollmentIndex;
    DWORD                               dwUserIndex;
    Vector4                             Position;
    Vector4                             SkeletonPositions[NUI_SKELETON_POSITION_COUNT];
    NUI_SKELETON_POSITION_TRACKING_STATE eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_COUNT];
    DWORD                               dwQualityFlags;
} NUI_SKELETON_DATA;

typedef struct _NUI_SKELETON_FRAME {
    LARGE_INTEGER     liTimeStamp;
    DWORD             dwFrameNumber;
    DWORD             dwFlags;
    Vector4           vFloorClipPlane;
    Vector4           vNormalToGravity;
    NUI_SKELETON_DATA SkeletonData[NUI_SKELETON_COUNT];
} NUI_SKELETON_FRAME;

#pragma pack(pop)

static_assert(sizeof(Vector4) == 16, "Vector4 layout mismatch");
static_assert(sizeof(NUI_SKELETON_DATA) == 436, "NUI_SKELETON_DATA layout mismatch");
static_assert(sizeof(NUI_SKELETON_FRAME) == 2664, "NUI_SKELETON_FRAME layout mismatch");
