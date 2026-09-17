# Backups and restore

Local snapshots live in `~/Projects/PvPAdaptiveDynamics-backups/`, three files per version:

```
<date>_<label>_src.tar.gz          the source tree (no build/, no .git)
<date>_<label>_ENH Master.vst3     the installed bundle, exactly as it was
<date>_<label>_HardwareKit.tar.gz  the shared module at that point
```

`RESTORE.txt` in that folder lists each version and the commands. To put a build back:

```sh
rm -rf ~/.vst3/"ENH Master.vst3"
cp -r ~/Projects/PvPAdaptiveDynamics-backups/"<label>_ENH Master.vst3" ~/.vst3/"ENH Master.vst3"
```

Close the host first - hosts keep the bundle open.

The most recent label is `2026-09-17_enh-master-v12-masters-library-loupe` (device masters, the
HardwareKit extraction and the fisheye loupe).

Extract HardwareKit to `~/Projects/` - the plugin's CMake expects it at `../HardwareKit`, or pass
`-DHARDWAREKIT_PATH=`.

> [!note] Backups are not the repository
> These are local, whole-state snapshots including a built binary, which is why they are not in git.
> Git history is the other half: see the tags listed in the project README.

Related: [[01 Install and build]].
