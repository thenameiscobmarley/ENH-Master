# UPWARD COMPRESSOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called UPWARD LEVELER.

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

## What keeps it from brightening

The top band (above 2.2 kHz) only rises with the midrange, plus a little (BAND BALANCE: 0.5, 1.5 or
3 dB). Music's treble sits well under its mids; lifted on its own, it made a mix brighter than it was made
and tiring after a while. Quiet detail still comes up, with the midrange where footsteps and voices are.

## In its glass panel

**LIFT** (standard, gentle, big) · **GATE** (how quiet is "too quiet to lift") · **BAND BALANCE** ·
**STEREO** (L/R, mid or sides). See [Methods](../Reference/Methods.md).

Code: `SpectralLeveler.h/.cpp`.
