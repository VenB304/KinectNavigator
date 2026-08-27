#pragma once

// Appends one timestamped line to SkeletonKey.log, next to this DLL.
// printf-style, ASCII. Cheap enough for M1's call tracing; each call is an
// independent atomic append so lines from multiple threads never interleave.
void LogLine(const char* fmt, ...);
