# Parameter mapping

`Source/DSP/ParameterMapping.h` - `KnobValues` and `mapKnobs()`.

The processor reads every parameter atomic into a plain `KnobValues`, then one function turns that
into the settings each DSP stage wants. All the multiplying, scaling and clamping happens in this one
place, which is why it is easy to test.

```cpp
const auto p = enh::dsp::mapKnobs (k);
```

## MULTIPLY (0-3x)

Scales the **knob positions** on that device before the DSP sees them: at 1.5x, CLARITY 20 behaves as
30. Deliberately allowed to run past a knob's printed end - the clamp that follows is the real limit.

Not multiplied: **TONE & SPACE OUTPUT** (a gain; multiplying it would just be a volume surprise). ENH Master
has no gain knob, so nothing is excluded there.

## STRENGTH (0-5)

Scales the **processing**, not the knob positions: EQ moves, generated harmonics, sub lift, resonance
dips, air, body, width, tail level, shimmer. At 0 the device is inert; at 5 it is extreme.

Not scaled: **time** settings - DECAY, TONE, ADAPT. Stretching time with a strength control makes a
reverb sound broken rather than strong.

## Clamps

Every product is clamped to what the stage can actually take, for example:

```cpp
s.halo.width = std::clamp (k.widthPercent / 100.0f * sm, 0.0f, 6.0f);
```

and in the EQ, the summed target is clamped to ±24 dB with `std::clamp (s.strength, 0.0f, 5.0f)`
applied before it. The DSP test suite has a "MULTIPLY and STRENGTH" section that asserts the extremes
stay stable and that OUTPUT is untouched by MULTIPLY.

Related: [[Adaptive EQ]], [[Seraph stages]], [[Parameters]].
