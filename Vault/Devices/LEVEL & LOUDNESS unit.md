# LEVEL & LOUDNESS unit

The 2U unit at the bottom of the rack, first in the signal chain.

## LEVEL

−24 … +12 dB. It sets how loud the whole rack runs. Everything after it hears the new level:
- turned down, the limiters have less to catch, so they stop ducking;
- turned up, they protect harder.

The UPWARD LEVELER reads levels as if LEVEL were at 0 dB, so it never fights the knob
(`SpectralLeveler::Settings::levelDb`).

## Meters (the rack's output)

`LoudnessMeter.h` measures to ITU-R BS.1770-4 / EBU R128, with K-weighting for any sample rate:
- **MOMENTARY** (400 ms) and **SHORT-TERM** (3 s) on the two dials, −40 … 0 LUFS;
- **INTEGRATED** (gated at −70 LUFS absolute and −10 LU relative), since RESET;
- **TRUE PEAK** (4x oversampled), held until RESET.

The integrated and true-peak readings print in the strip under the waveform. `EnhDspTests --units`
checks the EBU Tech 3341 reference: −23 dBFS stereo 1 kHz reads −23.0 LUFS.

## Output waveform

A cream card with black ink, lit like the meter faces. The output's peak per 10 ms scrolls right to
left in linear amplitude. A faded afterimage stays where loud material went past and fades over a
second or two (`HardwareRenderer::uploadDisplays`, `waveScreen` shader).

Related: [[MIX BALANCER unit]], [[SPECTRAL LIMITER unit]].
