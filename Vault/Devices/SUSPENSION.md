# DYNAMICS SMOOTHER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called SUSPENSION.

A mastering simulation (3U, model SU-2). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

Dynamics smoothed as a car's suspension smooths a road. The road is the sound's level; the car's body rides it on a spring and a damper. A sudden jump in level is soaked up, a dip is filled.

- **SPRING**: 0 to 10. How quickly the body follows.
- **DAMPER**: 0 to 10. How settled it is.
- **TRAVEL**: 0 to 12 dB. How far it may move.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The road (the level) scrolling by, the wheel on it and the body riding above on its spring.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
