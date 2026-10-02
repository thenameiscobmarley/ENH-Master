# CLARITY ENHANCER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called CLARITY LENS.

A mastering simulation (3U, model CL-1). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

Brings forward the presence the rest of the mix is hiding. It watches how loud the presence band (around FOCUS) is against the whole sound. The more it's buried, the more it's lifted.

- **CLARITY**: 0 to 10. How far it may lift a buried presence (up to about 9 dB).
- **FOCUS**: 1 to 6 kHz. Where the presence is.
- **AIR**: 0 to 10. Opens the very top.
- **SAFE**: 0 to 10. Backs off where the presence is already strong, so it never turns harsh.
- **MIX**: 0 to 100 %.

## Its screen

White on black. Light through a lens: the bands as rays (thicker when louder), focused to a point that draws nearer as it lifts.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
