# MIX BALANCER unit

The 4U unit between the SPECTRAL LIMITER and the ADAPTIVE COMPRESSOR. It is mostly a display.

## What it does

`MixBalancer.h` rides six band faders: low shelf < 100 Hz, bells at 200 Hz, 500 Hz, 1.3 kHz and
3.5 kHz, and a high shelf > 7 kHz.

For each band it compares:
- how far the band has moved from its usual level (a few seconds, learnt four times slower while
  the band is being corrected);
- with how far the whole mix has moved (the median of the six).

A band that jumps is cut; a band that drops out is lifted gently, to half the RANGE. A mix that gets
louder as a whole moves nothing, because this is balance, not loudness. Further rules:
- bands in a fresh transient aren't cut, so attacks go through;
- near-empty bands (< 1 % of the mix) are left alone;
- each band's level follower listens over a few of its own cycles.

## Controls

- **BALANCE:** how much of each jump it corrects.
- **SPEED:** how quickly it rides.
- **TILT:** steer darker (−) or brighter (+).
- **RANGE:** the most any band moves, in dB.
- **IN.**
- **RESOLUTION:** from 6 BANDS to SPECTRAL. Spectral mode is 28 third-octave bands (31.5 Hz – 16 kHz),
  ridden against the median of all 28. The two sets of faders are blended in series (the six scaled
  by 1 − R, the 28 by R), which is an exact blend of the two curves with no phasing.

## Display

FabFilter-style layout, printed on the cream card like every display (`balancerDisplay` shader):
- the spectrum in (filled) and out (line);
- the faders drawn as the curve they make, in the band colours, with a handle per band;
- a Pro-C-style strip of the last ten seconds underneath: level in, level out, and the cut in red.

The curve and grid are computed per column on the CPU.

Related: [[LEVEL CONTROL and OUTPUT MONITOR]].
