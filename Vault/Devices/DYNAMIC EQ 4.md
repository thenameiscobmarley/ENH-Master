# DYNAMIC EQ 4

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Four bells that move with the music: each listens to its own frequency and, over its THRESH, turns by up to its RANGE - cut a harsh resonance only when it rings, lift a band only when it's there. (2U, model DQ-4.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **FREQ 1** — 40 – 400 Hz
- **THRESH 1** — -60 – 0 dB
- **RANGE 1** — -12 – 12 dB
- **FREQ 2** — 200 – 2000 Hz
- **THRESH 2** — -60 – 0 dB
- **RANGE 2** — -12 – 12 dB
- **FREQ 3** — 1000 – 8000 Hz
- **THRESH 3** — -60 – 0 dB
- **RANGE 3** — -12 – 12 dB
- **FREQ 4** — 4000 – 16000 Hz
- **THRESH 4** — -60 – 0 dB
- **RANGE 4** — -12 – 12 dB

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (dyneq), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
