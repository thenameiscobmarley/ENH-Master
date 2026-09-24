# CHARACTER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The sound of real studio hardware. Two models at once, blended.

## Knobs

- **A** and **B** — pick two of nine models.
- **BLEND** — from all A to all B.
- **COLOUR** — how much of the models' character you hear: their tone (tape's bass bump, a console's
  bite, a valve's warmth) and their gentle harmonics, **at any volume, without distortion**. 0 = none,
  5 = the default, 10 = strong.
- **DRIVE** — how hard they're pushed. Turn it up for grit and saturation. The volume stays the same.
- **GRIT** — *on*: DRIVE grows into real distortion. *Off*: colour only, never distortion.
- **IN** — off by default. **HARMONICS** meter — how much it's adding.

**Tip:** for colour, turn **COLOUR** up. For crunch, turn **DRIVE** up with GRIT on.

## The nine models

| Model | Sounds like |
|---|---|
| CLEAN | modern and see-through |
| BRIT | a crisp, tight British console |
| AMER | a punchy American console |
| VINTAGE | old class-A gear with transformers: weight and a silky top |
| TAPE 15 | tape at 15 ips: warm, with a bass bump |
| TAPE 30 | tape at 30 ips: cleaner, more top |
| VALVE | a valve that breathes with the music |
| ARENA | for games: tight lows, clear steps and callouts |
| CINEMA | for films: weight underneath, a smooth top |

Each model copies what the real thing does (transformers, amplifier stages, tape), not just one curve.
Nine models, any pair, any blend, any drive: lots of characters to find.

## Good to know

- COLOUR lifts the sound into each model's sweet spot on the way in and takes the lift off again on the
  way out, so quiet music gets the same character as loud music. It never pushes past the sweet spot.
- It keeps its peaks in check, so it never pushes the output limiter into distortion.
- Delay: 6 samples. CPU: about 6–10 % of one core.
- Glass panel: **COMPONENTS** (make left and right slightly different, like real gear) and **STEREO**.

Code: `Character.h`. Test: `EnhDspTests --character`.
