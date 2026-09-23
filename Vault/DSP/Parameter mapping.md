# How MULTIPLY and STRENGTH work

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Every knob value goes through one function, `mapKnobs()` in `Source/DSP/ParameterMapping.h`, which
turns knob positions into settings for each stage. All the scaling and limits live there, so it's
easy to test.

## MULTIPLY (0–3x)

Scales the **knob positions** on that unit: at 1.5x, CLARITY 20 acts like 30. It may go past a knob's
printed end; a limit after it keeps things safe. It never changes an output volume knob.

## STRENGTH (0–5)

Scales **how hard the unit works** (EQ moves, harmonics, width, room level…), not the knobs.
0 = the unit does nothing. Times (DECAY, ADAPT) are never scaled — a stretched reverb sounds broken,
not stronger.

## Limits

Every result is clamped to what the stage can take. The tests turn everything to its extremes and
check it stays stable, and that OUTPUT isn't touched by MULTIPLY.

Related: [Parameters](../Reference/Parameters.md).
