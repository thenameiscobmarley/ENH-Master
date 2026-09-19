# LEVEL CONTROL and OUTPUT MONITOR

## LEVEL CONTROL (1U, bottom of the rack, first in the chain)

**LEVEL**, −24 … +12 dB, sets how loud the whole rack runs. Everything after it hears the new level:
- turned down, the limiters have less to catch, so they stop ducking;
- turned up, they protect harder.

The UPWARD LEVELER reads levels as if LEVEL were at 0 dB, so it never fights the knob
(`SpectralLeveler::Settings::levelDb`). The **INPUT** meter shows the level going into the rack after
the knob, −40 … 0 dBFS RMS.

## OUTPUT MONITOR (3U, top of the rack, the end of the chain)

Shows what the rack does to the track. It is printed on the meters' cream card: the input in pencil,
the output in ink.

- **Waveform, in against out.** The peak per column scrolls right to left. Where the rack takes
  something down, the pencil input shows past the ink output. A faded afterimage of the output
  stays behind (`HardwareRenderer::uploadDisplays`, `waveScreen` shader).
- **SPEED** (1 … 10) sets how much audio the screen holds: about 20 s at 1, down to 1 s at 10
  (`WaveformReader::setSecondsAcross`).
- **Spectrum, in against out.** The analyser's curve, with the tone change (out minus in, ±12 dB) in
  red.
- **Loudness** of the output, from `LoudnessMeter.h` (ITU-R BS.1770-4 / EBU R128):
  - MOMENTARY and SHORT-TERM on the dials;
  - INTEGRATED and TRUE PEAK in the strip under the display;
  - **RESET** starts both again.
  - `EnhDspTests --units` checks the EBU Tech 3341 reference: −23 dBFS stereo 1 kHz reads −23.0 LUFS.

Related: [[MIX BALANCER unit]], [[SPECTRAL LIMITER unit]].
