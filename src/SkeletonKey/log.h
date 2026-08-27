#pragma once

// Appends one timestamped line to SkeletonKey.log, next to this module.
// printf-style, ASCII. Cheap enough for call tracing; each call is an
// independent atomic append so lines from multiple threads never interleave.
void LogLine(const char* fmt, ...);

namespace Log
{
    // When echo is on, every LogLine is also written to stdout. The DLL leaves
    // it off; the replay tool turns it on so recognizer output shows in the
    // terminal.
    void SetEcho(bool on);
}
