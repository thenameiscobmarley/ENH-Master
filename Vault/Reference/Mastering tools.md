# Mastering tools

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

## COMPARE (A/B)

A button on the OUTPUT MONITOR, and **A/B** in the router app. While it's on you hear your input
**untouched**, but:

- **in time** — delayed exactly like the rack, so it lines up;
- **at the same loudness** — so louder never wins the comparison;
- **smoothly** — a 30 ms crossfade, no click.

## STEREO (mid/side)

In the glass panel of six units (COMPRESSOR, LEVELER, SPECTRAL LIMITER, MIX BALANCER, TONE & SPACE,
CHARACTER):

| Setting | The unit works on |
|---|---|
| L/R | left and right (default) |
| MID | only the middle; the sides pass untouched |
| SIDE | only the sides; the middle passes untouched |

The untouched part stays in time with the processed part.

Code: `EnhEngine::compareStage`, `EnhEngine::inStereoMode`. Test: `EnhDspTests --mastering`.
