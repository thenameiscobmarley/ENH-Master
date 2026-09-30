# OVERCLIP

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A clipper run four times faster than the audio, so what it cuts doesn't fold back as aliasing. DRIVE in, CEILING out, KNEE rounds the corner; SOFT, HARD or TUBE (asymmetric). (1U, model OC-4X.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **DRIVE** — 0 – 24 dB
- **CEILING** — -12 – 0 dB
- **KNEE** — 0 – 10
- **MODE** — SOFT / HARD / TUBE
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (clip), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
