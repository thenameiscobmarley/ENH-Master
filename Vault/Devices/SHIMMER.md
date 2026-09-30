# SHIMMER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

An 8-line feedback delay network reverb (every line feeds every other through a Hadamard matrix, so the tail is dense and never metallic), with a pitch shifter in its loop: each pass climbs an octave (or +7, +19, -12), the choir-like shimmer. (2U, model SH-8.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **SIZE** — 0 – 10
- **DECAY** — 0.3 – 20 s
- **SHIMMER** — 0 – 100%
- **PITCH** — +12 / +7 / +19 / -12
- **DAMP** — 0 – 10
- **MOD** — 0 – 10
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (shimmer), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
