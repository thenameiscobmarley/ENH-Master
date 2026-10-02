# SPEAKER CABINET SIMULATOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called SPEAKER CAB.

A speaker in its cabinet and the mic in front of it (3U, model SC-412). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

A closed cabinet: nothing below the speaker's resonance (with a bump at it), and the top falling away
where the cone breaks up.

- **SIZE**: 10, 12 or 15 inch. Bigger goes lower and is darker.
- **MIC POS**: 0 to 10. From the dust cap (bright) to the edge (darker, rounder).
- **DISTANCE**: 0 to 10. The mic backed off: the floor's reflection and a little room come in.
- **ROOM**: 0 to 10. The room around it.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The speaker from the front, its cone moving with the sound, and the mic where it is. On the right, the same from the side, the mic as far off as DISTANCE.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
