# Safety for your ears and speakers

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Always on, whatever the knobs say. Everything leaving the rack goes through, in order:

1. **Rumble and DC filter** — nothing below 8 Hz reaches your speakers (costs 0.1 dB at 20 Hz).
2. **Output limiter** — nothing goes over the CEILING (0 dBFS by default), including the peaks
   *between* samples that cheap players and streaming can clip.
3. **Last check**
   - a 20 ms fade-in when audio starts, so there's no pop in your headset;
   - if anything ever goes wrong inside (a broken number), that moment is silenced and the rack restarts clean — never a full-volume blast;
   - a hard stop at full scale, which the limiter should never let the sound reach.

COMPARE (the A/B) goes through all of this too.

## How it's checked

`EnhDspTests --mastering` feeds it broken numbers, DC, a +35 dB jump and every unit at maximum on full
noise: it always stays at or under full scale. `EnhAudioLab check` checks every preset for clipping,
true peak, clicks, DC, surges and silence that doesn't stay silent.

Code: `EnhEngine::process` (`Source/DSP/EnhEngine.cpp`), `FinalLimiter.h`.
