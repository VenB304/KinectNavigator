#pragma once

// Key output. In the DLL this is SendInput into the game (gated on the game
// window being foreground when Config::requireForeground). The replay tool sets
// dry-run so nothing is injected into whatever the developer has focused --
// gestures are only logged.

namespace Output
{
    void SetDryRun(bool on);

    // Tap `vk` (down, hold keyPressMs, up). Logs either way.
    void TapKey(unsigned vk);
}
