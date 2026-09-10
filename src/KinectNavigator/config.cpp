#include "framework.h"
#include <math.h>
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

    // Keys that existed for the swipe model / song-burst mute, both removed in v1.1. An
    // upgraded kinectnav.ini still carries them; accept and ignore them silently rather than
    // logging "unknown key" for every line (see CHANGELOG v1.1.0).
    bool IsRetiredKey(const char* k)
    {
        if (_strnicmp(k, "swipe_", 6) == 0) return true;
        static const char* const retired[] = {
            "nav_model",
            "up_end_min_y", "up_elbow_margin", "down_start_min_y", "down_end_max_y",
            "left_end_max_x", "right_end_min_x",
            "post_swipe_ms", "arm_extend_frac", "arm_relax_frac", "arm_level_tan_max",
            "repeat_first_ms", "repeat_min_ms", "repeat_accel_ms",
            "confirm_dwell_ms", "confirm_repeat_ms", "confirm_repeat_first_ms",
            "confirm_raise_fy", "confirm_still_vel",
            "back_dwell_ms", "back_repeat_ms", "back_repeat_first_ms",
            "back_out_min", "back_down_min", "back_down_max", "back_still_vel",
            "dwell_grace_frames", "smooth_fast", "smooth_slow",
            "song_burst_count", "song_burst_ms", "gameplay_cap_ms",
        };
        for (const char* r : retired) if (_stricmp(k, r) == 0) return true;
        return false;
    }

    void Apply(const char* key, const char* val)
    {
        if      (KeyIs(key, "enable"))              g_cfg.enable            = atoi(val) != 0;
        else if (KeyIs(key, "mirror"))              g_cfg.mirror            = atoi(val) != 0;
        else if (KeyIs(key, "handedness"))          g_cfg.leftHanded        = (_stricmp(val, "left") == 0 || atoi(val) != 0);
        else if (KeyIs(key, "left_handed"))         g_cfg.leftHanded        = atoi(val) != 0;
        else if (KeyIs(key, "enable_confirm"))      g_cfg.enableConfirm     = atoi(val) != 0;
        else if (KeyIs(key, "enable_back"))         g_cfg.enableBack        = atoi(val) != 0;
        else if (KeyIs(key, "require_foreground"))  g_cfg.requireForeground = atoi(val) != 0;
        else if (KeyIs(key, "dpad_arm"))            g_cfg.dpadArm           = atoi(val) != 0;
        else if (KeyIs(key, "dpad_arm_dwell_ms"))   g_cfg.dpadArmDwellMs    = atoi(val);
        else if (KeyIs(key, "dpad_arm_dwell_ms_ingame")) g_cfg.dpadArmDwellGameplayMs = atoi(val);
        else if (KeyIs(key, "dpad_disarm_ms"))      g_cfg.dpadDisarmMs      = atoi(val);
        else if (KeyIs(key, "dpad_sleep_below_y"))  g_cfg.dpadSleepBelowY   = (float)atof(val);
        else if (KeyIs(key, "dpad_dance_disarm"))   g_cfg.dpadDanceDisarm   = atoi(val) != 0;
        else if (KeyIs(key, "dpad_dance_energy"))   g_cfg.dpadDanceEnergy   = (float)atof(val);
        else if (KeyIs(key, "dpad_dance_hold_ms"))  g_cfg.dpadDanceHoldMs   = atoi(val);
        else if (KeyIs(key, "dpad_park_radius"))    g_cfg.dpadParkRadius    = (float)atof(val);
        else if (KeyIs(key, "dpad_park_exit_k"))    g_cfg.dpadParkExitK     = (float)atof(val);
        else if (KeyIs(key, "dpad_up_reach_k"))     g_cfg.dpadUpReachK      = (float)atof(val);
        else if (KeyIs(key, "dpad_cross_reach_k"))  g_cfg.dpadCrossReachK   = (float)atof(val);
        else if (KeyIs(key, "dpad_wedge_half_deg")) g_cfg.dpadWedgeHalfDeg  = (float)atof(val);
        else if (KeyIs(key, "dpad_down_min_drop"))  g_cfg.dpadDownMinDrop   = (float)atof(val);
        else if (KeyIs(key, "dpad_down_min_out"))   g_cfg.dpadDownMinOut    = (float)atof(val);
        else if (KeyIs(key, "dpad_entry_debounce")) g_cfg.dpadEntryDebounce = atoi(val);
        else if (KeyIs(key, "dpad_repeat_dwell_ms")) g_cfg.dpadRepeatDwellMs = atoi(val);
        else if (KeyIs(key, "dpad_repeat_first_ms")) g_cfg.dpadRepeatFirstMs = atoi(val);
        else if (KeyIs(key, "dpad_repeat_min_ms"))  g_cfg.dpadRepeatMinMs   = atoi(val);
        else if (KeyIs(key, "dpad_repeat_accel_ms")) g_cfg.dpadRepeatAccelMs = atoi(val);
        else if (KeyIs(key, "dpad_wedge_grace_fr")) g_cfg.dpadWedgeGraceFr  = atoi(val);
        else if (KeyIs(key, "dpad_cmd_gate_radius")) g_cfg.dpadCmdGateRadius = (float)atof(val);
        else if (KeyIs(key, "dpad_cmd_gate_exit_k")) g_cfg.dpadCmdGateExitK  = (float)atof(val);
        else if (KeyIs(key, "dpad_cmd_dwell_ms"))   g_cfg.dpadCmdDwellMs    = atoi(val);
        else if (KeyIs(key, "dpad_back_dwell_ms"))  g_cfg.dpadBackDwellMs   = atoi(val);
        else if (KeyIs(key, "dpad_cmd_repeat_ms"))  g_cfg.dpadCmdRepeatMs   = atoi(val);
        else if (KeyIs(key, "dpad_handoff_grace_ms")) g_cfg.dpadHandoffGraceMs = atoi(val);
        else if (KeyIs(key, "suppress_in_game"))    g_cfg.suppressInGame    = atoi(val) != 0;
        else if (KeyIs(key, "file_idle_gameplay_ms")) g_cfg.fileIdleGameplayMs = atoi(val);

        else if (KeyIs(key, "arm_after_frames"))    g_cfg.armAfterFrames    = atoi(val);
        else if (KeyIs(key, "filter_1e_min_cutoff")) g_cfg.filter1eMinCutoff = (float)atof(val);
        else if (KeyIs(key, "filter_1e_beta"))      g_cfg.filter1eBeta      = (float)atof(val);
        else if (KeyIs(key, "filter_1e_dcutoff"))   g_cfg.filter1eDCutoff   = (float)atof(val);

        else if (KeyIs(key, "key_left"))            g_cfg.keyLeft           = (unsigned)strtoul(val, nullptr, 0);
        else if (KeyIs(key, "key_right"))           g_cfg.keyRight          = (unsigned)strtoul(val, nullptr, 0);
        else if (KeyIs(key, "key_up"))             g_cfg.keyUp             = (unsigned)strtoul(val, nullptr, 0);
        else if (KeyIs(key, "key_down"))           g_cfg.keyDown           = (unsigned)strtoul(val, nullptr, 0);
        else if (KeyIs(key, "key_confirm"))        g_cfg.keyConfirm        = (unsigned)strtoul(val, nullptr, 0);
        else if (KeyIs(key, "key_back"))           g_cfg.keyBack           = (unsigned)strtoul(val, nullptr, 0);
        else if (KeyIs(key, "key_press_ms"))       g_cfg.keyPressMs        = atoi(val);

        else if (KeyIs(key, "trace"))              g_cfg.trace             = atoi(val) != 0;
        else if (KeyIs(key, "overlay"))           g_cfg.overlay           = atoi(val) != 0;
        else if (IsRetiredKey(key))                { /* removed in v1.1 -- accepted, ignored */ }
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
    if (got >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF)
        p += 3;   // skip a UTF-8 BOM (some editors add one)
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

    // Guard the anisotropic park-exit scalers away from 0 / negative (would divide-by-zero or
    // invert the axis). Clamp to a sane band; 1.0 is symmetric / off.
    if (g_cfg.dpadUpReachK    < 0.25f) g_cfg.dpadUpReachK    = 0.25f;
    if (g_cfg.dpadUpReachK    > 4.0f)  g_cfg.dpadUpReachK    = 4.0f;
    if (g_cfg.dpadCrossReachK < 0.25f) g_cfg.dpadCrossReachK = 0.25f;
    if (g_cfg.dpadCrossReachK > 4.0f)  g_cfg.dpadCrossReachK = 4.0f;

    // precompute the wedge half-angle tangent so the recogniser isn't calling tanf per body per frame
    g_cfg.dpadWedgeTanHalf = tanf(g_cfg.dpadWedgeHalfDeg * 0.01745329f);

    LogLine("Config: loaded kinectnav.ini (%d settings)  hand=%s mirror=%d confirm=%d back=%d  "
            "arm/disarm=%d/%d ms (ingame arm %d)  park r=%.2f  back dwell=%d ms",
            applied, g_cfg.leftHanded ? "L" : "R", g_cfg.mirror, g_cfg.enableConfirm, g_cfg.enableBack,
            g_cfg.dpadArmDwellMs, g_cfg.dpadDisarmMs, g_cfg.dpadArmDwellGameplayMs,
            g_cfg.dpadParkRadius, g_cfg.dpadBackDwellMs);
    g_loaded = true;
    return g_cfg;
}

const Config& Cfg::Get()
{
    if (!g_loaded) return Cfg::Load();
    return g_cfg;
}
