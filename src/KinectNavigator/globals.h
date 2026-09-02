#pragma once
#include "framework.h"

// Handle of this DLL, set in DllMain.
extern HMODULE g_hSelf;

// GetTickCount64() of the last NuiImageGetColorPixelCoordinatesFromDepthPixel
// call. The game calls that export ~500/s while a song plays and never in a
// menu, so "set recently" == "in gameplay". The recognizer stays silent then so
// dance moves don't drive the menu. Stays 0 in the offline tools (they never
// call it), which reads as "not in gameplay" -- exactly what we want there.
extern volatile LONGLONG g_lastRenderTick;

// Probe signals for menu-vs-gameplay detection: the eColorResolution argument
// the game last passed to NuiImageGetColorPixel..., and a monotonic count of
// those calls (for computing the call rate). Logged ~1/s by the recognizer.
extern volatile long     g_colorRes;
extern volatile LONGLONG g_colorCalls;

// Menu-vs-gameplay probe signals (gameprobe.cpp / exports.cpp), all observe-only.
extern volatile LONGLONG g_menuPollTick;   // last GetAsyncKeyState poll of a nav key
extern volatile long     g_menuPollVk;     // ...which VK
extern volatile LONGLONG g_fileOpens;      // count of "interesting" game-file opens
extern volatile LONGLONG g_lastOpenTick;    // GetTickCount64() of the last game-file open
extern volatile LONGLONG g_songLoadTick;   // ...of the last "song bundle load" burst (the game
                                            // reopens maps\<yr>\<song>_pc.ipk when you hit play).
                                            // idle + this recent => in a song, not just a static menu.
extern wchar_t           g_lastFile[260];  // last such path (racy telemetry copy)
extern volatile DWORD    g_trkId0, g_trkId1;  // args to the last NuiSkeletonSetTrackedSkeletons

// 1 when the game window is foreground, not minimised, and a normal (gameplay-
// sized) rect; 0 otherwise. Set ~1/s by the recognizer. The gameplay-mute gates
// on this so a stale song-load arm can't mute while the game is minimised /
// alt-tabbed (a song only plays when the game is actually up front). Defaults 1
// so the offline tools are unaffected.
extern volatile long     g_gameWndActive;

// Writes the directory containing this DLL (with trailing backslash) into `out`.
// On failure `out` is set to an empty string.
void GetSelfDir(wchar_t* out, size_t cch);
