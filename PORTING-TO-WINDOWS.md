# Does this build on Windows?

**Not as it stands.** The DSP, the plugin format and the whole OpenGL renderer are portable, but two
small helpers talk to the X server directly, so the project only compiles on Linux/X11 today:

| File (in the HardwareKit module) | What it does | Windows equivalent |
|---|---|---|
| `input/PointerPoller.*` | Reads the pointer position, left button and Shift/Ctrl once per rendered frame, and checks whether the plugin window is really topmost under the pointer | `GetCursorPos` + `ScreenToClient`, `GetAsyncKeyState (VK_LBUTTON / VK_SHIFT / VK_CONTROL)`, `WindowFromPoint` walked up with `GetAncestor (…, GA_ROOT)` |
| `input/WindowVisibility.*` | Asks whether the editor window is actually on screen, so rendering can pause when the host is minimised | `IsWindowVisible` + `IsIconic` on the window and its root ancestor |
| `hardwarekit_x11.cpp` | The only translation unit that includes `Xlib.h` | Replace with a `hardwarekit_win32.cpp` that includes `<windows.h>` |
| `modules/hardwarekit/hardwarekit.h` | Module declaration: `linuxLibs: X11` | `windowsLibs: user32` (JUCE links most of this already) |

Everything else - JUCE 8, the VST3 target, `juce_opengl` (3.2 core profile), the shaders, the DSP and
the offline tests - already builds with MSVC.

The install path differs too: CMake copies the built bundle to `~/.vst3` here; on Windows that is
`%COMMONPROGRAMFILES%\VST3`, which JUCE's `COPY_PLUGIN_AFTER_BUILD` handles on its own.

## Prompt for converting it yourself

If you downloaded this and want a Windows build, hand the repository plus the prompt below to a
coding AI (Claude Code, Codex, Cursor, …). It is written to be pasted as-is.

---

You are porting a JUCE 8 VST3 plugin ("ENH Master") from Linux/X11 to Windows. The repository builds
with CMake + Ninja and depends on two checkouts side by side: JUCE at `$ENV{HOME}/JUCE` (override with
`-DJUCE_PATH=`) and the shared module `HardwareKit` at `../HardwareKit` (override with
`-DHARDWAREKIT_PATH=`). Do not change the DSP, the shaders, the layout or the look: this is a platform
port only, and the rendering must come out pixel-identical.

1. Read `modules/hardwarekit/input/PointerPoller.h` and `input/WindowVisibility.h` in HardwareKit.
   Keep both public interfaces exactly as they are, including the `unsigned long window` handle type
   (on Windows it carries an `HWND`; cast through `reinterpret_cast<HWND> ((uintptr_t) window)`).
2. Add `hardwarekit_win32.cpp` next to `hardwarekit_x11.cpp`, guarded with `#if JUCE_WINDOWS`, and
   guard the existing X11 file with `#if JUCE_LINUX || JUCE_BSD`. Implement:
   - `PointerPoller::query (window, windowX, windowY [, leftDown, fineModifier])` - `GetCursorPos`
     then `ScreenToClient` on that HWND; buttons and modifiers from `GetAsyncKeyState`, with
     `fineModifier` true when Shift or Ctrl is down. Return false when the window is null or the
     call fails, so the caller falls back to JUCE mouse events.
   - `PointerPoller::isTopmostUnderPointer (window)` - `WindowFromPoint` at the cursor, then walk up
     with `GetAncestor (…, GA_ROOT)` and compare against the root ancestor of `window`. This gates
     mouse presses: clicks in another application that covers the plugin must not move knobs.
   - `WindowVisibility::isVisible (window)` - false only when the window (or its root ancestor) is
     `IsIconic` or not `IsWindowVisible`; return **true** whenever the state cannot be determined, so
     a failed query never pauses rendering.
3. In `modules/hardwarekit/hardwarekit.h`, keep `linuxLibs: X11` and add `windowsLibs: user32`.
4. Build the VST3 and Standalone targets with MSVC (x64, Release) and run the offline DSP tests
   (`EnhDspTests`), which are platform-independent and must all pass.
5. Check by hand on Windows: knobs respond to drags; clicking in a window that overlaps the plugin
   does not change any value; minimising the host pauses rendering and restoring resumes it (the
   standalone prints this with `PAD_UI_TEST_STATS=1`).

Report what you changed, and anything you had to work around.

---

If you get that working, a pull request is welcome.
