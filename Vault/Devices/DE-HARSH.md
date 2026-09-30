# DE-HARSH

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A dynamic harsh remover. Units that keep adapting (compressors, AUTO, adaptive EQs) move the level many
times a second. In the presence region (2–9 kHz), where ears are most sensitive, that constant flicker is
heard as harshness, even when the balance is right. DE-HARSH evens it out, and takes down harsh moments.
(1U, model DH-4.) It starts in the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

Put it **last**, after the units that adapt.

- **SMOOTH** — 0 – 10: how much of the flicker is evened out. Four bands around FOCUS each follow their
  own average; where a band jumps above it, it is taken down, and where it dips, lifted back a little.
- **DEPTH** — 0 – 12 dB: the most it may take away (it lifts by half of this at most).
- **SENSE** — 0 – 10: how far the presence may stand above the mids before it counts as harsh. Higher
  catches more.
- **FOCUS** — 2 – 8 kHz: where the four bands sit (half an octave apart).
- **AIR** — 0 – 10: softens above 10 kHz along with the cut (fizz rides along with harshness).
- **LISTEN** — hear only what it takes away.
- **MIX** — 0 – 100%

Tested: a 3.5 kHz tone jumping 8 dB 20 times a second moves half as much afterwards; a top 18 dB over
the mids comes down 4 dB (DEPTH 4); a balanced, steady mix is left alone.

Its LED ladder shows how much it is taking away. It fades in and out with POWER (no click), and its
output never passes +6 dBFS.

Code: `DSP/units/Harsh.h`, panel: `Tools/units/gen_units.py`. Tests: `EnhDspTests --units16`.
