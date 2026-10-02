# SPECTRAL DETAIL ENHANCER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Brings out what the mix hides (3U, model SD-24). It starts in the [Gear locker](../UI/Gear%20locker.md),
under EQ and tone → Clarity and presence, and under Mastering simulations.

It works like the ADAPTIVE ENHANCER EQ: it listens, then lifts. What it listens with is a simulation of
hearing. It takes a fine spectrum of the sound 90 times a second and finds every partial in it. For each
one it works out how much the louder partials around it mask it: a loud sound hides quieter ones near it,
far more above it in pitch than below. Whatever is real but hidden is lifted, by how deeply it is hidden,
so it comes out from under what masks it. That can be a reverb's tail, a breath, a ghost note, or a second
guitar. Hidden attacks get a little more. Hiss and silence are never lifted. The level is matched to what
came in: it brings detail out without making anything louder.

Its screen shows the 24 bands as the ear hears them. Each band's level is a bar, the masking threshold is a
line over them, and what it lifts out from under the line glows, with motes rising from it.

| Control | What it does |
|---|---|
| DETAIL | How much it lifts. |
| DEPTH | How far under the masking it reaches (0 to 24 dB). |
| CLARITY | Eases the low-mids when they bury the presence (1 to 5 kHz). |
| AIR | Adds harmonics over the top, made from the presence it uncovered. |
| SPEED | How quickly it follows the music. |
| LISTEN | Plays only what it brings out. |
| MIX | Dry against processed. |
| POWER | Off: the sound passes untouched. |

With DETAIL, CLARITY and AIR at 0 it leaves the sound as it was. Both sides get the same lift, so the
stereo image stays where it is.

**MULTIPLY and STRENGTH.** MULTIPLY scales its amount knobs together, from 0.25x to 3x (its mix, gain,
thresholds, frequencies and times stay as set; a knob either side of 0 is scaled from its middle).
STRENGTH sets how much of its change to the sound you hear: 0 % is the sound as it came in, 100 % as set,
200 % twice the change.
