# SPRING TANK

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A real spring disperses: its highs arrive before its lows, so every echo is a chirp. 1 to 3 springs, each a feedback loop with a chain of 24 allpasses (the dispersion), a tone filter and a gently driven input (DWELL). (1U, model ST-3.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **DWELL** — 0 – 10
- **DECAY** — 0 – 10
- **TONE** — 0 – 10
- **SPRINGS** — 1 / 2 / 3
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (spring), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
