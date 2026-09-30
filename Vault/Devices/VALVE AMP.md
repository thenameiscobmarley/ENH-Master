# VALVE AMP

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A valve amplifier (3U, model VA-50). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

A triode preamp into a push-pull power stage, then an output transformer. The power supply sags as it's
worked: the harder it plays, the less headroom it has. That's the bloom and squash of a valve amp.

- **DRIVE**: 0 to 10. How hard the preamp is driven.
- **BIAS**: 0 to 10. Where the valve works: it changes the even harmonics.
- **SAG**: 0 to 10. How much the supply gives way when it's worked.
- **TONE**: -5 to +5. Darker or brighter, between the two stages.
- **OUTPUT**: -12 to +12 dB.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The two valves, glowing as hard as they work, and the supply rail dipping as it sags. The bar under them is DRIVE.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
