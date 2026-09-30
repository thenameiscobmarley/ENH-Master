# CASSETTE DECK

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The sound recorded to cassette and played back (3U, model CD-3). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

- **TAPE**: I, II or IV. Type I is warm, saturates early and loses the top first. Type II is brighter.
  Type IV (metal) has the most headroom.
- **WOW**: 0 to 10. A slow drift in pitch.
- **FLUTTER**: 0 to 10. A quick shimmer in pitch.
- **HISS**: 0 to 10.
- **DROPOUTS**: 0 to 10. Now and then the tape lifts off the head for a moment: quieter and duller.
- **SATURATE**: 0 to 10. How hard it's recorded. The colour is the same at any volume.
- **STOP**: the deck stops. The tape winds down (the pitch falls) to silence. Switch it back and the
  tape starts up again and fades back into the live sound.

## Its screen

The cassette: the two reels (the tape moving from one to the other), the tape path and the head. The
head glows with the level. In a dropout the tape lifts off it. With STOP everything slows to a halt.
The bars top right are the tape type.

Code: `DSP/units/Sims.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
