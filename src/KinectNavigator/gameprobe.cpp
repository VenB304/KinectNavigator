#include "framework.h"
#include "gameprobe.h"
#include "globals.h"
#include "config.h"
#include "log.h"

// Menu-vs-gameplay probes, all observe-only (nothing gates on them yet).
// IAT hooks on the host exe (safe pointer swaps, no code patched):
//   - USER32!GetAsyncKeyState  -> when did the game last poll a menu key
//   - KERNEL32!CreateFileW     -> what game files is it opening (song assets?)
// Plus cheap state the recognizer samples ~1/s: cursor visibility, window rect,
// last tracked-skeleton IDs, which render/input modules are loaded.

// ---- shared IAT walker --------------------------------------------------
namespace
{
    void** FindIat(HMODULE mod, const char* fn)
    {
        BYTE* base = (BYTE*)mod;
        auto* dos = (IMAGE_DOS_HEADER*)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
        auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
        DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        if (!rva) return nullptr;

        for (auto* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + rva); imp->Name; ++imp)
        {
            auto* thunkOrig = (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
            auto* thunkIat  = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
            for (; thunkOrig->u1.AddressOfData; ++thunkOrig, ++thunkIat)
            {
                if (thunkOrig->u1.Ordinal & IMAGE_ORDINAL_FLAG) continue;
                auto* nm = (IMAGE_IMPORT_BY_NAME*)(base + thunkOrig->u1.AddressOfData);
                if (strcmp((const char*)nm->Name, fn) == 0)
                    return (void**)&thunkIat->u1.Function;
            }
        }
        return nullptr;
    }

    bool PatchSlot(void** slot, void* repl, void*& savedOrig)
    {
        savedOrig = *slot;
        DWORD old = 0;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        *slot = repl;
        VirtualProtect(slot, sizeof(void*), old, &old);
        return true;
    }
    void UnpatchSlot(void** slot, void* orig)
    {
        DWORD old = 0;
        if (VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old))
        {
            *slot = orig;
            VirtualProtect(slot, sizeof(void*), old, &old);
        }
    }
}

// ---- GetAsyncKeyState --------------------------------------------------
namespace
{
    typedef SHORT (WINAPI *PFN_GAKS)(int);
    PFN_GAKS s_gaks = nullptr;
    void**   s_gaksSlot = nullptr;

    bool IsMenuKey(int vk)
    {
        switch (vk)
        {
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_RETURN: case VK_ESCAPE: case VK_SPACE: case VK_BACK:
        case 'A': case 'D': case 'W': case 'S': return true;
        }
        return false;
    }

    SHORT WINAPI Hook_GAKS(int vKey)
    {
        if (IsMenuKey(vKey))
        {
            InterlockedExchange64((volatile LONGLONG*)&g_menuPollTick, (LONGLONG)GetTickCount64());
            InterlockedExchange(&g_menuPollVk, (long)vKey);
        }
        return s_gaks ? s_gaks(vKey) : (SHORT)0;
    }
}

// ---- CreateFileW -----------------------------------------------------
namespace
{
    typedef HANDLE (WINAPI *PFN_CFW)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    PFN_CFW s_cfw = nullptr;
    void**  s_cfwSlot = nullptr;

    bool Interesting(const wchar_t* p)
    {
        if (!p) return false;
        wchar_t low[MAX_PATH];
        int i = 0;
        for (; p[i] && i < MAX_PATH - 1; ++i) low[i] = (wchar_t)towlower(p[i]);
        low[i] = 0;
        if (wcsstr(low, L"\\windows\\"))       return false;
        if (wcsstr(low, L"\\program files"))   return false;
        if (wcsstr(low, L"\\programdata\\"))   return false;
        if (wcsstr(low, L"kinectnavigator"))       return false;   // our own log
        if (wcsstr(low, L"\\\\.\\"))           return false;   // device paths
        if (wcsstr(low, L"\\\\?\\"))           return false;
        return true;
    }

    bool IsSongBundle(const wchar_t* p, const wchar_t*& baseOut)
    {
        const wchar_t* b = wcsrchr(p, L'\\'); b = b ? b + 1 : p;
        baseOut = b;
        size_t n = wcslen(b);
        if (n < 8) return false;
        if (_wcsicmp(b + n - 7, L"_pc.ipk") != 0) return false;
        // must sit under a \maps\ directory (per-song bundle, not the root bundle_pc.ipk)
        wchar_t low[MAX_PATH]; int i = 0;
        for (; p[i] && i < MAX_PATH - 1; ++i) low[i] = (wchar_t)towlower(p[i]);
        low[i] = 0;
        return wcsstr(low, L"\\maps\\") != nullptr || wcsncmp(low, L"maps\\", 5) == 0;
    }

    void Note(const wchar_t* name)
    {
        if (!name || !Interesting(name)) return;
        wcsncpy_s(g_lastFile, 260, name, _TRUNCATE);
        InterlockedIncrement64((volatile LONGLONG*)&g_fileOpens);
        const LONGLONG now = (LONGLONG)GetTickCount64();
        InterlockedExchange64((volatile LONGLONG*)&g_lastOpenTick, now);

        // "song is loading" = the SAME maps\<yr>\<song>_pc.ipk opened in a rapid burst
        // (the game reopens it several times when you press play). A carousel scroll
        // opens each bundle once, so browsing never trips this.
        // "song is loading" = the SAME maps\<yr>\<song>_pc.ipk opened songBurstCount times
        // in a row with NOTHING else opened between. Coach-select etc. reopen a bundle a
        // few times but interspersed with other assets, so they never accumulate a run.
        const wchar_t* base = nullptr;
        static wchar_t  s_song[64] = L"";
        static int      s_count = 0;
        static LONGLONG s_first = 0;
        if (IsSongBundle(name, base))
        {
            const Config& c = Cfg::Get();
            if (_wcsicmp(base, s_song) != 0 || now - s_first > c.songBurstMs)
            { wcsncpy_s(s_song, 64, base, _TRUNCATE); s_count = 1; s_first = now; }
            else if (++s_count >= c.songBurstCount)
            {
                InterlockedExchange64((volatile LONGLONG*)&g_songLoadTick, now);
                s_count = 0;
            }
        }
        else { s_count = 0; s_song[0] = 0; }   // any other file breaks the run
        static volatile LONG guard = 0;
        if (InterlockedCompareExchange(&guard, 1, 0) == 0)
        {
            static DWORD lastMs = 0;
            DWORD now = GetTickCount();
            if (now - lastMs > 60) { lastMs = now; LogLine("GameProbe/open: %ls", name); }
            InterlockedExchange(&guard, 0);
        }
    }

    HANDLE WINAPI Hook_CFW(LPCWSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa,
                           DWORD disp, DWORD flags, HANDLE tmpl)
    {
        Note(name);
        return s_cfw ? s_cfw(name, acc, share, sa, disp, flags, tmpl) : INVALID_HANDLE_VALUE;
    }

    typedef HANDLE (WINAPI *PFN_CFA)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
    PFN_CFA s_cfa = nullptr; void** s_cfaSlot = nullptr;
    HANDLE WINAPI Hook_CFA(LPCSTR name, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa,
                           DWORD disp, DWORD flags, HANDLE tmpl)
    {
        if (name) { wchar_t w[MAX_PATH]; int n = MultiByteToWideChar(CP_ACP, 0, name, -1, w, MAX_PATH);
                    if (n > 0) Note(w); }
        return s_cfa ? s_cfa(name, acc, share, sa, disp, flags, tmpl) : INVALID_HANDLE_VALUE;
    }

    typedef HANDLE (WINAPI *PFN_CF2)(LPCWSTR, DWORD, DWORD, DWORD, void*);
    PFN_CF2 s_cf2 = nullptr; void** s_cf2Slot = nullptr;
    HANDLE WINAPI Hook_CF2(LPCWSTR name, DWORD acc, DWORD share, DWORD disp, void* ext)
    {
        Note(name);
        return s_cf2 ? s_cf2(name, acc, share, disp, ext) : INVALID_HANDLE_VALUE;
    }
}

// ---- install / remove ----------------------------------------------
void GameProbe::Install()
{
    static bool done = false;
    if (done) return;
    done = true;

    HMODULE exe = GetModuleHandleW(nullptr);

    if ((s_gaksSlot = FindIat(exe, "GetAsyncKeyState")) != nullptr)
    {
        void* o = nullptr;
        if (PatchSlot(s_gaksSlot, (void*)&Hook_GAKS, o)) { s_gaks = (PFN_GAKS)o;
            LogLine("GameProbe: GetAsyncKeyState IAT-hooked"); }
    }
    else LogLine("GameProbe: GetAsyncKeyState NOT in exe IAT");

    if ((s_cfwSlot = FindIat(exe, "CreateFileW")) != nullptr)
    {
        void* o = nullptr;
        if (PatchSlot(s_cfwSlot, (void*)&Hook_CFW, o)) { s_cfw = (PFN_CFW)o;
            LogLine("GameProbe: CreateFileW IAT-hooked"); }
    }
    else LogLine("GameProbe: CreateFileW NOT in exe IAT");

    if ((s_cfaSlot = FindIat(exe, "CreateFileA")) != nullptr)
    {
        void* o = nullptr;
        if (PatchSlot(s_cfaSlot, (void*)&Hook_CFA, o)) { s_cfa = (PFN_CFA)o;
            LogLine("GameProbe: CreateFileA IAT-hooked"); }
    }
    if ((s_cf2Slot = FindIat(exe, "CreateFile2")) != nullptr)
    {
        void* o = nullptr;
        if (PatchSlot(s_cf2Slot, (void*)&Hook_CF2, o)) { s_cf2 = (PFN_CF2)o;
            LogLine("GameProbe: CreateFile2 IAT-hooked"); }
    }

    // one-time: which render / input stacks are loaded
    LogLine("GameProbe: modules gl=%d d3d9=%d d3d11=%d dxgi=%d dinput8=%d xinput1_4=%d unity=%d",
            GetModuleHandleW(L"opengl32.dll")  != nullptr,
            GetModuleHandleW(L"d3d9.dll")      != nullptr,
            GetModuleHandleW(L"d3d11.dll")     != nullptr,
            GetModuleHandleW(L"dxgi.dll")      != nullptr,
            GetModuleHandleW(L"dinput8.dll")   != nullptr,
            GetModuleHandleW(L"xinput1_4.dll") != nullptr,
            GetModuleHandleW(L"UnityPlayer.dll") != nullptr);
}

void GameProbe::Remove()
{
    if (s_gaksSlot && s_gaks) { UnpatchSlot(s_gaksSlot, (void*)s_gaks); s_gaksSlot = nullptr; }
    if (s_cfwSlot  && s_cfw)  { UnpatchSlot(s_cfwSlot,  (void*)s_cfw);  s_cfwSlot  = nullptr; }
    if (s_cfaSlot  && s_cfa)  { UnpatchSlot(s_cfaSlot,  (void*)s_cfa);  s_cfaSlot  = nullptr; }
    if (s_cf2Slot  && s_cf2)  { UnpatchSlot(s_cf2Slot,  (void*)s_cf2);  s_cf2Slot  = nullptr; }
}
