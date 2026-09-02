# KinectNavigator installer — translation brief (2026-09-02)

**For:** Gemini (or a human translator). **Task:** translate the 9 English-fallback UI
language files for the KinectNavigator setup tool.

---

## What this is

**KinectNavigator** is a small, free fan tool for the *Just Dance Legacy Offline PC* mod. It
lets someone drive the game's menus with Kinect arm poses instead of a keyboard. The thing
being translated is **`KinectNavigator-Setup`**, a one-window installer + settings editor
(WinForms, ~100 short strings). Tone: friendly, plain, concise, second person, imperative
("Pick your game folder", not "The user should select..."). It is not a corporate product —
keep it light.

## Files

All in **`dist/lang/`**, one flat JSON object per language, `"key": "value"` pairs.

- **`en.json`** — the source of truth. 105 translatable keys + 3 `_meta.*`.
- **`fr.json`, `es.json`** — already translated (reference for style/'`{token}`' handling).
- **Translate these 9** (currently English copies, marked `"_meta.status": "english-fallback; awaiting translation"`):
  `de` `it` `ja` `ko` `nl` `pt` `ru` `zh-Hans` `zh-Hant`

For each: keep **exactly the same keys, in the same order** as `en.json`. Output **UTF-8, no BOM**.

## Rules

1. **Translate values only. Never translate or reorder keys.**
2. **`_meta` block:**
   - `_meta.code` — keep (`de`, `zh-Hans`, …).
   - `_meta.name` — keep (English name of the language).
   - `_meta.nativeName` — keep (already the correct autonym: `Deutsch`, `日本語`, `Русский`, `简体中文`, …).
   - `_meta.status` — **delete this line** once the file is fully translated (or set it to `"translated"`).
3. **Keep every `{placeholder}` token verbatim** — `{v}`, `{inst}`, `{shim}`, `{mb}`, `{kb}`,
   `{k}`, `{err}`, `{label}`, `{name}`, `{got}`, `{want}`. Put them where they read naturally in
   the target language; do not add, remove, or rename them. (A token-count check runs on every
   file — a dropped token fails it.)
4. **Keep `\n`** exactly where it is (hard line breaks in `dlg.*` and `gestures.body`). In
   `gestures.body` the rows of dots (`......`) are just filler leaders — shorten or drop them so
   the translated line isn't too long; exact alignment does not matter (it's a message box).
5. **Do NOT translate these literals** wherever they appear: `KinectNavigator`, `Kinect10.dll`,
   `Kinect10_backend.dll`, `Kinect10.dll.orig-backup`, `legacy.exe`, `config.xml`,
   `kinectnav.ini`, `kinectnav.example.ini`, `KinectNavigator.log`, `skcap-*.skcap`,
   `FullScreen="0"`, `overlay`, `SETUP.md`, `Kinect for Windows Runtime`, and the literal ini
   key/`overlay = 1` fragments inside `log.hud_on` / `log.hud_off`.
6. **`Enter` / `Esc`** — the on-screen keyboard keys. Keep as `Enter` / `Esc`, or use the
   convention your locale's Windows uses (e.g. `Entf`/`Esc` is wrong — `Esc` is universal;
   `Enter` may be `Eingabe` in de, `Invio` in it, `엔터` — your call, but be consistent).
7. **`HUD`** — keep as "HUD"; it's widely understood. Add a gloss only if your language has an
   established term.
8. **Units & numbers:** keep `m`, `ms`; localise `MB`/`KB` if the locale differs (`Mo`/`Ko` in
   fr) and the decimal mark (`2.5 m` → `2,5 m` for de/fr/it/es/pt/ru/nl).
9. `state.foreign` mentions "~15 MB" — keep the approximate figure.

## Glossary — keep these consistent *within* each file

| en | meaning |
|---|---|
| wake it up / arm / disarm / it sleeps | the "clutch": you activate the recogniser by parking your hand at your shoulder; it deactivates when your arm hangs or you dance |
| reach (out / up / down-and-out) | extend the arm into a direction — this is the core nav gesture |
| park box / park your hand | the small dead zone centred on the shoulder; hand inside it = neutral |
| the shim | KinectNavigator's drop-in `Kinect10.dll` |
| genuine / original runtime | the real Microsoft `Kinect10.dll` (~15 MB) the tool renames aside |
| dominant / non-dominant hand | dominant = the navigating hand; non-dominant = the one you put on your shoulder for command mode |
| Back gesture | the pose that sends `Esc` |
| command mode | non-dominant hand on shoulder → reach = Enter/Esc |
| feel presets | the named option groups in "More settings…" (reach, scroll speed, …) |
| navigation hand | which hand navigates (Right/Left) |

## Check before returning

- Same key set as `en.json` (105 + `_meta.code/name/nativeName`), same order.
- Every `{token}` from the matching `en.json` value is present in the translation.
- Valid JSON, UTF-8 no BOM.
- `_meta.status` removed (or `"translated"`).

Drop the finished files back into `dist/lang/`.
