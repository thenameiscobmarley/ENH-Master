# VINYL DECK

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The sound played from a record (3U, model TT-12). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

- **SPEED**: 33, 45 or 78. How fast the record turns. The dust ticks and the wow come round once a turn,
  so they're quicker at 45 and 78. A 78 is also narrower, more mono and noisier.
- **WEAR**: 0 to 10. A worn stylus: the top end breaks up.
- **DUST**: 0 to 10. How many specks sit on the record (up to 24). Each ticks every time the stylus
  passes it: once a turn, like a real record. A few are replaced now and then. Loose crackle too, at high settings.
- **WOW**: 0 to 10. The record sits off centre: the pitch bends once a turn.
- **AGE**: 0 to 10. Less top, more hiss and rumble, and it folds toward mono.
- **MIX**: 0 to 100 %.

The surface (hiss, ticks, rumble) plays even over silence, as a record does. It stays quiet.

## Its screen

White on black: the record from above, turning, with its grooves and label. The dust turns with it and
flashes as the stylus passes. The tonearm works slowly in toward the label. The lit mark on the right
is the speed.

Code: `DSP/units/Sims.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
