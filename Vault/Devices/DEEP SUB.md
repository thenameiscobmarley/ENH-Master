# DEEP SUB unit

1U, blued-steel plate, between the UPWARD LEVELER and the SPECTRAL LIMITER (4th of the 8 processors).
Source: `Source/DSP/DeepSub.h`. Back to [[00 Start Here]].

## What it is for

Low end you feel as much as hear: deep, dark and huge, like standing inside a steel hull under water.
It is not an EQ boost; it adds things that were not there:

1. **DEPTH - the sub.** The bass line is tracked note by note: zero crossings of the band below
   ~110 Hz, with hysteresis, and a confidence that only builds on a steady pitch. A new tone is
   generated an octave below each note, following the bass's own envelope. Where the octave-down
   would fall under ~30 Hz (nothing reproduces it), the note is reinforced at its own pitch instead,
   blended smoothly by frequency.
2. **HULL - the resonance.** Six two-pole resonators at the inharmonic ratios of a steel shell
   (1, 1.52, 2.03, 2.71, 3.39, 4.43 x a base of 48..22 Hz from SIZE), struck by the bass and the sub.
   They ring for up to ~3 s. Each drifts by a fraction of a percent at 0.05-0.16 Hz (a hull under
   pressure), and a dark low rumble breathes under it while it rings.
3. **PRESSURE - weight.** A low shelf on the programme (up to +6 dB at 55 Hz) and a soft saturation
   of what is generated, which adds the harmonics that make a sub audible on small speakers.

**SIZE** moves the hull's resonances down and makes them ring longer.

## Safety

Everything it generates is mono, high-passed at 18 Hz, and turned down (to 35 %) when the programme is
already near full scale, so it does not push the limiters into pumping. The SPECTRAL LIMITER and the
output limiter come after it. Turning it IN or OUT fades over 30 ms. With DEPTH, HULL and PRESSURE at 0
the audio is not touched, to the bit.

## Settings in the glass panel

SUB SHAPE (sine, warm, growl, crossfaded), TRACKING (30 / 8 / 90 ms glide) and HULL MATERIAL (steel,
iron, cavern). See [[Methods]]. Every knob has its own dropdown with SMOOTHING, CURVE and RANGE.

## Checking it

`EnhDspTests --units` (the DEEP SUB section):
- under an 80 / 98 Hz bass line, DEPTH 8 adds +34 dB at 40 Hz and +27 dB at 49 Hz;
- the hull rings on after the bass stops;
- everything at full on a near-full-scale bass stays bounded and is identical at every block size;
- at 0 it changes nothing.

The factory preset **DEEP SUB: SUBMARINE** shows it off.
