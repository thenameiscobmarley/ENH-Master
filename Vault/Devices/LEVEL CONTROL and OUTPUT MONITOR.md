# LEVEL CONTROL and OUTPUT MONITOR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

## LEVEL CONTROL (bottom, first)

- **LEVEL** (−24 … +12 dB) — how loud the whole rack runs. Lower = the limiters have less to do.
  Higher = they protect harder. The UPWARD LEVELER never fights this knob.
- **INPUT** meter — the level coming in.

## OUTPUT MONITOR (top, last)

Shows what the rack did to your sound: the input in pencil, the output in ink.

- **Waveform** — before and after, scrolling. Where the rack turned something down, the pencil shows
  past the ink. **SPEED** sets how much time fits on screen.
- **Spectrum** — before and after, with the tone change drawn in red.
- **Loudness** — MOMENTARY and SHORT-TERM on the dials, INTEGRATED and TRUE PEAK underneath. **RESET** starts them again.
- **DUCK** — names whoever is turning things down right now, where, and by how much,
  e.g. `DUCK  SPECTRAL LIMITER  AT 2.5 kHz  -4.2 dB`.
- **COMPARE** — hear the untouched input at the same loudness. See [Mastering tools](../Reference/Mastering%20tools.md).

## In its glass panel

- **CEILING** — 0, −0.3 or −1 dBFS.
- **LOUDNESS TARGET** — OFF, −23, −18 or −14 LUFS: brings everything to one loudness. It moves
  slowly, ignores short bangs and pauses, and never lifts near-silence.

Code: `LoudnessMeter.h`, `LoudnessTarget.h`, `FinalLimiter.h`.
