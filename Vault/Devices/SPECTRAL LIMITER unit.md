# SPECTRAL LIMITER unit

1U, steel-blue plate, between the UPWARD LEVELER and the ADAPTIVE COMPRESSOR (stage 4 of the 7 processors).
Source: `Source/DSP/SpectralLimiter.h/.cpp`. Back to [[00 Start Here]].

## What problem it solves

The ADAPTIVE COMPRESSOR listens to the whole signal. When a huge bass hit arrives, it turns the
*whole mix* down, footsteps and detail included: the "everything gets quieter when the bass hits"
breathing. This unit deals with the hit in the region where it happens, so the compressor has less
reason to react.

## How it decides, lightest action first

1. **Is anything abnormal, and where?** It reuses the enhancer's 24-band analyser (see
   [[Adaptive EQ]]). For each band it keeps a rolling baseline and learns how far above that baseline
   the band *normally* goes: the 97th percentile over the last 10 s, not counting the newest 1.5 s.
   Kick drums teach it that their hits are normal. Only energy beyond that normal counts, and a band
   whose excess is just leakage from a loud band an octave or more away is ignored.
2. **Localised:** up to three moving cuts follow the offending regions. Each is a bell, or a shelf
   when the region runs off the bottom or top of the spectrum.
3. **Headroom threatened** (peak here above CEILING): the same region is cut deeper.
4. **Broadband and threatening headroom:** only then is the whole signal turned down.

CEILING defaults to 0 dBFS in every preset, so step 3 acts only on real overs. Step 2 is about tone
(a region out of balance), never about how loud the programme is.

## Loudness keeper

Taking the excess away loses nothing: it was never part of the mix. But where a cut takes a band
below what it usually carries (a wide cut spilling onto its neighbours, a cut still letting go after
the event), the mix is quieter than usual and everything else *sounds* quieter although its level
never moved. The keeper gives 60 % of that loss back to the whole signal:
- each decision carries every band's power now and its baseline, ear-weighted (`powerK`, `usualK`);
  every 8th control step (~190 Hz) the cuts' response at each band says how far below usual they
  take it;
- the lift follows the cuts' own attack and release, at most 3 dB, and never past the headroom left
  under CEILING (less 0.5 dB); clipping cuts (`extraDb`) are not made up;
- it is applied after the compressor's key is taken, so the compressor never hears it.

`getMakeupDb()`, shown as `LOUDNESS KEPT` on the OUTPUT MONITOR.

It also feeds the compressor a *key*: the audio with the flagged region pulled down to its baseline.
While it handles a localised event, the compressor ignores that event. With nothing flagged the key is
the plain audio, so the compressor behaves exactly as before.

## Controls

| Control | Range | What it does |
|---|---|---|
| RANGE | 0-18 dB | deepest spectral cut |
| RELEASE | 30-600 ms | how quickly a cut lets go (attack follows: 1.5-8 ms) |
| CEILING | -12..0 dBFS | when a spectral event also threatens headroom here |
| IN | | hardware bypass |

Meters: CUT (deepest spectral cut, 0-18 dB) and BROADBAND (0-12 dB); in normal use BROADBAND
barely moves. On the enhancer's analyser a magenta curtain hangs from the top of the plot where it
is cutting (amber = broadband), with a `LIMIT -9 dB @ 68 Hz` readout in the header. The OUTPUT
MONITOR's DUCK readout names it when it is the deepest duck in the rack.

## Checking it

`build/EnhDspTests_artefacts/Release/EnhDspTests --limiter` runs a mix (bed, kicks, 2 kHz detail)
with a huge bass hit, a broadband burst and a treble whistle, with the unit IN and OUT, and prints
the detail dip, compressor gain reduction and where it cut.
