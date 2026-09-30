# BOUNCE DELAY

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Echoes like a dropped ball bouncing (3U, model BD-1). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

The first echo comes after HEIGHT. Each next one is sooner and quieter, until they run together and stop.

- **HEIGHT**: 50 to 1000 ms. The first bounce.
- **BOUNCE**: 0.30 to 0.95. How lively the ball is: each bounce this much sooner and quieter.
- **TONE**: -5 to +5.
- **SPREAD**: 0 to 10. Bounces thrown left and right.
- **MIX**: 0 to 100 %. The sound itself always stays.

## Its screen

White on black. The ball, dropped by the last hit, bouncing along. Each bounce is marked where it lands.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
