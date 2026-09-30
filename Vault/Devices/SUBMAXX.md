# SUBMAXX

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Bass you hear on small speakers: the lows are turned into their harmonics (our ears rebuild the missing fundamental), kept to the 2nd - 5th and added; ORIGINAL turns the real lows down to spare the speaker. (1U, model SMX.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **FREQUENCY** — 40 – 150 Hz
- **HARMONICS** — 0 – 10
- **ORIGINAL** — -24 – 0 dB
- **OUTPUT** — -12 – 6 dB

Its LED ladder (and its display, on a 2U) shows how hard it is working. It fades in and out with POWER (no click), and its output never passes +6 dBFS.

Code: `DSP/units/` (submaxx), panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
