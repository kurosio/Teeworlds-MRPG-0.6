# Server translations

`index.json` lists the available language files. English is the fallback language and does not need a separate `en.txt`; translations are stored in files such as `ru.txt`, `de.txt`, and `fr.txt`.

Translate the source text while preserving any `{}` format placeholders and their order. Keep the language entry and filename consistent with `index.json`.

The server's RCON command `sync_lines_for_translate` compares the translation files with the current localization keys, adding newly required entries and moving untranslated lines toward the top. It runs asynchronously. `reload_localization` synchronizes the keys and reloads localization files. Review the generated changes before committing or deploying them; these commands do not provide a completed translation automatically.

`docs/server_sync.md` describes the relay's localization hash exchange. Full transfer and automatic replacement of translation files through the relay are not implemented.
