#include "framework.h"
#include "config.h"
#include "globals.h"
#include "log.h"

namespace
{
    Config g_cfg;
    bool   g_loaded = false;

    void Trim(char*& s, char* end)
    {
        while (s < end && (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')) ++s;
        while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) --end;
        *end = '\0';
    }

    bool KeyIs(const char* k, const char* name) { return _stricmp(k, name) == 0; }

    void Apply(const char* key, const char* val)
    {
        if      (KeyIs(key, "enable"))             g_cfg.enable            = atoi(val) != 0;
        else if (KeyIs(key, "mirror"))             g_cfg.mirror            = atoi(val) != 0;
        else if (KeyIs(key, "require_foreground")) g_cfg.requireForeground = atoi(val) != 0;
        else if (KeyIs(key, "swipe_velocity"))     g_cfg.swipeVelocity     = (float)atof(val);
        else if (KeyIs(key, "swipe_distance"))     g_cfg.swipeDistance     = (float)atof(val);
        else if (KeyIs(key, "swipe_min_frames"))   g_cfg.swipeMinFrames    = atoi(val);
        else if (KeyIs(key, "cooldown_ms"))        g_cfg.cooldownMs        = atoi(val);
        else if (KeyIs(key, "neutral_hold_ms"))    g_cfg.neutralHoldMs     = atoi(val);
        else if (KeyIs(key, "arm_after_frames"))   g_cfg.armAfterFrames    = atoi(val);
        else if (KeyIs(key, "smooth_fast"))        g_cfg.smoothFast        = (float)atof(val);
        else if (KeyIs(key, "smooth_slow"))        g_cfg.smoothSlow        = (float)atof(val);
        else if (KeyIs(key, "key_right"))          g_cfg.keyRight          = (unsigned)strtoul(val, nullptr, 0);
        else if (KeyIs(key, "key_press_ms"))       g_cfg.keyPressMs        = atoi(val);
        else if (KeyIs(key, "trace"))              g_cfg.trace             = atoi(val) != 0;
        else LogLine("Config: unknown key '%s' ignored", key);
    }
}

const Config& Cfg::Load()
{
    g_cfg = Config{};              // reset to defaults

    wchar_t path[MAX_PATH];
    GetSelfDir(path, MAX_PATH);
    if (path[0] == L'\0') { g_loaded = true; return g_cfg; }
    wcscat_s(path, MAX_PATH, L"kinectnav.ini");

    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
    {
        LogLine("Config: no kinectnav.ini -- using defaults");
        g_loaded = true;
        return g_cfg;
    }

    char buf[8192];
    DWORD got = 0;
    ReadFile(h, buf, sizeof(buf) - 1, &got, nullptr);
    CloseHandle(h);
    buf[got] = '\0';

    int applied = 0;
    char* p = buf;
    while (*p)
    {
        char* eol = p;
        while (*eol && *eol != '\n') ++eol;
        char saved = *eol;
        *eol = '\0';

        char* line = p;
        while (*line == ' ' || *line == '\t') ++line;
        if (*line && *line != '#' && *line != ';')
        {
            char* eq = strchr(line, '=');
            if (eq)
            {
                *eq = '\0';
                char* key = line;   char* keyEnd = eq;
                char* val = eq + 1; char* valEnd = eq + 1 + (int)strlen(eq + 1);
                Trim(key, keyEnd);
                Trim(val, valEnd);
                if (*key) { Apply(key, val); ++applied; }
            }
        }

        if (saved == '\0') break;
        p = eol + 1;
    }

    LogLine("Config: loaded kinectnav.ini (%d settings)  swipe v=%.2f d=%.2f  mirror=%d  cooldown=%d",
            applied, g_cfg.swipeVelocity, g_cfg.swipeDistance, g_cfg.mirror, g_cfg.cooldownMs);
    g_loaded = true;
    return g_cfg;
}

const Config& Cfg::Get()
{
    if (!g_loaded) return Cfg::Load();
    return g_cfg;
}
