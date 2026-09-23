# Harmonics and clarity

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

## Two modes

- **NORM** (0–30): balances the detail that's already there.
- **ADD + NORM** (0–10): also *creates* harmonics — new detail — where the sound has none.

20 in one mode isn't 20 in the other, so the printed scale swaps.

## How it decides

It measures how rich in harmonics the sound already is, then works out:

- **need** — how much help this sound wants;
- **rescue** — when a sound is already rich, it keeps a little lift going, so the harmonic stage
  always has something to work with (instead of switching off exactly when it's needed);
- **amount** — what actually gets made.

## Making them

The harmonics are made *relative to the sound's own level*, so a quiet passage and a loud one get the
same character. Where they go follows the sound, not a fixed frequency. Idle bands are skipped, which
saves most of the CPU.

Code: `HarmonicPlanner`, `SpectralAnalyzer`, `AnalogStage`. Related: [Adaptive EQ](Adaptive%20EQ.md).
