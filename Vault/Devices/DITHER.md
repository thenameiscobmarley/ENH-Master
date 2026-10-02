# DITHER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A mastering simulation (3U, model DQ-16). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

The last step of a master: reduced to 16, 20 or 24 bits, with dither. Put it last.

- **BITS**: 16, 20 or 24.
- **DITHER**: OFF (the rounding left as distortion), TPDF (a flat, gentle hiss) or SHAPED (the hiss moved to where the ear hears least).

## Its screen

White on black. The last few samples, zoomed in to the last bit: the smooth signal and the steps it leaves as.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
