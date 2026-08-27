#pragma once
#include "nui_types.h"

// Captures raw NUI_SKELETON_FRAME records to <module dir>\skcap-<timestamp>.skcap
// for offline replay. Only active when a file named "record.flag" sits next to
// the module (dist\record.bat toggles it). Frames are written back-to-back after
// a 24-byte header; timing is reconstructed on replay from frame.liTimeStamp.

namespace Recorder
{
    void Init();                               // check record.flag, open the file
    void Write(const NUI_SKELETON_FRAME& f);   // append one frame (no-op if inactive)
    void Close();

    // Layout of the file header (also used by the replay tool).
#pragma pack(push, 1)
    struct Header
    {
        char               magic[8];        // "SKCAP01\n"
        uint32_t           frameSize;       // sizeof(NUI_SKELETON_FRAME) sanity check
        uint32_t           reserved;
        uint64_t           startFileTime;   // FILETIME when capture began
    };
#pragma pack(pop)
    static_assert(sizeof(Header) == 24, "capture header must be 24 bytes");
}
