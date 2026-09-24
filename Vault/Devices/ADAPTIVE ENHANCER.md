# ADAPTIVE ENHANCER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The black unit, second from the bottom. It listens to the sound and brings out what's missing.

## Knobs and buttons

- **CLARITY** + **MODE** — *NORM* (0–30) balances the detail that's there. *ADD + NORM* (0–10) also
  creates new detail (harmonics) where there's none. The printed scale changes with the mode.
- **ADAPT** — how fast it follows the sound (0–100 %).
- **SUB** + **+BOOST** — more low end.
- **METER** — **STEPS** lights flash with every footstep the [FOOTSTEP RADAR](FOOTSTEP%20RADAR.md) finds.
- **MULTIPLY / STRENGTH** — see [Your first sound](../Tutorial/02%20Your%20first%20sound.md).
- **PRESET ◀ ▶** — steps through the [presets](../Reference/Presets.md).

## Its screen

The EQ curve it's actually using, live. Each **precision band** is marked on it: a dot at its depth
(red for a cut, teal for a lift) and a bar across its width, so you can watch every band and its Q move. When the SPECTRAL LIMITER cuts, a magenta curtain shows where.

## What it does, in order

1. Measures the sound in 24 bands: what it's like *usually*, and what it's doing *now*.
2. Builds an EQ curve from that — no fixed "smile" ([Adaptive EQ](../DSP/Adaptive%20EQ.md)).
3. **Precision bands:** up to 8 moving bells, each with its own frequency, width (Q) and depth, found on
   a spectrum 24 steps per octave fine: a narrow dip where something rings, a wide one for a build-up, a
   gentle lift into a hole. The 24 fixed bands are too broad for these. Notes, hums and test tones are
   recognised as content and left alone. Glass panel **PRECISION**: 8
   bands (default), 4, or off.
4. Adds harmonics in ADD mode ([Harmonics and clarity](../DSP/Harmonics%20and%20clarity.md)).
5. Adds sub, then keeps the level the same as it came in.

Footsteps have their own unit now: the [FOOTSTEP RADAR](FOOTSTEP%20RADAR.md).

Code: `AdaptiveEQ`, `PrecisionEQ`, `AnalogStage`, `SubEnhancer`, `HarmonicPlanner`. Test: `EnhDspTests --precision`.
