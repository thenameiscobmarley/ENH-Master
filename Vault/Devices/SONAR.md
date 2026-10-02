# TRANSIENT SHAPER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called SONAR.

A mastering simulation (3U, model SN-3). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

A transient shaper that finds each hit as a sonar finds an echo.

- **ATTACK**: -10 to +10. The start of each hit up or down (at most 9 dB).
- **SUSTAIN**: -10 to +10. What follows it.
- **SENSE**: 0 to 10. How small a hit it answers to.
- **MIX**: 0 to 100 %.

## Its screen

White on black. A sonar screen: the sweep going round and a ping for each hit (bigger for stronger hits), fading.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
