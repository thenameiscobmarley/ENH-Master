# Harmonics and clarity

`Source/DSP/SpectralAnalyzer.{h,cpp}`, `Source/DSP/HarmonicPlanner.{h,cpp}`

## The two modes

CLARITY does different work depending on MODE:

- **NORM** (0-30): normalise what is there. Detail is balanced where the analysis finds it.
- **ADD + NORM** (0-10): the same, plus harmonics generated from the incoming audio.

The knob's printed scale swaps with the mode, because 20 in one mode is not 20 in the other.

## How the planner decides

An STFT tonality measure says how harmonically rich the source already is. From that the planner
computes:

- **need** - how much help this source wants;
- **rescue** - the interesting case: when harmonics are already high, clarity would normally back
  off, which starves the next stage of material. Instead it *keeps* lifting so the harmonic generator
  has something to work from, and if the harmonic content starts falling it adds some back;
- **amount** - what actually gets generated, coupling the two.

This is what "make clarity follow better" meant in practice: it tracks the source instead of
switching itself off at the moment it is needed.

## Generating them

Envelope-normalised Chebyshev shaping (T2/T3) - the harmonics are generated relative to the envelope,
so **the result does not depend on how loud the input is**. A quiet passage and a loud one get the
same character, which is the difference between an effect and an accident. The exciters are skipped
entirely when the band is idle, which is most of the CPU saving.

Where they go is chosen by the analysis, not by a fixed frequency: the centres move with the source
and are printed every 5 s by `--diagnose` ([[Dev hooks]]).

Related: [[Adaptive EQ]], [[Seraph stages]].
