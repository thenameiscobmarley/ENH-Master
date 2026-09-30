# PLATE 140

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Jon Dattorro's plate reverb (1997): pre-delay, four diffusers, then the figure-eight tank with modulated allpasses and damping, read out at many taps. Bright, dense, classic. (1U, model EMT-STYLE.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **DECAY** — 0.2 – 8 s
- **PRE-DELAY** — 0 – 120 ms
- **DAMPING** — 0 – 10
- **SIZE** — 0 – 10
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (plate), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
