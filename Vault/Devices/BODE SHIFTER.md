# BODE SHIFTER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A frequency shifter: every frequency moved by the same number of Hertz (not a pitch shift), from a Hilbert transform. A few Hertz: a slow swirl; more: bells and robots; FEEDBACK: spirals. (1U, model BS-1630.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **SHIFT** — -500 – 500 Hz
- **FEEDBACK** — 0 – 90%
- **SPREAD** — 0 – 50 Hz
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (bode), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
