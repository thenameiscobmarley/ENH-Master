# 04 Making it sound heavenly

[[SERAPH unit]] is the purple unit on top. **POWER** is a three-way selector:

- **OFF** - true bypass
- **TONE** - tone and texture only (no space)
- **LOUDNESS** - TONE plus SPACE, the spatial half

Zero latency in every mode, so it is safe for playing games and for live monitoring.

## TONE: texture

| Knob | What it does |
|---|---|
| SMOOTH | finds resonances that are ringing right now and dips them, gently, where they are |
| AIR | high shelf that follows what the source can take |
| WARMTH | low-mid weight |
| BODY | the part that makes thin sources sound whole |
| OUTPUT | make-up gain, never scaled by MULTIPLY |
| PROTECT | keeps transients intact while the rest is processed |
| TAPE | tape-style saturation, applied as a scaled difference so it adds rather than swamps |
| AUTO | auto gain so changes do not just get louder |

## SPACE: space

| Knob | What it does |
|---|---|
| WIDTH | mid/side width, LR4 crossover so the bottom stays mono |
| SPACE | how much of the 8-line reverb you hear |
| DECAY | tail length, 0.3-8 s (not scaled by STRENGTH: time stays time) |
| SHIMMER | octave-up content fed into the tail |
| TONE | dark to bright tail |
| DUCK | tail gets out of the way while the dry signal is loud |
| BASS MONO | keeps everything below the crossover centred |
| MOD | slow modulation so the tail never sits still |

## How to use it for games

Start at LOUDNESS with the defaults, then set **SPACE** by ear with the game running: enough that the
world feels open, little enough that you can still place a footstep. If the tail smears the direction
of sounds, raise **DUCK** before lowering SPACE. Keep **WIDTH** at or below 140 % - beyond that, side
content starts to fight positional audio.

For music or mixing, TONE alone on a bus is the safer setting; LOUDNESS on a send.

How it works inside: [[Seraph stages]]. Next: [[05 Reading the panel]]
