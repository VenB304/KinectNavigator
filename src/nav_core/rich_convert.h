#pragma once
#include "nui_types.h"
#include "rich_types.h"

// Conversions between the v1 skeleton frame and the shared v2-shaped KN_RichFrame.
// v1 is mapped UP into the 25-joint shape (not v2 down to 20): see
// docs/notes/kinect-v2-joint-mapping-2026-09-19.md. The one trap: v1's
// SHOULDER_CENTER (index 2) lands in slot 20 (SPINE_SHOULDER), NOT slot 2 (NECK).

namespace RichConvert
{
    // v1 joint index -> KN_JointIndex slot (see above). 20 entries.
    extern const int kV1ToRich[NUI_SKELETON_POSITION_COUNT];

    // Maps every TRACKED skeleton of `f` (compacted, original slot order preserved)
    // into `out`. Neck, hand tips and thumbs are left untracked; hand states
    // Unknown. Tracking ids are widened unchanged.
    void DeriveRich(const NUI_SKELETON_FRAME& f, KN_RichFrame& out);

    // The inverse for the HUD, which still draws v1-shaped joint arrays.
    void RichBodyToV1Joints(const KN_RichBody& b, Vector4 pos[NUI_SKELETON_POSITION_COUNT],
                            NUI_SKELETON_POSITION_TRACKING_STATE st[NUI_SKELETON_POSITION_COUNT]);

    // 32-bit gesture-layer id for a body. v1 ids pass through unchanged; v2 ids are
    // 64-bit (2^56 + a small counter), so the low 32 bits are the discriminating part.
    inline unsigned long BodyId32(unsigned long long trackingId)
    {
        unsigned long id = (unsigned long)(trackingId & 0xFFFFFFFFull);
        return id ? id : 1ul;
    }
}
