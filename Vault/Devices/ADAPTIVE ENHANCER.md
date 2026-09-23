# ENH Master unit

The lower, black unit - a Pro-XL-style rack processor. It processes first, and TONE & SPACE finishes what
it produces.

## Sections

**MASTER** - MULTIPLY (0-3x) and STRENGTH (0-5), see [[Parameter mapping]]. Aluminium knobs.

**CLARITY** - CLARITY, ADAPT and the MODE button.
- MODE NORM: CLARITY runs 0-30 and only *normalises* the detail already present.
- MODE ADD + NORM: CLARITY runs 0-10 and also *generates* harmonics where the source is short of
  them ([[Harmonics and clarity]]).
- The printed scale ring on the knob physically swaps when you change mode, and the knob dips while
  it does.
- ADAPT (0-100 %) sets how quickly the analysis follows the audio.

**SUB** - SUB (0-100 %) and the +BOOST button.

**FOOTSTEP** - PRIORITY button; the DETECT ladder beside it lights on accepted events
([[Footstep detection]]).

**METER** - OUT and ENH ladders.

## Display

A live 24-band curve of what the [[Adaptive EQ]] is doing right now, with the current values printed
underneath. The curve is the actual solved filter gain, not a drawing of the knob positions.

## What it does to audio, in order

1. 24-band constant-Q analysis plus a long-term average spectrum, so "what is this source usually
   like" and "what is it doing right now" are separate.
2. The [[Adaptive EQ]] derives a target curve from that: balance, local corrections, bursts,
   footstep weighting.
3. [[Harmonics and clarity]] adds envelope-normalised harmonics when in ADD mode.
4. Sub enhancement, then the output ladder.

Related: [[SERAPH unit]], [[Parameters]].
