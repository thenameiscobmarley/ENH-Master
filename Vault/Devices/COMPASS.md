# STEREO PHASE ALIGNER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called COMPASS.

A mastering simulation (3U, model PA-1). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

Finds how far left and right have drifted apart in time (a mic further off, a track nudged) and brings them back into line, on the bass only or the full range.

- **ALIGN**: 0 to 10. How far it corrects.
- **RANGE**: 0.1 to 2 ms. How far it looks.
- **BAND**: FULL or BASS.
- **MIX**: 0 to 100 %.

## Its screen

White on black. A compass: north is in phase, south is cancelling. Beside it, the drift it found and how much is corrected.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
