# ROBOVOX

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A 16-band vocoder: the sound's levels, band by band, open the same bands of a carrier made here - a saw, a pulse, noise or a chord at NOTE. FORMANT moves the carrier's bands against the voice. (2U, model VX-16.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **NOTE** — 24 – 72
- **CARRIER** — SAW / PULSE / NOISE / CHORD
- **FORMANT** — -12 – 12 st
- **RELEASE** — 5 – 200 ms
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (vocoder), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
