# Adaptive EQ

`Source/DSP/AdaptiveEQ.{h,cpp}`

The curve is **derived from the audio**, every block. There is no stored "smile", and the plugin does
not cut mids and lift lows and highs regardless of source - that behaviour was the original complaint
and the tests now guard against it.

## Where the target comes from

Four contributions are summed, then scaled by the device STRENGTH:

- **balance** - treble and bass measured *against the midrange*, so the midrange is the anchor rather
  than a victim (an earlier slope-based tilt is what produced the permanent scoop);
- **local** - resonances and holes corrected in place, at the frequency where they actually are;
- **bursts** - short-term events that deserve a moment of help;
- **footstep** - `6 * dynamicWeight - 3 * competitorWeight`, weighted by confidence, from
  [[Footstep detection]];
- plus an **add** curve when CLARITY is in ADD mode ([[Harmonics and clarity]]).

A **midrange guard** limits how far the curve may pull the midrange down under any circumstances.
The summed target is clamped to ±24 dB, the PD controller that follows it is limited to 24, and the
final filter gains to ±30 dB.

## Solving it

The 24 band targets overlap, so the filter gains are solved with a regularised least-squares fit
rather than applied one by one - otherwise neighbouring bands add up and overshoot.

Analysis runs decimated 4x (control rate); the filters themselves run per sample.

## What the tests assert

- pink noise in → within ±0.1 dB of flat (nothing is "improved" that was already right);
- an injected resonance or hole is corrected **in place**, not compensated elsewhere;
- **1-4 kHz is never scooped**, whatever the source;
- a dull source is brightened with shelves rather than being declared empty.

Related: [[Harmonics and clarity]], [[Parameter mapping]].
