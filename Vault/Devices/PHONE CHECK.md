# SMALL SPEAKER TEST

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called PHONE CHECK.

A mastering simulation (3U, model PC-2). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

The master through a phone, a laptop or earbuds, at a volume where the device's protection limiter squashes it, as a phone does turned up.

- **DEVICE**: PHONE (nothing under 600 Hz, a 2.5 kHz peak), LAPTOP or EARBUDS.
- **VOLUME**: 0 to 10. How hard the device is played.
- **MONO**: A phone plays in mono.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The device, its speaker, and the wave coming out, its tops squashed as far as the limiter squashes them.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
