# 03 Tuning for footsteps

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Footsteps are quiet, short, and buried under gunfire, voices and clutter. A plain EQ boost makes
all of that louder too. ENH Master has a unit just for them: the
**[FOOTSTEP RADAR](../Devices/FOOTSTEP%20RADAR.md)**. It *recognises* footsteps and lifts only them.

## What to do

1. Pick the **COMPETITIVE FOOTSTEPS** preset, or flip the radar's **IN** switch on yourself.
2. Walk around in a game and watch the radar's **STEP LIFT** meter: the needle kicks on every step.
3. Far steps too quiet? Raise **BOOST**. Can't tell far from near? Raise **SPACE**.
4. Missing faint steps? Raise **SENSITIVITY**. Lifting things that aren't steps? Lower it, or set
   **DETECTION** to *STR* in the glass panel.
5. Press **LISTEN** to hear only what the radar adds.
6. Steps audible but thin? Raise **CLARITY** on the ADAPTIVE ENHANCER. The information is in the attack.

## How it tells a step from a crate

It looks at the *shape* of each sound over time: how it starts, how fast it dies away, whether it
rings like metal, and whether it comes in a walking rhythm. In tests it finds every near and mid-distance
step on 10 surfaces, and 96–98 % of far ones.

Curious? [Footstep detection](../DSP/Footstep%20detection.md). Watch it decide: `EnhDspTests --radar table`.

Next: [Making it sound heavenly](04%20Making%20it%20sound%20heavenly.md)
