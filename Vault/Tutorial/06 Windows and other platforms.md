# 06 Windows and other platforms

**Linux / X11:** fully supported, this is where it is developed and tested (Carla, VST3 and
standalone).

**Windows and macOS:** the plugin does **not** build there as it stands. The DSP, the VST3 target and
the entire OpenGL renderer are portable; two small helpers in [[HardwareKit]] talk to the X server
directly and need a platform version:

- `input/PointerPoller` - pointer position, left button and modifiers once per frame, plus the
  "is my window really topmost under the pointer" check that stops clicks in other applications from
  turning knobs;
- `input/WindowVisibility` - whether the editor is actually on screen, which is what pauses rendering
  when the host is minimised.

The repository ships `PORTING-TO-WINDOWS.md` with a table of exactly what to replace (`GetCursorPos`,
`GetAsyncKeyState`, `WindowFromPoint`, `IsIconic`, …) **and a ready-made prompt** you can paste into a
coding AI along with the repo to do the port for you.

macOS would need the same two files (`NSEvent.mouseLocation`, `-[NSWindow isMiniaturized]`) plus the
usual JUCE/AU housekeeping, and OpenGL is deprecated there - the renderer runs, but a Metal backend
would be the real answer.

Back to [[00 Start Here]].
