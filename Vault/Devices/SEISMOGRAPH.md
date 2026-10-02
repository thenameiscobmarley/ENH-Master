# LOW-END DAMPER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called SEISMOGRAPH.

A mastering simulation (3U, model SG-1). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

The low end's quakes, caught and damped. Below FREQ the sound is watched; past THRESHOLD it's held down. The rest is untouched.

- **THRESHOLD**: -40 to 0 dB.
- **DAMPING**: 0 to 10. From a light touch to a firm hand.
- **FREQ**: 40 to 200 Hz. Where the low end is split off.
- **MIX**: 0 to 100 %.

## Its screen

White on black. A seismograph's paper with the bass traced on it, the threshold lines, and the damper holding the pen.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
