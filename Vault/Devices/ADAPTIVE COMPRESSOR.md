# ADAPTIVE COMPRESSOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Evens out the level: loud parts come down a little, so the whole thing sits more steadily. Its
threshold follows the music, so it works the same on a quiet game and a loud song.

## Knobs

- **MIX** — how much compressed sound is blended with the original (60 % by default).
- **RESPONSE** — slow and smooth, or quick and tight.
- **IN** — on or off.

## What makes it gentle

- It ignores bass hits the SPECTRAL LIMITER is already handling.
- A **dual release**: it lets go fast after a short hit, and slowly after a long loud part, so it doesn't pump.
- It measures each channel, so a wide mix is treated like a mono one.

## In its glass panel

**DETECTOR** (peak, RMS, loudness-weighted) · **SIDE-CHAIN** · **GAIN** curve · **SMOOTHING** ·
**MAKE-UP** · **RESPONSE** law · **STEREO**. See [Methods](../Reference/Methods.md).

Code: `DynamicCompressor.h/.cpp`.
