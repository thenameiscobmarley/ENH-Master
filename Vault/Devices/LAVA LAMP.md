# RESONANT FILTER BANK

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called LAVA LAMP.

A lava lamp as a filter (3U, model LL-60). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

Blobs of wax, heated at the bottom, rise; cooled at the top, they sink. Each blob is a resonance in the
sound, as high as the blob is, on the side of the lamp it's on. Slow filter movement that never repeats.

- **HEAT**: 0 to 10. How fast the wax moves.
- **BLOBS**: 1 to 6.
- **DEPTH**: 0 to 10. How strong the resonances are.
- **CENTRE**: 200 to 4000 Hz. Where the middle of the lamp sits.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The lamp and its blobs where they are (warmer is brighter).

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
