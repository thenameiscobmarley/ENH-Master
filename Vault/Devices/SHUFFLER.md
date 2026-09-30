# SHUFFLER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Blumlein stereo shuffling: the sides' low end widened more than their top (below ~700 Hz we judge direction by phase and the image narrows), the bass made mono below BASS MONO, and MID / SIDE levels. (1U, model SF-B.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **WIDTH** — 0 – 200%
- **SHUFFLE** — 200 – 3000 Hz
- **LOW WIDTH** — 0 – 200%
- **BASS MONO** — 20 – 300 Hz
- **MID** — -6 – 6 dB
- **SIDE** — -6 – 6 dB

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (shuffler), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
