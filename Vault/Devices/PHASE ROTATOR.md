# PHASE ROTATOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A broadcast phase rotator: allpasses turn the phase of each frequency differently, so lopsided waveforms (voices especially) even out - the same sound and spectrum, a few dB less peak. Never mixed with the dry sound, so no comb filtering. (1U, model PR-16.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **STAGES** — 4 / 8 / 16
- **FREQUENCY** — 60 – 2000 Hz
- **AMOUNT** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (rotator), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
