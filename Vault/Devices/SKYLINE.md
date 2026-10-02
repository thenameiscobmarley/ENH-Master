# RESONANCE SUPPRESSOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called SKYLINE.

A mastering simulation (3U, model SK-16). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

Resonances trimmed back. The spectrum is a skyline of 16 bands; any tower standing well above its neighbours (a ringing note) is trimmed back toward them.

- **DEPTH**: 0 to 10. Up to 12 dB.
- **SHARPNESS**: 0 to 10. How narrow a peak it looks for.
- **SPEED**: 0 to 10.
- **FOCUS**: FULL, MIDS or HIGHS.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The skyline: each band a building, what's trimmed off drawn dashed.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
