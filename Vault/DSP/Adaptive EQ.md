# Adaptive EQ

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The ADAPTIVE ENHANCER's EQ curve is **worked out from the sound**, all the time. There's no fixed
"smile" curve that scoops the mids on everything.

## Where the curve comes from

- **Balance** — treble and bass compared to the mids (the mids are the anchor, never scooped).
- **Local fixes** — a ringing spot is cut, a hole is filled, right where it is.
- **Bursts** — short events get a moment of help.
- **Footsteps** — lift where a found footstep carries its detail, dip what's masking it.
- **ADD mode** — room for generated harmonics.

A guard stops it ever pulling the mids down far. The 24 bands overlap, so their gains are solved
together (least squares) instead of added up and overshooting.

## What the tests check

- Pink noise in → flat out (within ±0.1 dB).
- A ringing spot or hole is fixed *where it is*.
- 1–4 kHz is never scooped.
- A dull sound gets brighter, not "empty".

Code: `AdaptiveEQ.h/.cpp`. Related: [Harmonics and clarity](Harmonics%20and%20clarity.md).
