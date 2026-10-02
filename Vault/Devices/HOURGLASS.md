# PROGRAM-DEPENDENT LIMITER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called HOURGLASS.

A mastering simulation (3U, model HG-1). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

A clean limiter whose release is an hourglass. Peaks run the sand down at once and it runs back over RELEASE. The longer it has been holding, the slower it returns: no pumping on dense material, quick on sparse hits.

- **CEILING**: -6 to 0 dB.
- **DRIVE**: 0 to 12 dB.
- **RELEASE**: 10 to 1000 ms.

## Its screen

White on black. The hourglass: the sand at the top, the pile below and the stream as it holds. The bar is how hard it's limiting.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
