# BBD ENSEMBLE

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A bucket-brigade chorus: delays swept by LFOs, through the chips' own filters, compander and clock hiss. CHORUS I, CHORUS II, or ENSEMBLE (three delays 120 degrees apart with a fast shimmer: the string-machine sound). (1U, model CE-3.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **RATE** — 0.1 – 10 Hz
- **DEPTH** — 0 – 10
- **MODE** — CHORUS I / CHORUS II / ENSEMBLE
- **NOISE** — 0 – 10
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (bbd), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
