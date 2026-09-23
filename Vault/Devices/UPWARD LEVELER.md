# UPWARD LEVELER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Lifts quiet sounds — quiet dialogue, distant footsteps, soft passages — in three bands (low, mid,
high), so a quiet mid can come up even under loud bass. Loud sounds are left alone.

## Knobs

- **TARGET** (−36 … −6 dB) — how loud quiet sounds are brought up to.
- **RESPONSE** (0–10) — how quickly it moves.
- **IN** — on or off.

## What keeps it from pumping

- It ignores steady hiss and room noise (it only lifts sound that moves).
- It moves slowly, and waits a moment after a loud part before lifting again.
- When a sound starts after silence, it listens first before lifting — no sudden jump.
- It measures each channel, so wide and out-of-phase sounds are treated like mono ones.
- While the SPECTRAL LIMITER handles a bass hit, it holds still.

## In its glass panel

**LIFT** (standard, gentle, big) · **GATE** (how quiet is "too quiet to lift") · **BAND BALANCE** ·
**STEREO** (L/R, mid or sides). See [Methods](../Reference/Methods.md).

Code: `SpectralLeveler.h/.cpp`.
