# CAR LISTENING TEST

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called CAR TEST.

A mastering simulation (3U, model CT-4). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

The master as it sounds in a car: the cabin's bass lift and its low resonances, the speakers, the seat you're in, and road noise.

- **CABIN**: 0 to 10. The cabin's size and boom.
- **SPEAKERS**: DOOR (bass-heavy, a hole in the low mids), DASH (thin and bright) or PREMIUM (with a sub).
- **ROAD**: 0 to 10. Road noise.
- **SEAT**: DRIVER (the near side louder and first), MIDDLE or BACK.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The car from above: its seats (yours lit), its speakers pulsing, the cabin's boom, the road going by.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
