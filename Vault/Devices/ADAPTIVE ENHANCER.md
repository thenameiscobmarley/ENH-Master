# ADAPTIVE ENHANCER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The black unit, second from the bottom. It listens to the sound and brings out what's missing.

## Knobs and buttons

- **CLARITY** + **MODE** — *NORM* (0–30) balances the detail that's there. *ADD + NORM* (0–10) also
  creates new detail (harmonics) where there's none. The printed scale changes with the mode.
- **ADAPT** — how fast it follows the sound (0–100 %).
- **SUB** + **+BOOST** — more low end.
- **FOOTSTEP** — lifts recognised footsteps. **DETECT** lights flash when one is found.
- **MULTIPLY / STRENGTH** — see [Your first sound](../Tutorial/02%20Your%20first%20sound.md).
- **PRESET ◀ ▶** — steps through the [presets](../Reference/Presets.md).

## Its screen

The EQ curve it's actually using, live. When the SPECTRAL LIMITER cuts, a magenta curtain shows where.

## What it does, in order

1. Measures the sound in 24 bands: what it's like *usually*, and what it's doing *now*.
2. Builds an EQ curve from that — no fixed "smile" ([Adaptive EQ](../DSP/Adaptive%20EQ.md)).
3. Adds harmonics in ADD mode ([Harmonics and clarity](../DSP/Harmonics%20and%20clarity.md)).
4. Adds sub, then keeps the level the same as it came in.

Code: `AdaptiveEQ`, `AnalogStage`, `SubEnhancer`, `FootstepDetector`, `HarmonicPlanner`.
