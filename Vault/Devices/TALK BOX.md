# TALK BOX FORMANT FILTER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called TALK BOX.

The sound through a mouth (3U, model TB-3). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

Three formants (the resonances that make a vowel) move between A, E, I, O and U.

- **VOWEL**: 0 to 4: A, E, I, O, U.
- **MOVE**: 0 to 10. The sound's own level opens it through the vowels, so it talks with the playing.
- **RATE**: 0 to 10. Sweeps the vowels on its own.
- **SIZE**: -5 to +5. A smaller or bigger mouth.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The mouth from the front, shaping each vowel, the vowel it's at along the bottom, and its three formants as peaks on the right.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
