Paste everything below the line into Gemini, then paste the 12 language files after it
(`en.json` first as reference, then the 11 others). Ask for the review.

────────────────────────────────────────────────────────────────────────

You are a **localisation QA reviewer** with native or near-native fluency in German, Spanish,
French, Italian, Japanese, Korean, Dutch, Portuguese, Russian, and both Simplified and
Traditional Chinese. Review the 12 UI language files for **KinectNavigator-Setup** and report
what needs fixing before release.

## What the app is

KinectNavigator is a free fan tool for the *Just Dance Legacy Offline PC* mod: it lets someone
drive the game's menus with Kinect arm poses instead of a keyboard. The files under review are
the strings for **KinectNavigator-Setup**, a single-window installer that swaps a DLL into the
game folder and edits a plain-text settings file. Register: friendly, plain, concise, direct,
second-person imperative — a hobby tool, not a corporate product.

## Inputs

- `en.json` — the source of truth (below / pasted first).
- The 11 translations: `fr`, `es`, `de`, `it`, `ja`, `ko`, `nl`, `pt`, `ru`, `zh-Hans`,
  `zh-Hant`. **`fr` and `es` were done by a non-native author — give them the same scrutiny as
  the machine-translated ones.**

Mechanical checks (key set, key order, `{token}` preservation, `\n` counts, escaped quotes,
`_meta.*` values, do-not-translate literals) have **already passed** for all 12 files. You do
not need to re-verify those — but if you happen to spot a violation, flag it as a **blocker**.
Your job is the **linguistic** quality the automated checks can't judge.

## Expected `_meta` values (for reference)

| file | `_meta.code` | `_meta.name` | `_meta.nativeName` |
|---|---|---|---|
| fr | fr | French | Français |
| es | es | Spanish | Español |
| de | de | German | Deutsch |
| it | it | Italian | Italiano |
| ja | ja | Japanese | 日本語 |
| ko | ko | Korean | 한국어 |
| nl | nl | Dutch | Nederlands |
| pt | pt | Portuguese | Português |
| ru | ru | Russian | Русский |
| zh-Hans | zh-Hans | Chinese (Simplified) | 简体中文 |
| zh-Hant | zh-Hant | Chinese (Traditional) | 繁體中文 |

## Review each translation for

1. **Accuracy** — does every string carry the same meaning as the English? Flag
   mistranslations, reversed or lost meaning, dropped conditions/nuance, wrong referent
   (e.g. "it" meaning the tool vs. the game vs. the DLL).
2. **Fluency / naturalness** — does it read like a native speaker wrote it for a small
   software installer? Flag machine-translation artefacts, calques, stilted or overly literal
   phrasing, wrong collocations, missing or wrong articles/particles, punctuation that doesn't
   match the language's norms (e.g. spacing around `:` and `?` in French, full-width
   punctuation in ja/zh).
3. **Tone & register consistency** — friendly, concise, imperative second person. Where a
   language's norm for UI is a polite form (German *Sie*, Japanese *です/ます*, Korean
   *합니다*), that's fine **as long as it's consistent across the whole file**. Flag mixed
   registers within one file, and anything needlessly formal, passive, or verbose.
4. **Terminology consistency within the file** — the recurring concepts (see glossary) must
   use the **same term every time**. Flag a concept translated two or three different ways
   (common ones to watch: *reach*, *wake / arm / disarm*, *game folder*, *shim*, *runtime*,
   *hold*, *setting / preset*).
5. **Key names** — `Esc` must stay `Esc`. `Enter` may be localised (de *Eingabe*, it *Invio*,
   ru *Ввод*, …) but must be **one consistent form** throughout the file. Flag inconsistency.
6. **`HUD`** — should stay `HUD` unless the language has a genuinely standard term; either way,
   consistent.
7. **Numbers & units** — `2.5 m` should use the locale's decimal mark (comma for de, es, fr,
   it, pt, ru, nl; keep the dot for ja, ko, zh). `MB`/`KB` may be localised (`Mo`/`Ko` in fr).
   The approximate "~15 MB" figure must survive.
8. **Length / truncation risk** — these strings sit in **fixed-width controls** and will clip
   if much longer than the English. Flag any translation that is clearly too long, name the
   key, and suggest a shorter wording:
   - very tight (~84 px): `key.left` `key.right` `key.up` `key.down` `key.confirm` `key.back`,
     `app.lang_label`
   - tight (buttons, ~95–160 px): all `btn.*`, `opt.hand_right`, `opt.hand_left`
   - moderate (~210–290 px): `more.reach` `more.scroll` `more.cmdreach` `more.hold`,
     `opt.mirror` `opt.back` `opt.hud`, all `preset.*`
9. **Leftover English** — flag any value still in English (other than deliberate ones:
   `app.title` is always `KinectNavigator`; the pure format strings `log.ini_set`,
   `log.fs0_ok`, and the `preset.*.normal` = "Normal" family are fine to leave).
10. **Do-not-translate literals still literal** — `KinectNavigator`, `Kinect10.dll`,
    `Kinect10_backend.dll`, `Kinect10.dll.orig-backup`, `legacy.exe`, `config.xml`,
    `kinectnav.ini`, `kinectnav.example.ini`, `KinectNavigator.log`, `skcap-*.skcap`,
    `FullScreen="0"`, `overlay`, `SETUP.md`, `Kinect for Windows Runtime`. Flag any that were
    translated, cased differently, or spelled wrong.

## Glossary — the intended meaning of the recurring terms

| English | meaning |
|---|---|
| wake it up / arm / disarm / it sleeps | the "clutch": the recogniser turns on when you park your hand at your shoulder, off when your arm hangs or you dance |
| reach (out to the side / up / down-and-out) | extending the arm into a direction — the core navigation gesture |
| park box / park your hand | the small dead zone on the shoulder; hand inside = neutral |
| the shim | KinectNavigator's drop-in `Kinect10.dll` |
| genuine / original runtime | the real Microsoft `Kinect10.dll` (~15 MB) the tool renames aside |
| dominant / non-dominant hand | dominant = the navigating hand; non-dominant = the one placed on the shoulder for command mode |
| Back gesture | the pose that sends `Esc` |
| command mode | non-dominant hand on the shoulder, then reach = Enter/Esc |
| feel presets | the named option groups in "More settings…" (reach, scroll speed, command reach, hold time) |
| navigation hand | which hand navigates (Right / Left) |

## Output format

For **each** of the 11 translations, in this order (fr, es, de, it, ja, ko, nl, pt, ru,
zh-Hans, zh-Hant):

```
## <code> — <READY | NEEDS FIXES (n)>

| key | severity | issue | suggested fix |
|-----|----------|-------|---------------|
| ... | blocker/major/minor | ... | ... |
```

- **blocker** = mechanical violation, meaning-breaking mistranslation, or truncation that
  hides essential text.
- **major** = clearly wrong or unnatural; a native speaker would notice and it looks bad.
- **minor** = polish; understandable but not idiomatic.
- If a file has no issues, write `No issues.` under the header.

Then a **summary matrix**:

```
| lang | accuracy | fluency | tone/consistency | length | verdict |
|------|----------|---------|------------------|--------|---------|
```
(rate each column good / ok / weak.)

Then a short paragraph: which languages are ship-ready as-is, which need one fix pass, and any
issue that recurs across several files (systemic wording to fix in the source or the glossary).

---

### REFERENCE — `en.json`

```json
{
  "_meta.code": "en",
  "_meta.name": "English",
  "_meta.nativeName": "English",

  "app.title": "KinectNavigator",
  "app.window_title": "KinectNavigator - Setup",
  "app.subtitle": "Hands-free Kinect menu navigation for Just Dance Legacy Offline PC",
  "app.msgbox_title": "KinectNavigator Setup",
  "app.installer_version": "installer  v{v}",
  "app.lang_label": "Language",

  "btn.browse": "Browse...",
  "btn.install": "Install",
  "btn.update": "Update",
  "btn.reinstall": "Reinstall",
  "btn.uptodate": "Up to date",
  "btn.uninstall": "Uninstall",
  "btn.gestures": "Gestures",
  "btn.close": "Close",
  "btn.cancel": "Cancel",
  "btn.change": "Change...",
  "btn.reset_keys": "Reset keys",
  "btn.more": "More settings...",
  "btn.open_ini": "Open kinectnav.ini",
  "btn.reset_cfg": "Reset to defaults",

  "gf.label": "Game folder",
  "gf.browse_desc": "Pick the folder that has legacy.exe in it",

  "runtime.detected": "Kinect for Windows Runtime: detected",
  "runtime.missing": "Kinect for Windows Runtime: not detected - click to download it",

  "state.none": "Choose your game folder.",
  "state.notexist": "That folder doesn't exist - choose your game folder.",
  "state.nogame": "No legacy.exe in this folder - pick the folder that has the game in it.",
  "state.installed": "KinectNavigator is installed here.",
  "state.nodll": "No Kinect10.dll here - is this the right folder?",
  "state.genuine": "Genuine Kinect runtime found ({mb} MB). Ready to install.",
  "state.foreign": "The Kinect10.dll here is only {kb} KB - not the genuine ~15 MB runtime. If you drive the game with a webcam emulator, KinectNavigator cannot sit on top of it.",

  "ver.update_avail": "Update available - installed v{inst}, this download is v{shim}.",
  "ver.uptodate": "KinectNavigator v{inst} is installed and up to date.",
  "ver.older": "This download is v{shim} - OLDER than the installed v{inst}.",
  "ver.unreadable": "Click Update to replace the installed DLL with this download (its version could not be read).",

  "fs.fullscreen": "config.xml is set to fullscreen - the HUD stays hidden unless you set FullScreen=\"0\".",
  "fs.windowed": "config.xml is windowed - the HUD can show.",

  "opt.header": "Settings  -  saved to kinectnav.ini in the game folder, applied next launch",
  "opt.hand_label": "Navigation hand:",
  "opt.hand_right": "Right hand",
  "opt.hand_left": "Left hand",
  "opt.mirror": "Reverse left / right",
  "opt.back": "Enable the Back gesture (Esc)",
  "opt.hud": "Show the on-screen HUD  (windowed only)",

  "more.title": "KinectNavigator - more settings",
  "more.feel_header": "Feel  -  each row picks a preset, saved to kinectnav.ini",
  "more.reach": "Reach to navigate",
  "more.scroll": "Scroll speed when held",
  "more.cmdreach": "Command-mode reach",
  "more.hold": "Hold time for Enter / Esc",
  "more.clutch": "Require waking it up first  (park hand at the shoulder to arm)",
  "more.keys_header": "Key bindings",

  "preset.custom": "(custom)",
  "preset.reach.sensitive": "More sensitive",
  "preset.reach.normal": "Normal",
  "preset.reach.big": "Bigger reach",
  "preset.scroll.slow": "Slower",
  "preset.scroll.normal": "Normal",
  "preset.scroll.fast": "Faster",
  "preset.cmd.easy": "Easier",
  "preset.cmd.normal": "Normal",
  "preset.cmd.strict": "Stricter",
  "preset.hold.short": "Shorter",
  "preset.hold.normal": "Normal",
  "preset.hold.long": "Longer",

  "key.left": "Left",
  "key.right": "Right",
  "key.up": "Up",
  "key.down": "Down",
  "key.confirm": "Confirm",
  "key.back": "Back",

  "capture.title": "Press a key",
  "capture.body": "Press the key you want to use for this action.",

  "dlg.no_shim": "Kinect10.dll (the KinectNavigator shim) isn't next to this installer.\nExtract the whole release together, then run it again.",
  "dlg.hud_fs_body": "The HUD only shows in windowed mode, and config.xml is set to fullscreen.\n\nSet FullScreen=\"0\" in config.xml now?",
  "dlg.reset_cfg_body": "Delete kinectnav.ini and return every setting to its default?",
  "dlg.game_running": "Legacy is currently running. Close the game first, then try again.",
  "dlg.install_ok_new": "KinectNavigator is installed.",
  "dlg.install_ok_upd": "KinectNavigator is updated.",
  "dlg.install_ok_body": "Launch the game normally. Stand about 2.5 m back and rest a hand near your shoulder to wake it. Click 'Gestures' any time for the how-to.",
  "dlg.install_ok_hud_note": "\n\nNote: set FullScreen=\"0\" in config.xml if you want to see the HUD.",
  "dlg.install_fail": "Install failed:\n{err}",
  "dlg.uninstall_ok": "KinectNavigator removed. The original Kinect10.dll is back.",
  "dlg.uninstall_fail": "Uninstall failed:\n{err}",

  "log.ini_set": "kinectnav.ini: {k} = {v}",
  "log.hud_on": "kinectnav.ini: overlay = 1 (HUD on)",
  "log.hud_off": "kinectnav.ini: overlay = 0 (HUD off)",
  "log.fs0_ok": "config.xml: FullScreen=\"0\"",
  "log.fs0_fail": "config.xml: could not change it - edit FullScreen=\"0\" by hand.",
  "log.ini_created": "Created kinectnav.ini from the example template.",
  "log.ini_opened": "Opened kinectnav.ini - re-pick the game folder afterward to reload these controls.",
  "log.ini_none": "No kinectnav.ini - already at defaults.",
  "log.ini_deleted": "kinectnav.ini deleted - all settings back to defaults.",
  "log.keys_reset": "kinectnav.ini: key bindings reset to defaults.",
  "log.preset_set": "kinectnav.ini: {label} preset = {name}",
  "log.renamed_backend": "Renamed the genuine runtime to Kinect10_backend.dll (backup: Kinect10.dll.orig-backup).",
  "log.installed_new": "Installed KinectNavigator as Kinect10.dll.",
  "log.installed_upd": "Updated the installed Kinect10.dll.",
  "log.uninstalled": "Removed the shim and restored the genuine Kinect10.dll.",
  "log.uninstalled_kept": "Left kinectnav.ini / KinectNavigator.log / skcap-*.skcap in place.",
  "log.error": "ERROR: {err}",
  "log.av_altered": "Kinect10.dll copied but landed at {got} bytes, not {want} - antivirus may have altered it. Check your antivirus quarantine / add a folder exclusion, then try again.",
  "log.dll_small": "Kinect10.dll is back but looks too small - something is wrong. Check the game folder by hand.",

  "gestures.title": "KinectNavigator - gestures",
  "gestures.body": "WAKING IT UP\nRest your dominant hand near your shoulder for a moment. It sleeps again if your arm just hangs or you dance, so it won't fire mid-routine.\n\nNAVIGATING - a small + centred on your dominant shoulder\n  reach out to the side ......... Left / Right\n  reach up ...................... Up\n  reach down-and-out ........... Down\n  hold the reach ............... repeats (speeds up)\n  bend the elbow / pull back ... stops\n\nCONFIRM / BACK\n  put your OTHER hand on your OTHER shoulder, then:\n  reach up or right and hold .... Enter\n  reach down or left and hold ... Esc\n\nStand about 2.5 m back, centred, facing the sensor.\nFull details and troubleshooting are in SETUP.md."
}
```

### Files to review

Paste `fr.json`, `es.json`, `de.json`, `it.json`, `ja.json`, `ko.json`, `nl.json`, `pt.json`,
`ru.json`, `zh-Hans.json`, `zh-Hant.json` here, each in its own fenced block.
