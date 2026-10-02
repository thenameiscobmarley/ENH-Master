# AUTOMATIC FADER RIDER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called RIDER.

A mastering simulation (3U, model FR-1). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

An engineer's hand on the fader, riding the master toward a loudness. Slowly, as a person does, never more than RANGE either way, and it holds in silence.

- **TARGET**: -24 to -6 LUFS.
- **SPEED**: 0 to 10.
- **RANGE**: 0 to 12 dB. How far it may ride.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The fader moving, and the loudness it's riding against the target line.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
