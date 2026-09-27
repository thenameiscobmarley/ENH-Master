# VELVETIZER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Smooths sound the way good analog gear does, then colours it (2U). Designed by the owner in the
[Rack Unit Designer](https://thenameiscobmarley.github.io/ENH-Master/designer.html). It starts in the
[Gear locker](../UI/Gear%20locker.md). It comes after the PRO X4. **POWER** off or **BYPASS** in: it
doesn't touch the sound at all.

- **VELVET LOW / MID / HIGH** (under 250 Hz, 250 Hz – 3.5 kHz, over 3.5 kHz) — how much each band is
  smoothed: its spiky transients rounded off, and a soft analog saturation that thickens it.
- **GRAIN** — texture: fine, quiet harmonics, like tape and valves.
- **CRISP** — clarity given back on top: the attack of the treble, not its level.
- **COLOR TYPE A** and **B** — TUBE, TAPE, TRANSFORMER, CONSOLE, TRANSISTOR or CRYSTAL (clean), and
  **BALANCE** between them.
- **ADD** puts the warmth on top of the sound (a little fuller, about 1 dB louder). **BALANCE**
  rebalances what is there, just as loud.
- The **VELVET dB+** meter shows how much it is smoothing.

About 3–4 % harmonics on a loud tone at the defaults: warm, never fuzzy. No latency.

Code: `DSP/Velvetizer.h`, `DSP/UnitKit.h`. Tests: `EnhDspTests --designed`.
