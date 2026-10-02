# MULTIBAND SATURATOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called PRISM.

A mastering simulation (3U, model PR-5). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

The sound split into five bands, each saturated as far as its own level drives it. The colour is the same at any volume.

- **DRIVE**: 0 to 10. All bands.
- **SPREAD**: 0 to 10. How different the colours are.
- **WARMTH**: 0 to 10. The low bands.
- **SHINE**: 0 to 10. The high bands.
- **MIX**: 0 to 100 %.

## Its screen

White on black. A prism: the sound enters as a white beam and leaves as five rays, each as bright as its band is saturated.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
