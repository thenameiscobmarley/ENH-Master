# 06 Windows and other platforms

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

| System | Status |
|---|---|
| Linux (X11) | where it's made and tested: VST3 and the app |
| Windows | supported since 1.2.4.1: VST3 and the app |
| macOS | not yet |

## Windows

Every release has a `windows-x64` zip. For **any** release, even old Linux-only ones, download
[`convert-to-windows.bat`](https://github.com/thenameiscobmarley/ENH-Master/blob/main/convert-to-windows.bat)
and double-click it:

1. It lists every release.
2. Type the number you want.
3. It downloads it, or builds it for you if there was no Windows version (it can install the tools it
   needs; a build takes 10–30 minutes).
4. The result lands on your Desktop, and it offers to install the plugin.

What the Windows port changed: [PORTING-TO-WINDOWS.md](../../PORTING-TO-WINDOWS.md).

## macOS

It would need two small platform files, an AU version, and ideally a Metal renderer.

Next: [The router app](07%20The%20router%20app.md)
