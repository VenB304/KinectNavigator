Paste everything below the line into Gemini.

────────────────────────────────────────────────────────────────────────

You are localising the UI of **KinectNavigator-Setup**, a small one-window installer for a
free fan tool. KinectNavigator lets someone play the *Just Dance Legacy Offline PC* mod by
navigating its menus with Kinect arm poses instead of a keyboard. This installer swaps a DLL
into the game folder and edits a settings file.

I will give you the English source (`en.json`) and a finished French reference (`fr.json`).
Produce **9 translated files**, one per language below. Output each as its own fenced
` ```json ` block, with a line before it saying the filename (e.g. `de.json`). Each block must
be a **complete JSON object with every key from `en.json`, in the same order**.

## Target languages

| filename | `_meta.code` | `_meta.name` | `_meta.nativeName` |
|---|---|---|---|
| `de.json` | `de` | `German` | `Deutsch` |
| `it.json` | `it` | `Italian` | `Italiano` |
| `ja.json` | `ja` | `Japanese` | `日本語` |
| `ko.json` | `ko` | `Korean` | `한국어` |
| `nl.json` | `nl` | `Dutch` | `Nederlands` |
| `pt.json` | `pt` | `Portuguese` | `Português` |
| `ru.json` | `ru` | `Russian` | `Русский` |
| `zh-Hans.json` | `zh-Hans` | `Chinese (Simplified)` | `简体中文` |
| `zh-Hant.json` | `zh-Hant` | `Chinese (Traditional)` | `繁體中文` |

Set those three `_meta` values exactly as in the table. **Do not** include a `_meta.status`
key in your output (the English placeholders have one; the finished files should not).

## Rules

1. **Translate the values only. Never translate, rename, or reorder the keys.** Every output
   file has exactly the keys of `en.json`.
2. **Tone:** friendly, plain, concise, second person, imperative ("Pick your game folder",
   not "The user selects…"). It is a hobby tool, not a corporate product. Match the register
   of the French reference.
3. **Keep every `{placeholder}` token verbatim** — `{v}` `{inst}` `{shim}` `{mb}` `{kb}` `{k}`
   `{err}` `{label}` `{name}` `{got}` `{want}`. Do not add, drop, or rename them; place them
   where they read naturally. Each translated value must contain the same set of tokens as the
   matching English value.
4. **Keep `\n` exactly where it appears** (hard line breaks in the `dlg.*` and `gestures.body`
   strings). In `gestures.body` the runs of dots (`......`) are just leader filler — shorten
   or drop them so lines don't overflow; alignment does not matter (it's a plain message box).
5. **Never translate these literal strings**, wherever they occur:
   `KinectNavigator`, `Kinect10.dll`, `Kinect10_backend.dll`, `Kinect10.dll.orig-backup`,
   `legacy.exe`, `config.xml`, `kinectnav.ini`, `kinectnav.example.ini`, `KinectNavigator.log`,
   `skcap-*.skcap`, `FullScreen="0"`, `overlay = 1`, `overlay = 0`, `overlay`, `SETUP.md`,
   `Kinect for Windows Runtime`. Also keep the literal ini fragments inside `log.hud_on` /
   `log.hud_off` / `log.fs0_ok` / `log.fs0_fail` intact.
6. **`Enter` / `Esc`** are on-screen keyboard keys. Keep `Esc` as `Esc` everywhere. `Enter`
   may use your locale's Windows convention (de `Eingabe`, it `Invio`, ja `Enter`, ru `Ввод`,
   etc.) — pick one and use it consistently in that file.
7. **`HUD`** — keep as `HUD`; it is widely understood. Only add a gloss if your language has a
   truly standard term.
8. **Numbers / units:** keep `m` and `ms`. Localise `MB` / `KB` where your locale differs
   (French uses `Mo` / `Ko`). Localise the decimal mark where that is the norm
   (`2.5 m` → `2,5 m` for de, it, es, pt, ru, nl; keep `.` for ja, ko, zh). Keep the
   approximate "~15 MB" figure in `state.foreign`.
9. **Valid JSON, UTF-8.** Escape `"` as `\"` inside values (see `fs.fullscreen`). Do not add a
   BOM or trailing commas.

## Glossary — be consistent *within* each file

| English | meaning |
|---|---|
| wake it up / arm / disarm / it sleeps | the "clutch": the recogniser turns on when you park your hand at your shoulder, and turns off when your arm hangs or you dance |
| reach (out to the side / up / down-and-out) | extending the arm into a direction — the core navigation gesture |
| park box / park your hand | the small dead zone on the shoulder; hand inside = neutral |
| the shim | KinectNavigator's drop-in `Kinect10.dll` |
| genuine / original runtime | the real Microsoft `Kinect10.dll` (~15 MB) that the tool renames aside |
| dominant / non-dominant hand | dominant = the navigating hand; non-dominant = the one placed on the shoulder for command mode |
| Back gesture | the pose that sends `Esc` |
| command mode | non-dominant hand on the shoulder, then reach = Enter/Esc |
| feel presets | the named option groups in "More settings…" (reach, scroll speed, command reach, hold time) |
| navigation hand | which hand navigates (Right / Left) |

## Before you answer — self-check each file

- Same keys as `en.json` (105 strings + `_meta.code` / `_meta.name` / `_meta.nativeName`), same order, no `_meta.status`.
- Every `{token}` from each English value is present in your translation of it.
- `\n` preserved; `\"` used where the English has it.
- Parses as JSON.

---

### SOURCE — `en.json`

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

### REFERENCE — `fr.json` (already reviewed; follow this style, especially token and \n handling)

```json
{
  "_meta.code": "fr",
  "_meta.name": "French",
  "_meta.nativeName": "Français",

  "app.title": "KinectNavigator",
  "app.window_title": "KinectNavigator - Installation",
  "app.subtitle": "Navigation Kinect mains libres dans les menus de Just Dance Legacy Offline PC",
  "app.msgbox_title": "Installation de KinectNavigator",
  "app.installer_version": "installateur  v{v}",
  "app.lang_label": "Langue",

  "btn.browse": "Parcourir...",
  "btn.install": "Installer",
  "btn.update": "Mettre à jour",
  "btn.reinstall": "Réinstaller",
  "btn.uptodate": "À jour",
  "btn.uninstall": "Désinstaller",
  "btn.gestures": "Gestes",
  "btn.close": "Fermer",
  "btn.cancel": "Annuler",
  "btn.change": "Modifier...",
  "btn.reset_keys": "Réinitialiser les touches",
  "btn.more": "Plus de réglages...",
  "btn.open_ini": "Ouvrir kinectnav.ini",
  "btn.reset_cfg": "Rétablir les valeurs par défaut",

  "gf.label": "Dossier du jeu",
  "gf.browse_desc": "Choisissez le dossier qui contient legacy.exe",

  "runtime.detected": "Kinect for Windows Runtime : détecté",
  "runtime.missing": "Kinect for Windows Runtime : non détecté - cliquez pour le télécharger",

  "state.none": "Choisissez le dossier de votre jeu.",
  "state.notexist": "Ce dossier n'existe pas - choisissez le dossier de votre jeu.",
  "state.nogame": "Aucun legacy.exe dans ce dossier - choisissez le dossier qui contient le jeu.",
  "state.installed": "KinectNavigator est installé ici.",
  "state.nodll": "Aucun Kinect10.dll ici - est-ce le bon dossier ?",
  "state.genuine": "Runtime Kinect authentique trouvé ({mb} Mo). Prêt à installer.",
  "state.foreign": "Le Kinect10.dll ici ne fait que {kb} Ko - ce n'est pas le runtime authentique d'environ 15 Mo. Si vous pilotez le jeu avec un émulateur webcam, KinectNavigator ne peut pas se placer par-dessus.",

  "ver.update_avail": "Mise à jour disponible - v{inst} installée, ce téléchargement est en v{shim}.",
  "ver.uptodate": "KinectNavigator v{inst} est installé et à jour.",
  "ver.older": "Ce téléchargement est en v{shim} - PLUS ANCIEN que la v{inst} installée.",
  "ver.unreadable": "Cliquez sur Mettre à jour pour remplacer le DLL installé par ce téléchargement (sa version n'a pas pu être lue).",

  "fs.fullscreen": "config.xml est réglé en plein écran - le HUD reste masqué tant que vous ne mettez pas FullScreen=\"0\".",
  "fs.windowed": "config.xml est en mode fenêtré - le HUD peut s'afficher.",

  "opt.header": "Réglages  -  enregistrés dans kinectnav.ini du dossier du jeu, appliqués au prochain lancement",
  "opt.hand_label": "Main de navigation :",
  "opt.hand_right": "Main droite",
  "opt.hand_left": "Main gauche",
  "opt.mirror": "Inverser gauche / droite",
  "opt.back": "Activer le geste Retour (Échap)",
  "opt.hud": "Afficher le HUD à l'écran  (mode fenêtré uniquement)",

  "more.title": "KinectNavigator - plus de réglages",
  "more.feel_header": "Ressenti  -  chaque ligne choisit un préréglage, enregistré dans kinectnav.ini",
  "more.reach": "Amplitude pour naviguer",
  "more.scroll": "Vitesse de défilement (maintien)",
  "more.cmdreach": "Amplitude du mode commande",
  "more.hold": "Durée de maintien pour Entrée / Échap",
  "more.clutch": "Exiger un réveil préalable  (poser la main sur l'épaule pour armer)",
  "more.keys_header": "Raccourcis clavier",

  "preset.custom": "(personnalisé)",
  "preset.reach.sensitive": "Plus sensible",
  "preset.reach.normal": "Normal",
  "preset.reach.big": "Amplitude plus grande",
  "preset.scroll.slow": "Plus lent",
  "preset.scroll.normal": "Normal",
  "preset.scroll.fast": "Plus rapide",
  "preset.cmd.easy": "Plus facile",
  "preset.cmd.normal": "Normal",
  "preset.cmd.strict": "Plus strict",
  "preset.hold.short": "Plus court",
  "preset.hold.normal": "Normal",
  "preset.hold.long": "Plus long",

  "key.left": "Gauche",
  "key.right": "Droite",
  "key.up": "Haut",
  "key.down": "Bas",
  "key.confirm": "Valider",
  "key.back": "Retour",

  "capture.title": "Appuyez sur une touche",
  "capture.body": "Appuyez sur la touche que vous voulez utiliser pour cette action.",

  "dlg.no_shim": "Kinect10.dll (le module KinectNavigator) n'est pas à côté de cet installateur.\nExtrayez toute l'archive ensemble, puis relancez.",
  "dlg.hud_fs_body": "Le HUD ne s'affiche qu'en mode fenêtré, et config.xml est réglé en plein écran.\n\nMettre FullScreen=\"0\" dans config.xml maintenant ?",
  "dlg.reset_cfg_body": "Supprimer kinectnav.ini et rétablir tous les réglages par défaut ?",
  "dlg.game_running": "Legacy est en cours d'exécution. Fermez d'abord le jeu, puis réessayez.",
  "dlg.install_ok_new": "KinectNavigator est installé.",
  "dlg.install_ok_upd": "KinectNavigator est mis à jour.",
  "dlg.install_ok_body": "Lancez le jeu normalement. Placez-vous à environ 2,5 m de recul et posez une main près de votre épaule pour le réveiller. Cliquez sur « Gestes » à tout moment pour le mode d'emploi.",
  "dlg.install_ok_hud_note": "\n\nRemarque : mettez FullScreen=\"0\" dans config.xml si vous voulez voir le HUD.",
  "dlg.install_fail": "Échec de l'installation :\n{err}",
  "dlg.uninstall_ok": "KinectNavigator a été supprimé. Le Kinect10.dll d'origine est de retour.",
  "dlg.uninstall_fail": "Échec de la désinstallation :\n{err}",

  "log.ini_set": "kinectnav.ini : {k} = {v}",
  "log.hud_on": "kinectnav.ini : overlay = 1 (HUD activé)",
  "log.hud_off": "kinectnav.ini : overlay = 0 (HUD désactivé)",
  "log.fs0_ok": "config.xml : FullScreen=\"0\"",
  "log.fs0_fail": "config.xml : modification impossible - mettez FullScreen=\"0\" à la main.",
  "log.ini_created": "kinectnav.ini créé à partir du modèle d'exemple.",
  "log.ini_opened": "kinectnav.ini ouvert - re-sélectionnez ensuite le dossier du jeu pour recharger ces contrôles.",
  "log.ini_none": "Aucun kinectnav.ini - déjà aux valeurs par défaut.",
  "log.ini_deleted": "kinectnav.ini supprimé - tous les réglages sont revenus par défaut.",
  "log.keys_reset": "kinectnav.ini : raccourcis clavier réinitialisés par défaut.",
  "log.preset_set": "kinectnav.ini : préréglage {label} = {name}",
  "log.renamed_backend": "Runtime authentique renommé en Kinect10_backend.dll (sauvegarde : Kinect10.dll.orig-backup).",
  "log.installed_new": "KinectNavigator installé sous le nom Kinect10.dll.",
  "log.installed_upd": "Kinect10.dll installé mis à jour.",
  "log.uninstalled": "Module supprimé et Kinect10.dll authentique restauré.",
  "log.uninstalled_kept": "kinectnav.ini / KinectNavigator.log / skcap-*.skcap laissés en place.",
  "log.error": "ERREUR : {err}",
  "log.av_altered": "Kinect10.dll copié mais il fait {got} octets au lieu de {want} - un antivirus l'a peut-être modifié. Vérifiez la quarantaine de votre antivirus / ajoutez une exclusion de dossier, puis réessayez.",
  "log.dll_small": "Kinect10.dll est de retour mais il paraît trop petit - quelque chose ne va pas. Vérifiez le dossier du jeu à la main.",

  "gestures.title": "KinectNavigator - gestes",
  "gestures.body": "LE RÉVEILLER\nPosez votre main dominante près de votre épaule un instant. Il se rendort si votre bras pend simplement ou si vous dansez, pour ne pas se déclencher pendant une chorégraphie.\n\nNAVIGUER - un petit + centré sur votre épaule dominante\n  tendre le bras sur le côté ...... Gauche / Droite\n  tendre vers le haut ............. Haut\n  tendre en bas et sur le côté .... Bas\n  maintenir le geste .............. répète (accélère)\n  plier le coude / ramener ........ arrête\n\nVALIDER / RETOUR\n  posez votre AUTRE main sur votre AUTRE épaule, puis :\n  tendre en haut ou à droite et maintenir .... Entrée\n  tendre en bas ou à gauche et maintenir ..... Échap\n\nPlacez-vous à environ 2,5 m de recul, centré, face au capteur.\nTous les détails et le dépannage sont dans SETUP.md."
}
```

Now produce `de.json`, `it.json`, `ja.json`, `ko.json`, `nl.json`, `pt.json`, `ru.json`,
`zh-Hans.json`, `zh-Hant.json`.
