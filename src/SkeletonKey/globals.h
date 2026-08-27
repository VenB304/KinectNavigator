#pragma once
#include "framework.h"

// Handle of this DLL, set in DllMain.
extern HMODULE g_hSelf;

// Writes the directory containing this DLL (with trailing backslash) into `out`.
// On failure `out` is set to an empty string.
void GetSelfDir(wchar_t* out, size_t cch);
