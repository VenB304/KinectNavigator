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
        if      (KeyIs(key, "enable"))              g_cfg.enable            = atoi(val) != 0;
        else if (KeyIs(key, "mirror"))              g_cfg.mirror            = atoi(val) != 0;
        else if (KeyIs(key, "handedness"))          g_cfg.leftHanded        = (_stricmp(val, "left") == 0 || atoi(val) != 0);
        else if (KeyIs(key, "left_handed"))         g_cfg.leftHanded        = atoi(val) != 0;
        else if (KeyIs(key, "enable_confirm"))      g_cfg.enableConfirm     = atoi(val) != 0;
        else if (KeyIs(key, "enable_back"))         g_cfg.enableBack        = atoi(val) != 0;
        else if (KeyIs(key, "require_foreground"))  g_cfg.requireForeground = atoi(val) != 0;
        else if (KeyIs(key, "nav_model"))           g_cfg.navModel          = (_stricmp(val, "swipe") == 0) ? 0
                                                                           : (_stricmp(val, "extend") == 0) ? 1
                                                                           : atoi(val);
        else if (KeyIs(key, "dpad_arm"))            g_cfg.dpadArm           = atoi(val) != 0;
        else if (KeyIs(key, "dpad_arm_dwell_ms"))   g_cfg.dpadArmDwellMs    = atoi(val);
        else if (KeyIs(key, "dpad_disarm_ms"))      g_cfg.dpadDisarmMs      = atoi(val);
        else if (KeyIs(key, "dpad_park_radius"))    g_cfg.dpadParkRadius    = (float)atof(val);
        else if (KeyIs(key, "dpad_park_exit_k"))    g_cfg.dpadParkExitK     = (float)atof(val);
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
        else if (KeyIs(key, "suppress_in_game"))    g_cfg.suppressInGame    = atoi(val) != 0;
        else if (KeyIs(key, "song_burst_count"))    g_cfg.songBurstCount    = atoi(val);
        else if (KeyIs(key, "song_burst_ms"))       g_cfg.songBurstMs       = atoi(val);
        else if (KeyIs(key, "file_idle_gameplay_ms")) g_cfg.fileIdleGameplayMs = atoi(val);
        else if (KeyIs(key, "gameplay_cap_ms"))     g_cfg.gameplayCapMs     = atoi(val);

        else if (KeyIs(key, "swipe_velocity"))      g_cfg.swipeVelocity     = (float)atof(val);
        else if (KeyIs(key, "swipe_vx_ceiling"))    g_cfg.swipeVxCeiling    = (float)atof(val);
        else if (KeyIs(key, "swipe_peak_ratio"))    g_cfg.swipePeakRatio    = (float)atof(val);
        else if (KeyIs(key, "swipe_distance"))      g_cfg.swipeDistance     = (float)atof(val);
        else if (KeyIs(key, "swipe_min_frames"))    g_cfg.swipeMinFrames    = atoi(val);
        else if (KeyIs(key, "swipe_max_segments"))  g_cfg.swipeMaxSegments  = atoi(val);
        else if (KeyIs(key, "swipe_max_active_frames")) g_cfg.swipeMaxActiveFrames = atoi(val);
        else if (KeyIs(key, "swipe_axis_ratio"))    g_cfg.swipeAxisRatio    = (float)atof(val);
        else if (KeyIs(key, "swipe_cooldown_ms"))   g_cfg.swipeCooldownMs   = atoi(val);
        else if (KeyIs(key, "swipe_settle_after_ms")) g_cfg.swipeSettleAfterMs = atoi(val);
        else if (KeyIs(key, "swipe_settle_frames")) g_cfg.swipeSettleFrames = atoi(val);
        else if (KeyIs(key, "swipe_settle_frac"))   g_cfg.swipeSettleFrac   = (float)atof(val);
        else if (KeyIs(key, "swipe_y_min"))         g_cfg.swipeYMin         = (float)atof(val);
        else if (KeyIs(key, "swipe_y_max"))         g_cfg.swipeYMax         = (float)atof(val);
        else if (KeyIs(key, "swipe_vert_x_band"))   g_cfg.swipeVertXBand    = (float)atof(val);
        else if (KeyIs(key, "up_end_min_y"))        g_cfg.upEndMinY         = (float)atof(val);
        else if (KeyIs(key, "up_elbow_margin"))     g_cfg.upElbowMargin     = (float)atof(val);
        else if (KeyIs(key, "down_start_min_y"))    g_cfg.downStartMinY     = (float)atof(val);
        else if (KeyIs(key, "down_end_max_y"))      g_cfg.downEndMaxY       = (float)atof(val);
        else if (KeyIs(key, "left_end_max_x"))      g_cfg.leftEndMaxX       = (float)atof(val);
        else if (KeyIs(key, "right_end_min_x"))     g_cfg.rightEndMinX      = (float)atof(val);

        else if (KeyIs(key, "post_swipe_ms"))       g_cfg.postSwipeMs       = atoi(val);
        else if (KeyIs(key, "arm_extend_frac"))     g_cfg.armExtendFrac     = (float)atof(val);
        else if (KeyIs(key, "arm_relax_frac"))      g_cfg.armRelaxFrac      = (float)atof(val);
        else if (KeyIs(key, "arm_level_tan_max"))   g_cfg.armLevelTanMax    = (float)atof(val);
        else if (KeyIs(key, "repeat_first_ms"))     g_cfg.repeatFirstMs     = atoi(val);
        else if (KeyIs(key, "repeat_min_ms"))       g_cfg.repeatMinMs       = atoi(val);
        else if (KeyIs(key, "repeat_accel_ms"))     g_cfg.repeatAccelMs     = atoi(val);

        else if (KeyIs(key, "confirm_dwell_ms"))    g_cfg.confirmDwellMs    = atoi(val);
        else if (KeyIs(key, "confirm_repeat_ms"))   g_cfg.confirmRepeatMs   = atoi(val);
        else if (KeyIs(key, "confirm_repeat_first_ms")) g_cfg.confirmRepeatFirstMs = atoi(val);
        else if (KeyIs(key, "confirm_raise_fy"))    g_cfg.confirmRaiseFy    = (float)atof(val);
        else if (KeyIs(key, "confirm_still_vel"))   g_cfg.confirmStillVel   = (float)atof(val);

        else if (KeyIs(key, "back_dwell_ms"))       g_cfg.backDwellMs       = atoi(val);
        else if (KeyIs(key, "back_repeat_ms"))      g_cfg.backRepeatMs      = atoi(val);
        else if (KeyIs(key, "back_repeat_first_ms")) g_cfg.backRepeatFirstMs = atoi(val);
        else if (KeyIs(key, "back_out_min"))        g_cfg.backOutMin        = (float)atof(val);
        else if (KeyIs(key, "back_down_min"))       g_cfg.backDownMin       = (float)atof(val);
        else if (KeyIs(key, "back_down_max"))       g_cfg.backDownMax       = (float)atof(val);
        else if (KeyIs(key, "back_still_vel"))      g_cfg.backStillVel      = (float)atof(val);

        else if (KeyIs(key, "dwell_grace_frames"))  g_cfg.dwellGraceFrames  = atoi(val);
        else if (KeyIs(key, "arm_after_frames"))    g_cfg.armAfterFrames    = atoi(val);
        else if (KeyIs(key, "smooth_fast"))         g_cfg.smoothFast        = (float)atof(val);
        else if (KeyIs(key, "smooth_slow"))         g_cfg.smoothSlow        = (float)atof(val);
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

    LogLine("Config: loaded kinectnav.ini (%d settings)  hand=%s mirror=%d confirm=%d back=%d  swipe v=%.2f d=%.2f  extend=%.2f  confirm/back dwell=%d/%d ms",
            applied, g_cfg.leftHanded ? "L" : "R", g_cfg.mirror, g_cfg.enableConfirm, g_cfg.enableBack,
            g_cfg.swipeVelocity, g_cfg.swipeDistance, g_cfg.armExtendFrac,
            g_cfg.confirmDwellMs, g_cfg.backDwellMs);
    g_loaded = true;
    return g_cfg;
}

const Config& Cfg::Get()
{
    if (!g_loaded) return Cfg::Load();
    return g_cfg;
}
