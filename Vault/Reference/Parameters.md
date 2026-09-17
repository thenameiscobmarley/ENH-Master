# Parameters

From `Source/Parameters/ParameterSpecs.cpp`. Every one is host-automatable.

## ENH Master

| id | Panel | Range | Default |
|---|---|---|---|
| `clarityNorm` | CLARITY (NORM mode) | 0 - 30 | 15 |
| `clarityAdd` | CLARITY (ADD + NORM mode) | 0 - 10 | 3 |
| `clarityMode` | MODE | Norm / Add + Norm | Norm |
| `adaptSpeed` | ADAPT | 0 - 100 % | 40 |
| `sub` | SUB | 0 - 100 % | 0 |
| `subBoost` | +BOOST | off / on | off |
| `footstep` | FOOTSTEP | off / on | off |
| `enhMultiply` | MULTIPLY | 0 - 3 x | 1 |
| `enhStrength` | STRENGTH | 0 - 5 | 1 |

## SERAPH

| id | Panel | Range | Default |
|---|---|---|---|
| `seraphMode` | POWER | Off / Silk / Heaven | Heaven |
| `seraphMultiply` | MULTIPLY | 0 - 3 x | 1 |
| `seraphStrength` | STRENGTH | 0 - 5 | 1 |
| `silkSmooth` | SMOOTH | 0 - 10 | 4 |
| `silkAir` | AIR | 0 - 10 | 4 |
| `silkWarmth` | WARMTH | 0 - 10 | 3 |
| `silkBody` | BODY | 0 - 10 | 2 |
| `silkOutput` | OUTPUT | -12 - +12 dB | 0 |
| `silkProtect` | PROTECT | off / on | on |
| `silkTape` | TAPE | off / on | off |
| `silkAuto` | AUTO | off / on | on |
| `haloWidth` | WIDTH | 0 - 200 % | 120 |
| `haloSpace` | SPACE | 0 - 10 | 2.5 |
| `haloDecay` | DECAY | 0.3 - 8 s (skewed, centre 2 s) | 2.2 |
| `haloShimmer` | SHIMMER | 0 - 10 | 1.5 |
| `haloTone` | TONE | 0 - 10 | 6 |
| `haloDuck` | DUCK | off / on | on |
| `haloBassMono` | BASS MONO | off / on | on |
| `haloMod` | MOD | off / on | on |

A `Spec` carries the id, names, unit, kind (`continuous` / `toggle` / `choice`), range, default,
decimal places, an optional `skewCentre` and the texts for choices. The knob scale rings printed on
the panel are chosen from the range: 3, 5, 10 or 30.

How these are combined: [[Parameter mapping]].
