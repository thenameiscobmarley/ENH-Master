# LATINSPHIEL PRO X4

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A smart four-band valve enhancer with a PID controller (4U). Designed by the owner in the
[Rack Unit Designer](https://thenameiscobmarley.github.io/ENH-Master/designer.html); the sound was built to
its panel. It starts in the [Gear locker](../UI/Gear%20locker.md): install it from there. It comes after
CHARACTER. **PWR** off: it doesn't touch the sound at all.

## The two halves

The left half is the left channel, the right half the right. Each has four bands (under 200 Hz,
200 Hz – 1 kHz, 1 – 5 kHz, over 5 kHz), each with its own valve:

- **DRIVE** (big knurled knob) — how hard the band's valve is driven. The valves are driven against each
  band's own level, so the colour is the same loud or quiet: 40 adds clearly audible warmth, 100 a lot.
- **TONE** (red pointer) — the band's level, −10 to +10 dB: a four-band EQ around the valves.
- **MIX** (blue cap) — how much of the valve's harmonics are added.

## MAIN

- **POPULATE** — how dense the added harmonics are (0 none, 5 as the valves make them, 10 twice).
- **SATURATE** — every valve driven harder or softer.
- **WIDEN** — the harmonics spread in stereo; the direct sound keeps its image.
- **CRISP** — the top two bands' harmonics brought forward (clarity, not a treble lift).
- The **dB+** meter shows the harmonic density.

## Switches

- **MONO** — the left half's settings for both sides, and a mono output.
- **X2** — the effect doubled.
- **PID** — holds the harmonic density steady. Each band's DRIVE knob then sets the target, and the PID
  turns the drive up or down (24 dB at most) to hold it, whatever the material. The DRIVE knobs turn
  on screen to show the drive it is really using (your setting comes back when PID is off).
  **PROPORTIONAL**, **INTEGRAL** and **DERIVATIVE** set how firmly, how persistently and how quickly it corrects.

The display shows each band's harmonic density (a bar) and the PID's target for it (a line). It comes out
as loud as it went in. No latency. The MIDI jacks on the panel are decoration.

Code: `DSP/ProX4.h`, `DSP/UnitKit.h`. Tests: `EnhDspTests --designed`.
