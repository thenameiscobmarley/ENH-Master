# DOPPLER FLYBY

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called FLYBY.

The sound moving past you (3U, model DP-9). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

Everything comes from where the sound is: the delay of its travel (the pitch rises coming and falls
going: Doppler), the level, the air (further is duller) and the side it's on.

- **SPEED**: 5 to 100 m/s.
- **DISTANCE**: 1 to 50 m. How close it passes.
- **PATH**: LINE (going by again and again), CIRCLE (in front of you) or EIGHT.
- **AIR**: 0 to 10. How much the air dulls it with distance.
- **MIX**: 0 to 100 %.

## Its screen

White on black. From above: the path, you, and the sound with the wavefronts it leaves behind. They bunch up ahead of it and spread out behind: that's the Doppler effect.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
