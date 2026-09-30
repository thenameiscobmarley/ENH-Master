# ATR TAPE

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A tape machine: the saturation remembers where it has been (a hysteresis loop, like magnetic tape), then each SPEED's head bump and top roll-off, WOW and FLUTTER pitch wander, and optional HISS. (2U, model ATR-102.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **INPUT** — -12 – 18 dB
- **SATURATION** — 0 – 10
- **SPEED** — 7.5 / 15 / 30
- **WOW** — 0 – 10
- **FLUTTER** — 0 – 10
- **HISS** — 0 – 10
- **OUTPUT** — -12 – 12 dB

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (tape), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
