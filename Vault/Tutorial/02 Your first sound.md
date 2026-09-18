# 02 Your first sound

Two units are racked on top of each other. Audio goes through the lower one first.

| | Unit | Job |
|---|---|---|
| bottom | [[ENH Master unit]] (black) | find what matters in the signal and make it audible: clarity, sub, footsteps |
| top | [[SERAPH unit]] (purple) | make the result pleasant: texture, air, width, space |

## Turn these first

**CLARITY** is the main control. It has two modes, picked with the **MODE** button next to it:

- **NORM** (0-30): normalises detail. It finds where the interesting information is right now and
  balances it, without adding anything. Big numbers are safe here.
- **ADD + NORM** (0-10): the same, plus generated harmonics where the source has none to work with.
  Smaller range because it is doing more. See [[Harmonics and clarity]].

**ADAPT** (0-100 %) is how fast the analysis follows the audio. Low = steady and calm, high = reacts
to every event. 40 % is a good default.

**SUB** lifts the bottom without muddying the middle; **+BOOST** doubles down for small speakers.

**FOOTSTEP** switches on footstep priority: read [[03 Tuning for footsteps]] before using it.

## The two master knobs

Each unit has **MULTIPLY** (0-3x) and **STRENGTH** (0-5) in its MASTER section:

- MULTIPLY scales every knob on that device. At 1.5x a CLARITY of 20 behaves as 30. Output gain is
  never multiplied.
- STRENGTH scales how hard that device's *processing* hits, not the knob positions. 0 means the unit
  does nothing at all; 5 is extreme.

They stack, so `MULTIPLY 2x` with `STRENGTH 2` is a lot. Details in [[Parameter mapping]].

## Signal safety

TONE & SPACE ends in a limiter, so even MULTIPLY 3x with STRENGTH 5 on both units cannot clip the output.
It will sound wrong long before it clips - that is on you, not the plugin.

Next: [[03 Tuning for footsteps]]
