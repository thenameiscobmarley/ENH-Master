# TONAL BALANCE TILT

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called BALANCE.

A mastering simulation (3U, model TB-1). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

The low end and the top on a see-saw. It weighs them over a few seconds against where a balanced master sits, and levels the beam with a gentle tilt EQ about 1 kHz (at most 6 dB either way).

- **TILT**: -5 to +5. A darker or brighter balance as the target.
- **SPEED**: 0 to 10. How quickly it weighs and moves.
- **AMOUNT**: 0 to 10. How far it corrects.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The see-saw: the low end's weight on the left, the top's on the right, and the counterweight sliding to level it.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
