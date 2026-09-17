# 03 Tuning for footsteps

The point of the plugin. A footstep in a shooter is quiet, short and competing with gunfire, voices
and physics clutter (crate lids, ammo pickups, doors). Boosting "1-4 kHz" boosts all of that too,
which is why fixed-EQ "footstep presets" make games *louder* but not clearer.

## What to do

1. Switch **FOOTSTEP** on. The **DETECT** ladder in the METER section lights when the detector
   accepts an event, so you can see it working while you play.
2. Leave **CLARITY** where it sounds right for the game; footstep priority works on top of it.
3. If steps are audible but thin, raise **CLARITY** rather than SUB - the information is in the
   attack, not the weight.
4. If you get false lifts on crate lids and reloads, that is the detector being generous; lower
   **ADAPT** so the analysis moves more slowly and events have to be more convincing.

## Why it rejects crates

The detector classifies each transient on seven things - onset shape, decay, noisiness, clutter
context, sequence and level - instead of matching a frequency band. A crate lid is impulsive and
bright with a long ringing decay; a footstep is a short thump with a fast, weighted decay and tends
to arrive in a sequence. Full story in [[Footstep detection]].

Measured on the synthetic scenes: 76-100 % of real steps detected, and the crate scene went from 8/8
false lifts (old version) to 3/8, with lift time down from 54-61 % to 3-4 %.

## Watch it decide

```sh
build/EnhDspTests_artefacts/Release/EnhDspTests --events crates
```

prints every accepted and rejected event with the reasons and the ground truth. `--diagnose` does the
same for live audio, `--analyze file.wav` for a recording. See [[Dev hooks]].

Next: [[04 Making it sound heavenly]]
