# SYMPATHETIC RESONATOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called SYMPATHY.

Six sympathetic strings (3U, model SR-6). It starts in the [Gear locker](../UI/Gear%20locker.md).
**POWER** off: it doesn't touch the sound.

Strings tuned to a chord, left open, so they ring along with the sound like a sitar's or a piano's with the
pedal down. Each is a plucked-string model.

- **NOTE**: 28 to 64 (MIDI note). The chord's root.
- **CHORD**: OPEN, MAJOR, MINOR, FIFTHS or OCTAVES.
- **DECAY**: 0 to 10. How long they ring.
- **BRIGHT**: 0 to 10. How much top they keep.
- **MIX**: 0 to 100 %. The sound itself always stays.

## Its screen

White on black. The six strings between nut and bridge, each vibrating as much as it rings.

Code: `DSP/units/Sims2.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.
