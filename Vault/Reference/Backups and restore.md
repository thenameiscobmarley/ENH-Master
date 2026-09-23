# Backups and restore

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Local snapshots (not in git, because they include built plugins) live in
`~/Projects/PvPAdaptiveDynamics-backups/`. Each version has three files:

- `<date>_<label>_src.tar.gz` — the source
- `<date>_<label>_ENH Master.vst3` — the installed plugin
- `<date>_<label>_HardwareKit.tar.gz` — the UI library

`RESTORE.txt` in that folder lists them. To put a plugin back (close your DAW first):

```sh
rm -rf ~/.vst3/"ENH Master.vst3"
cp -r ~/Projects/PvPAdaptiveDynamics-backups/"<label>_ENH Master.vst3" ~/.vst3/"ENH Master.vst3"
```

Every release is also on [GitHub Releases](https://github.com/thenameiscobmarley/ENH-Master/releases).
