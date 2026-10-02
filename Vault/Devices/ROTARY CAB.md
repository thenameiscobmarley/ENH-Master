# ROTARY SPEAKER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called ROTARY CAB.

A rotating-speaker cabinet (3U, model RC-147). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

The sound splits at 800 Hz. The top goes to a spinning horn and the lows to a spinning drum. As each
turns toward and away from the two mics, the pitch rises and falls (Doppler) and the level swells and
fades. The mics are a quarter turn apart, so the sound moves in stereo.

- **SPEED**: SLOW or FAST. Switch it and the rotors speed up or slow down. They don't jump.
- **ACCEL**: 0 to 10. How long they take to get there. The heavy drum always takes longer than the horn.
- **DRIVE**: 0 to 10. The valve amplifier in the cabinet.
- **DISTANCE**: 0 to 10. The mics further off: less movement.
- **MIX**: 0 to 100 %.

## Its screen

The cabinet from above: the horn with its flared mouth turning over the drum, and the two mics. The
horn blurs when it's fast. The bar along the bottom is the horn's speed.

Code: `DSP/units/Sims.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
