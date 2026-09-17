# Footstep detection

`Source/DSP/FootstepDetector.{h,cpp}`

A **classifier of events**, not a band booster. Nothing here says "1-4 kHz"; a footstep is recognised
by its shape in time, and the frequency weighting is derived from the event that was actually found.

## The timeline of one event

| Time | What happens |
|---|---|
| onset | grouped 3-band spectral flux crosses a threshold with hysteresis (`armed`), so ambient noise cannot re-trigger it |
| +4 ms | a **provisional** decision, so the lift starts with the attack rather than after it |
| +42 ms | the real decision, once the decay shape is known |
| to +160 ms | a retraction window - a wrong provisional accept is taken back before it can do damage |

## The seven cues

onset shape · decay (`eventDropDb()`, weighted per band) · noisiness · clutter context · sequence ·
level · spectral similarity to the previous event.

Two rules do most of the crate rejection:

- **clutter** counts only impulsive neighbours (`clutterCount += decay`), so a busy but soft scene is
  not treated as clutter;
- **"too close" rejection** requires spectral similarity > 0.45 *and* the previous event to have been
  impulsive - without that second condition, speech syllables cancelled real footsteps.

A `suspicion` hold keeps the detector sceptical for a while after a rejected burst.

## Results (synthetic scenes, `--events`)

- steps detected: **76-100 %** depending on the scene;
- crate scene: **8/8 → 3/8** false lifts versus the old version, lift time **54-61 % → 3-4 %**.

## What it feeds

The [[Adaptive EQ]] gets `footConfidence`, a `dynamicWeight` per band (where this event's information
is) and a `competitorWeight` (where the things masking it are). The EQ lifts the first and dips the
second, in proportion to confidence - so the "footstep EQ" is different for every footstep.

Try it: `EnhDspTests --events crates` ([[Dev hooks]]).
