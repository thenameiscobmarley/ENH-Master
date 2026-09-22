# Windows

**ENH Master builds and runs on Windows from 1.2.4.1.** This file used to describe the port; it is done.

What the port changed (all in the HardwareKit module, the plugin itself was already portable):

| File | Linux | Windows |
|---|---|---|
| `input/PointerPoller` | the pointer, left button and Shift/Ctrl from the X server; "is my window topmost under the pointer" | `input/PointerPoller_win32.cpp`: `GetCursorPos` + `ScreenToClient`, `GetAsyncKeyState`, `WindowFromPoint` compared by `GetAncestor (..., GA_ROOT)` |
| `input/WindowVisibility` | mapped, and not `_NET_WM_STATE_HIDDEN` | `input/WindowVisibility_win32.cpp`: `IsWindowVisible` and `IsIconic` on the window and its root |
| module translation units | `hardwarekit_x11.cpp` (only on Linux / BSD) | `hardwarekit_win32.cpp` (only on Windows), `windowsLibs: user32` |

In the plugin, `CMakeLists.txt` looks for X11 only on Linux. Everything else, including the DSP, the
OpenGL 3.2 renderer, the shaders and the offline tests, is the same code on both platforms.

- **Releases:** the Release workflow builds and tests on `windows-2022` too, and attaches
  `ENH-Master-<version>-windows-x64.zip` to each release.
- **Older releases:** `convert-to-windows.bat` builds any older release on Windows (see the README's
  *Windows* section).

**macOS** would need the same two files (`NSEvent.mouseLocation`, `-[NSWindow isMiniaturized]`) and an AU
target. OpenGL is deprecated there: the renderer would run, but a Metal backend is the real answer.
