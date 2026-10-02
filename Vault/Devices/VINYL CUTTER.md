# VINYL MASTERING SIMULATOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called VINYL CUTTER.

A mastering simulation (3U, model VC-70). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

What a record-cutting lathe allows a master. The bass is made mono (side-to-side bass would throw the stylus out of the groove), the top is held back when it's too much, and the groove is cut to a depth.

- **MONO BASS**: 50 to 300 Hz. Below this, no stereo.
- **HF LIMIT**: 0 to 10. How much treble the cutter allows (cut only when it's too loud).
- **DEPTH**: 0 to 10. How hard it's cut.
- **MIX**: 0 to 100 %.

## Its screen

White on black. The groove being cut: its wiggle is the sound, its width the stereo, the cutting stylus at its end. The bar is the treble being held back.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
