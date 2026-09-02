#pragma once

// Menu-vs-gameplay probe. IAT-hooks USER32!GetAsyncKeyState in the host exe and
// records when the game last polled a menu-navigation key (arrows / Enter / Esc
// / Space / Backspace). The hypothesis: the game only reads those while a menu
// is up, so "polled recently" == "a menu is listening" == safe to inject keys.
//
// Observe-only for now -- it just updates g_menuPollTick (globals.h); nothing
// gates on it yet. Install() is idempotent and a no-op if the hook can't be
// placed.

namespace GameProbe
{
    void Install();
    void Remove();
}
