# Interface languages

One flat JSON file per language for `KinectNavigator-Setup`. `en.json` is the source of
truth; missing keys in any other file fall back to English at runtime. The setup tool lists
every `*.json` here in its language menu (flag + `_meta.nativeName`).

**Add a language:** copy `en.json` to `<code>.json` (BCP-47-ish, e.g. `pl`, `pt-BR`), set the
three `_meta.*` values, translate the rest, save as UTF-8. It shows up automatically.

**Status:** `en`, `fr`, `es` are translated. The other 9 (`de it ja ko nl pt ru zh-Hans
zh-Hant`) are English placeholders pending a translation pass — see
`docs/notes/translation-brief-2026-09-02.md`. A file still awaiting translation carries
`"_meta.status": "english-fallback; awaiting translation"`.

Rules for translators: translate values only (never keys), keep every `{token}` and `\n`, and
don't translate literal filenames (`Kinect10.dll`, `kinectnav.ini`, `config.xml`, …). Full
details in the brief above.
