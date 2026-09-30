# PENDULUM

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A real swinging pendulum that moves the sound (3U, model PT-2). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

It follows the physics: a longer pendulum swings slower, as a real one does (1 m: once every 2 s).

- **LENGTH**: 10 to 300 cm.
- **SWING**: 0 to 10. How far it's kept swinging (a clock's escapement tops it up).
- **FRICTION**: 0 to 10. How quickly it loses its swing between pushes.
- **MODE**: VOLUME (a dip at each end of the swing), PAN (left to right with it) or FILTER.
- **KICK**: 0 to 10. The track's hits push it, so it swings with the music.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The pendulum swinging, the range it's kept to, and what it moves: two speakers for PAN, a level bar for VOLUME, a filter curve for FILTER. It flashes when a hit kicks it.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
