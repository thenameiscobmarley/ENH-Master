# Tone & Space stages

`Source/DSP/Seraph.{h,cpp}` - `SilkStage`, `HaloStage` and the `Seraph` wrapper.

## TONE

- **SMOOTH** - opto-style detection finds resonances that are ringing *now* and dips them with TPT/SVF
  filters at those frequencies. The dips are what the left half of the display draws.
- **AIR / WARMTH / BODY** - shelves and bells whose amount follows the source, each with its own
  activity meter (per channel) feeding the display.
- **TAPE** and the triode colour are applied as **scaled differences** (`w += dc * triodeMix`), so
  turning them up adds saturation instead of replacing the signal with it.
- **PROTECT** keeps transients intact: a sample-accurate onset check releases the detector dip
  instantly rather than waiting for a smoothed envelope. Cost measured at -2.9 dB of attack against a
  -3.5 dB test threshold - a zero-latency design cannot do better without look-ahead.
- **AUTO** trims the output so tone changes are not just level changes.

## SPACE

- **Early reflections** - sparse stereo taps between 7 and 37 ms (`Delay early` + 8 taps) in front of
  the tail. This is what makes it sound like a room instead of a wash.
- **Tail** - an 8-line FDN with Hadamard feedback; DECAY sets the time, TONE the colour, MOD a slow
  movement so it never sits still.
- **SHIMMER** - an octave-up delay-line pitch shifter feeding the tail.
- **WIDTH / BASS MONO** - mid/side over an LR4 (two cascaded Butterworth) crossover. A one-pole
  subtraction leaked 1.8 dB of bass into the sides; LR4 does not.
- **DUCK** - the tail gets out of the way while the dry signal is loud.

## Output

Limiter at -0.7 dBFS with 80 ms recovery, then a soft clip:

```cpp
const float needed = peak > ceiling ? ceiling / peak : 1.0f;
limiterGain = needed < limiterGain ? needed : needed + (limiterGain - needed) * limiterRelease;
```

## Metering

Every process keeps a per-channel **activity meter** (change energy, gated on `dryPower > 1e-8` so
silence reads zero), which is what the right half of the display shows.

Related: [[SERAPH unit]], [[Parameter mapping]].
