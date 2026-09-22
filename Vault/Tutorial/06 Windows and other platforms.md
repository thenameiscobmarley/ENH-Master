# 06 Windows and other platforms

**Linux / X11:** where it is developed and tested (Carla, VST3 and standalone).

**Windows:** supported from 1.2.4.1. Every release has a `windows-x64` zip with the VST3 and the
standalone app. To get **any** release on Windows, older Linux-only ones included, double-click
`convert-to-windows.bat` from the repository:

1. it lists every release on GitHub and marks the ones with a Windows build;
2. you type the number of the one you want;
3. a release with a Windows build is downloaded; an older one is built from its own source (the script
   fetches that release, JUCE, and [[HardwareKit]] as it was then plus today's Windows platform files, and
   offers to install Git, CMake and the Visual Studio C++ build tools with `winget` if they are missing);
4. the plugin lands on your Desktop, and the script offers to install it into
   `C:\Program Files\Common Files\VST3`.

What the port changed is in `PORTING-TO-WINDOWS.md`: two small HardwareKit helpers
(`input/PointerPoller`, `input/WindowVisibility`) got Win32 versions; everything else was already
portable.

**macOS:** not yet. It would need the same two helpers for Cocoa and an AU target, and OpenGL is
deprecated there, so a Metal backend would be the real answer.

Back to [[00 Start Here]].
