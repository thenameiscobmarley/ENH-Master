# 02 Your first sound

Eight units are racked in a curved walnut case. Audio goes up through them, bottom to top.

| | Unit | Job |
|---|---|---|
| 1 (bottom) | [[LEVEL CONTROL and OUTPUT MONITOR\|LEVEL CONTROL]] | how loud the whole rack runs |
| 2 | [[ENH Master unit]] (black) | find what matters in the signal and make it audible: clarity, sub, footsteps |
| 3 | UPWARD LEVELER | lift quiet detail, per band |
| 4 | [[SPECTRAL LIMITER unit]] | cut a region that jumps out, where it is, so the mix doesn't duck |
| 5 | [[MIX BALANCER unit]] | ride band faders to keep the balance |
| 6 | ADAPTIVE COMPRESSOR | even out the level, threshold follows the programme |
| 7 | [[SERAPH unit]] (purple) | make the result pleasant: texture, air, width, space |
| 8 (top) | [[LEVEL CONTROL and OUTPUT MONITOR\|OUTPUT MONITOR]] | see what the rack is doing: in against out, loudness, who is ducking |

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

The rack ends in an output limiter at 0 dBFS, so even MULTIPLY 3x with STRENGTH 5 on both units cannot clip the output.
It will sound wrong long before it clips - that is on you, not the plugin.

Next: [[03 Tuning for footsteps]]
