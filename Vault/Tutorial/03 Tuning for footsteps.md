# 03 Tuning for footsteps

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Footsteps are quiet, short, and buried under gunfire, voices and clutter. A plain EQ boost makes
all of that louder too. ENH Master instead *recognises* footsteps and lifts only them.

## What to do

1. Pick the **COMPETITIVE FOOTSTEPS** preset, or switch **FOOTSTEP** on yourself.
2. Watch the **DETECT** lights on the black unit: they flash when a footstep is recognised.
3. Steps audible but thin? Raise **CLARITY**, not SUB. The information is in the attack.
4. Crates and reloads getting lifted too? Lower **ADAPT** so it has to be more sure.

## How it tells a step from a crate

It looks at the *shape* of each sound over time: how it starts, how fast it dies away, how noisy it is,
and whether it comes in a rhythm like walking. A crate lid rings on; a step is a short thump in a
sequence. In tests it catches 76–100 % of steps and ignores most clutter.

Curious? [Footstep detection](../DSP/Footstep%20detection.md). Watch it decide: `EnhDspTests --events crates`.

Next: [Making it sound heavenly](04%20Making%20it%20sound%20heavenly.md)
