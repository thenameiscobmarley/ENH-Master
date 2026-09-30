# TESLA COIL

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The sound played by a singing Tesla coil (3U, model TC-1M). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

The arc switches on and off at the music's own pitch, but only while the sound is loud enough to strike.
Nothing plays over silence.

- **VOLTAGE**: 0 to 10. How easily it strikes.
- **BUZZ**: 0 to 10. The mains hum in the arc.
- **ARC**: 0 to 10. The spark's crackle.
- **TONE**: -5 to +5.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The coil and its toroid, with arcs striking out of it: as many and as far as it plays.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
