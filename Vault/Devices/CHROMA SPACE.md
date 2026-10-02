# SPACE & TONE PROCESSOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called CHROMA SPACE.

Space, tone and chops (3U, model CS-X). It starts in the [Gear locker](../UI/Gear%20locker.md), under Space.
**POWER** off: it doesn't touch the sound.

- **SPACE**: 0 to 10. From 0 to 5 a room that sits in a mix; from 6 to 10 it grows into a vast tail with a
  shimmer an octave up (about 25 seconds at 10). It always dies away.
- **TONE**: -10 to +10. On the whole sound: warm and dark (lows up, warmth in them) to bright and airy.
- **WIDTH**: 0 to 10. How wide the space is.
- **CHOP**: 0 to 10. How loud the chops play. At 0 nothing is captured.
- **SIZE**: 60 to 600 ms. Each chop's length.
- **VARIETY**: 0 to 10. How often a chop plays reversed or stutters.
- **AUTO AMT**: 0 to 10. How much the AUTO switches do.
- **MIX**: 0 to 100 %. How much space.
- **AUTO TIMING**: chops also play on the track's beat grid (eighths, sixteenths when it's busy) between hits,
  and when the melody moves.
- **AUTO TRICKS**: it picks reverse and stutter itself, more when the track is busy (instead of VARIETY).
- **AUTO LENGTH**: each chop an eighth note at the track's tempo (instead of SIZE).
- **AUTO SPACE**: the space opens in the gaps and draws in when the track is busy.

## The chops

When the track hits, the next SIZE of it is copied into a bank of six. This happens in memory only: nothing is
ever saved to a file, and the bank is overwritten as it goes. On later hits, and with AUTO TIMING on the beat grid,
one of them plays, retuned to the note playing now. It follows the melody as it moves. They play at most every so
often, so it never gets busy.

## Its screen

In colour: what just played as strands of light, warm colours when the sound is warm, cool when it's bright,
the note moving the colour. A box goes round each captured chop, brighter while it plays; the bank is along the
bottom.

Code: `DSP/units/Sims4.h`, the screen: `UI/Scene/ColourScreens.h`. Tests: `EnhDspTests --chroma`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
