# LOUDNESS MAXIMIZER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called PRESSURE.

A mastering simulation (3U, model PV-24). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

A loudness maximizer as a pressure vessel. The sound is pumped in, rounds off as it fills, and the valve vents anything that would pass it. Nothing ever passes the valve.

- **PRESSURE**: 0 to 24 dB. The gain in.
- **VALVE**: -6 to 0 dB. The ceiling.
- **RELEASE**: 10 to 500 ms. How quickly the valve eases shut.
- **CHARACTER**: 0 to 10. How it rounds off as it fills (soft saturation).
- **MIX**: 0 to 100 %.

## Its screen

White on black. The tank filling, its gauge, and the valve on top venting puffs as it holds.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
