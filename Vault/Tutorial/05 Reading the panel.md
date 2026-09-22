# 05 Reading the panel

The panels are printed at hardware scale, which means the legends are small on purpose. Nothing is
hidden behind menus - everything is on the front.

## The loupe

Hover anything - a printed word, a knob, a button - and a round glass magnifier appears **centred on
your cursor**, showing the scene re-rendered 1.8x larger behind a fisheye lens about 155 px across. Because it
magnifies about the cursor, the thing under your pointer stays under your pointer: you can read a
label and then keep moving onto the next one straight through the glass. The glass is slightly
transparent so you can still see what is underneath.

Controls also get a small pill under the lens with the control's name and current value, and the
knob's **value arc** lights up. While you are dragging a knob, the loupe locks to that knob so the
value stays readable as the mouse moves.

More: [[The loupe]].

## Clicking a unit

Click a unit's faceplate and a frosted glass panel opens at the right, joined to the unit by a thin
line. It lists the unit's settings one under another; click one to see its choices, and hover any of
them to read how it changes the sound. Clicking another unit switches panels; clicking off the rack
closes it. To walk up to a unit, scroll. See [[Glass panel]] and [[Methods]].

## The displays

**OUTPUT MONITOR** (top): the input against the output as a waveform and a spectrum, the tone change
in red, loudness in LUFS, and a **DUCK** line saying which unit is ducking, where, and by how much.
See [[LEVEL CONTROL and OUTPUT MONITOR]].

**MIX BALANCER**: the band faders as a curve over the spectrum, and the last ten seconds underneath.
See [[MIX BALANCER unit]].

**ADAPTIVE ENHANCER**: a live curve of what the [[Adaptive EQ]] is doing across 24 bands, with the
current CLARITY / ADAPT / SUB values printed underneath.

**TONE & SPACE**: one unified front, not a channel strip pair.
- left half - the resonance dips SMOOTH is applying right now, 150 Hz to 16 kHz;
- right half - a pair of L/R bars per process (SMOOTH, AIR, WARMTH, BODY, TAPE, LEVEL, WIDTH, SPACE,
  SHIMMER), so you can see what the unit is doing *to each channel* rather than guessing.

## The ladders

**OUT** and **ENH** in the METER section show output level and how much enhancement is being applied;
**DETECT** lights on accepted footstep events ([[Footstep detection]]).

## Colour of a moving knob

While a knob moves, its pointer is tinted by whoever moved it: amber for you, blue for host
automation, violet for the plugin tuning itself.

Next: [[06 Windows and other platforms]]
