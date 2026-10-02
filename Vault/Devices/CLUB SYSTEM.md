# CLUB SOUND SYSTEM TEST

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called CLUB SYSTEM.

A mastering simulation (3U, model CS-8). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

The master on a club's PA: its subs, the room, the crowd soaking up the top and the reverb, and where you stand.

- **SIZE**: 0 to 10. The room.
- **SUB**: 0 to 10. The subs.
- **CROWD**: 0 to 10. How full it is.
- **DISTANCE**: 0 to 10. How far back you stand.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The club from above: the stage and its stacks, the bass pressure rolling out, the crowd bobbing, and you.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
