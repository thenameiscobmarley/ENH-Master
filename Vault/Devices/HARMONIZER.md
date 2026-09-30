# HARMONIZER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A two-voice pitch shifter: each voice up or down to an octave, DETUNE pulling them apart, a DELAY and FEEDBACK round them - the studio harmonizer, voice 1 left, voice 2 right. (2U, model H-910.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **VOICE 1** — -12 – 12 st
- **VOICE 2** — -12 – 12 st
- **DETUNE** — 0 – 50 ct
- **DELAY** — 0 – 100 ms
- **FEEDBACK** — 0 – 80%
- **MIX** — 0 – 100%

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (harm), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
