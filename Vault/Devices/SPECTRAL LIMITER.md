# SPECTRAL LIMITER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

## The problem it solves

A normal compressor hears a huge bass hit and turns the *whole* mix down — footsteps and voices
too. That's the "everything goes quiet when the bass hits" feeling. This unit takes the hit down
**only in the part of the sound where it is**, so the rest stays put.

## How it decides

1. It learns what's *normal* for each part of the sound over the last 10 seconds (a kick drum
   teaches it that kicks are normal).
2. Only what jumps past normal gets cut, with up to three moving cuts, right where it happens.
3. If something is about to go over the CEILING, that spot is cut deeper.
4. Only if *everything* is too loud does it turn the whole sound down.

It also gives back a little loudness while it cuts, so the mix doesn't sound quieter, and it tells the
ADAPTIVE COMPRESSOR to ignore what it's already handling.

Cuts ease in over at least one cycle of the note they cut, and its filters glide smoothly, so it never clicks.

## Knobs

| Knob | What it does |
|---|---|
| RANGE | the deepest cut (0–18 dB) |
| RELEASE | how fast a cut lets go |
| CEILING | the level it protects |
| IN | on or off |

Meters: **CUT** and **BROADBAND**. The black unit's screen shows where it's cutting (magenta).

Code: `SpectralLimiter.h/.cpp`. Test: `EnhDspTests --limiter`.
