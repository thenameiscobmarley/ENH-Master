# PHOSPHOR SCOPE

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A 2U green-phosphor CRT oscilloscope and vectorscope (model XY-2), like the X-Y displays on old studio,
laboratory and computer gear. It watches the rack's **output** - exactly what you hear - and never changes
the sound. It starts in the [Gear locker](../UI/Gear%20locker.md).

The trace is drawn like a real CRT's beam: it moves from sample to sample, glows brighter where it lingers
and fainter where it sweeps fast, and fades slowly on the phosphor.

- **MODE** — **X-Y**: left across, right up (Lissajous figures; a mono sound is a diagonal line).
  **M/S**: the same turned 45° like a goniometer - mono stands straight up, stereo width spreads sideways,
  out-of-phase lies flat. **Y-T**: the waveform, triggered so it stands still.
- **INTENSITY** — how bright the beam is.
- **FOCUS** — how sharp the beam is.
- **PERSIST** — how long the glow lasts (20 ms to about 5 s).
- **V/DIV** — the size of the picture.
- **TIME/DIV** — Y-T only: how much time the screen shows (0.2 ms to 200 ms a division).
- **FIT** — sizes the picture to fill the whole screen, across and up, whatever the level: quiet detail is
  drawn as large as loud (it follows the loudest part of the last second; V/DIV is ignored while it's on).
  Below -60 dBFS it stops growing, so silence isn't blown up into hiss.
- **POWER** — the screen on or off (the sound is never touched either way).

Code: `UI/Scene/HardwareRenderer.cpp` (`drawScopeTrace`), `UI/Scene/DesignedLayout.h` (`scPrint`),
`DSP/EngineMeters.h` (the sample ring).
