# MORPHING VISUALIZER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called HYPERCUBE.

A visualiser (3U, model HC-4D). It starts in the [Gear locker](../UI/Gear%20locker.md), under Meters and tools.
It passes the sound through untouched, even when on.

At rest it is a clean neon purple wireframe cube: its 12 edges and one diagonal across each face, drawn like a
vector monitor's beam (soft phosphor glow, a slight flicker, the beam never quite on the same pixel twice).

With music it morphs. Each new shape is a blend of two or three of 30 forms, each with its own random shape: a
water droplet, a puddle with a random rim, a torus, a wave sheet, melting, a blob, a spiral... or, when the music
is hard, stars, shards, crystals, spikes, an exploding cube. A material goes on top: liquid wobble, ripples,
shattering on hits, rubber that springs. Calm music gets slow, liquid shapes; hard music gets sharp ones, and a
jump to a new shape on the big hits. Shapes flow slowly into each other in between. When the music goes quiet it
comes home to the purple cube.

It is one colour at a time, fading between a hundred.

- **MORPH**: 0 to 10. How far it wanders from the cube. AUTO: the music's energy decides.
- **REACT**: 0 to 10. How hard the music pushes it (each band bends its own height of the shape, hits pulse it).
  AUTO: harder music, more.
- **PALETTE**: 0 to 10. Its colour, from the neon purple through a hundred. AUTO: it wanders through them, faster
  when the music is hard.
- **BEAM**: 0 to 10. The beam's focus and how long its glow stays. AUTO: calm music, a long soft glow; hard music,
  tight and quick.

Code: `DSP/units/Sims4.h` (what it hears), the screen: `UI/Scene/ColourScreens.h`.
