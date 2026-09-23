# Windows port

> 🔎 **[Searchbar](Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

ENH Master runs on Windows since 1.2.4.1. Only two small helpers in
[HardwareKit](https://github.com/thenameiscobmarley/HardwareKit) needed Windows versions; everything
else (the sound, the 3D renderer, the tests) is the same code on both.

| Helper | Linux | Windows |
|---|---|---|
| `input/PointerPoller` | mouse and "is my window on top" from X11 | `PointerPoller_win32.cpp` (`GetCursorPos`, `WindowFromPoint`) |
| `input/WindowVisibility` | window shown / minimised from X11 | `WindowVisibility_win32.cpp` (`IsWindowVisible`, `IsIconic`) |

Every release is built and tested on Windows by the
[Release workflow](https://github.com/thenameiscobmarley/ENH-Master/actions/workflows/release.yml).
Older releases: `convert-to-windows.bat` (see [Windows and other platforms](Vault/Tutorial/06%20Windows%20and%20other%20platforms.md)).

**macOS** would need the same two helpers, an AU version, and ideally a Metal renderer.
