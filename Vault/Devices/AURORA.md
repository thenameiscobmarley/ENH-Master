# AIR BAND EXCITER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called AURORA.

A mastering simulation (3U, model AU-1). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

Air over the top. The upper presence drives a generator of harmonics above SHEEN, laid over the sound. SMOOTH holds it back when the top is already bright, so it never turns harsh.

- **AIR**: 0 to 10.
- **SHEEN**: 5 to 16 kHz. Where the air starts.
- **SMOOTH**: 0 to 10.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The northern lights: curtains as bright as the air they add, over the hills.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
