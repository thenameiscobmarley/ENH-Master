# THERMAL SATURATOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called FURNACE.

A mastering simulation (3U, model FN-9). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

Warmth that builds with heat. The sound heats it up and it cools on its own. The hotter it runs, the harder it saturates, the darker it gets and the more it gives, like a valve or a voice coil heating.

- **HEAT**: 0 to 10. How much the sound heats it.
- **COOLING**: 0 to 10. How quickly it cools.
- **COLOUR**: 0 to 10. How much it darkens when hot.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The furnace, its flames as high as it's hot, and a thermometer.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
