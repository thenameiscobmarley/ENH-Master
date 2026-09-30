# TAPE ECHO

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A tape-loop echo (3U, model TE-201). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

A loop of tape runs past a record head and three playback heads. The heads are 1, 2 and 3 times TIME apart.
What comes back is recorded again. Each pass comes back a little duller and warmer.

- **TIME**: 50 to 800 ms. Change it while it plays and the pitch sweeps, like a tape motor changing speed.
- **INTENSITY**: 0 to 110 %. How much goes round again. Past 100 % it runs away, but the tape's own
  saturation holds it.
- **HEADS**: 1, 2, 3, 1+2, 2+3 or ALL.
- **WOW**: 0 to 10. The tape wavers.
- **TONE**: -5 to +5. Darker or brighter repeats.
- **MIX**: 0 to 100 %. How much echo is added. The sound itself always stays.

## Its screen

The tape loop, the record head (the big one) and the three playback heads. The heads HEADS uses are
lit. Each echo is a mark carried round the loop, and it flares as it passes a lit head. The bar in the
middle is INTENSITY, with 100 % marked.

Code: `DSP/units/Sims.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
