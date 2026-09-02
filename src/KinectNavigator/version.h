#pragma once

// KinectNavigator version -- single source of truth.
// Bump this in lockstep with CHANGELOG.md and the GitHub release tag; pass the
// same X.Y to  dist\package.cmd  when building the release zip. The installer
// (dist\KinectNavigator-Setup.ps1) reads FILEVERSION out of the built DLL to
// decide whether the copy in a game folder is older than the one it ships.

#define KN_VER_MAJOR 0
#define KN_VER_MINOR 9
#define KN_VER_PATCH 0
#define KN_VER_BUILD 0

#define KN_STR2(x) #x
#define KN_STR(x)  KN_STR2(x)
#define KN_VER_STRING  KN_STR(KN_VER_MAJOR) "." KN_STR(KN_VER_MINOR) "." KN_STR(KN_VER_PATCH)
