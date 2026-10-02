# SUBWOOFER SIMULATOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called SUB DRIVER.

A mastering simulation (3U, model SD-18). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

The lows played through a modelled subwoofer: a cone on its suspension, tuned to TUNE. Below its tuning the sub rolls away cleanly. At XMAX the suspension stiffens and holds the cone, for a tight low end.

- **SUB**: 0 to 10. How much of the sub goes into the sound.
- **TUNE**: 30 to 80 Hz. The cone's tuning.
- **XMAX**: 0 to 10. How far the cone may move.
- **TIGHT**: 0 to 10. Loose and ringing at 0, tight at 10.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The sub from the side: the magnet, the voice coil and the cone moving, the XMAX limits, and the air it pushes.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
