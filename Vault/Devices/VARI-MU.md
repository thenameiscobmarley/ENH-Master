# TUBE COMPRESSOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called VARI-MU.

A variable-mu valve compressor: the ratio rises as it works harder (gentle, then firm), a soft knee, valve harmonics, RECOVERY times or AUTO, and M/S to compress the middle and the sides apart. (2U, model VM-2.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **INPUT** — 0 – 10
- **THRESHOLD** — -40 – 0 dB
- **RECOVERY** — 0.1 / 0.3 / 0.6 / 1.2 / 2.0 / AUTO
- **GAIN** — 0 – 20 dB
- **M/S** — on / off
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (varimu), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
