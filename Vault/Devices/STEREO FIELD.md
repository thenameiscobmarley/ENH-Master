# STEREO WIDTH CONTROL

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called STEREO FIELD.

A mastering simulation (3U, model SF-2). It starts in the [Gear locker](../UI/Gear%20locker.md), under Simulated, Mastering simulations. **POWER** off: it doesn't touch the sound.

Width as the field between two poles. WIDTH spreads or narrows the image, the bass is kept mono, and SAFE pulls the width back when left and right start to cancel.

- **WIDTH**: 0 to 200 %. 0 is mono.
- **BASS MONO**: 20 to 300 Hz. Below this, no sides.
- **FOCUS**: 0 to 10. Firms up the centre.
- **SAFE**: 0 to 10. Mono safety.
- **MIX**: 0 to 100 %.

## Its screen

White on black. Field lines between the left and right poles, spreading with the width and breaking up when the sides cancel. The bar is the correlation.

Code: `DSP/units/Sims3.h`, the screen: `UI/Scene/SimScreens.h`. Tests: `EnhDspTests --units16`.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
