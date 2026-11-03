#include "framework.h"
#include "output.h"
#include "config.h"
#include "log.h"

namespace
{
    bool g_dryRun = false;

    bool GameIsForeground()
    {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    const char* VkName(unsigned vk)
    {
        switch (vk)
        {
        case 0x25: return "VK_LEFT";
        case 0x26: return "VK_UP";
        case 0x27: return "VK_RIGHT";
        case 0x28: return "VK_DOWN";
        case 0x0D: return "VK_RETURN";
        case 0x1B: return "VK_ESCAPE";
        case 0x20: return "VK_SPACE";
        case 0x08: return "VK_BACK";
        default:   return "VK_?";
        }
    }
}

void Output::SetDryRun(bool on) { g_dryRun = on; }

void Output::TapKey(unsigned vk)
{
    const Config& c = Cfg::Get();

    if (g_dryRun)
    {
        LogLine("OUTPUT: tap %s (0x%02X) [dry-run]", VkName(vk), vk);
        return;
    }

    if (c.requireForeground && !GameIsForeground())
    {
        LogLine("OUTPUT: tap %s suppressed (game not foreground)", VkName(vk));
        return;
    }

    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = (WORD)vk;
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(1, &in[0], sizeof(INPUT));
    Sleep(c.keyPressMs > 0 ? (DWORD)c.keyPressMs : 40);
    SendInput(1, &in[1], sizeof(INPUT));

    LogLine("OUTPUT: tap %s (0x%02X)", VkName(vk), vk);
}
