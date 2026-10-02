# OPTICAL COMPRESSOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called T4 OPTO.

An optical leveler: a light panel and a photocell. The cell lets go in two stages - fast, then slowly, slower the longer it has been lit - the smooth, program-dependent release leveling amps are loved for. EMPHASIS makes it hear treble first. (1U, model LA-T4.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **PEAK REDUCTION** — 0 – 100
- **GAIN** — 0 – 30 dB
- **LIMIT** — on / off
- **EMPHASIS** — 0 – 10
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (opto), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
