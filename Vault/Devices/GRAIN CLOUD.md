# GRANULAR DELAY

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called GRAIN CLOUD.

Granular delay: the sound goes into a 2.5 s memory and comes back as grains - windowed, pitched, scattered in time and stereo, some backwards. FEEDBACK builds it into a texture. (2U, model GC-1.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **SIZE** — 10 – 500 ms
- **DENSITY** — 1 – 40/s
- **PITCH** — -12 – 12 st
- **SPRAY** — 0 – 100%
- **REVERSE** — 0 – 100%
- **FEEDBACK** — 0 – 90%
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (grain), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
