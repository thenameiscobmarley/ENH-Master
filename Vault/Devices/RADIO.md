# RADIO SIMULATOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called RADIO.

The sound as a radio receives it (3U, model AM-5). Mono, as a radio is. It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

- **BAND**: AM (150 Hz to 4.5 kHz), SW (narrower, and it fades) or FM (wide, a little hiss).
- **TUNE**: -10 to +10. Off the station either way: the signal weakens, static rises and a whistle comes in.
- **STATIC**: 0 to 10. Crackle and hiss.
- **FADE**: 0 to 10. The signal coming and going (short wave most of all).
- **SPEAKER**: 0 to 10. The set's small speaker: a honk, and it breaks up.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The dial with its needle (the station is the mark in the middle), what it receives (clean on the station, lost in static off it), and the signal strength.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
