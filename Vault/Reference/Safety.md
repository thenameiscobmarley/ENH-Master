# Safety for your ears and speakers

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Always on, whatever the knobs say. Everything leaving the rack goes through, in order:

1. **Rumble and DC filter** — nothing below 8 Hz reaches your speakers (costs 0.1 dB at 20 Hz).
2. **EAR GUARD** — the sound never suddenly gets more than **12 dB (about four times) louder** than it
   has been. It measures loudness the way LUFS meters do (over 400 ms) and compares it with how loud
   things have usually been over the last several seconds. A blast after a quiet stretch is held at
   +12 dB. It looks 5 ms ahead, so it's already holding before you hear the blast. Short sounds carry little
   energy, so a single shot up to about 20 dB over the usual level passes untouched (a much louder one
   is held down too). It spends its allowance evenly, so there is no spike as a blast starts (the loudest
   10 ms stay under +20 dB) and a held blast stays level instead of stuttering; material that's loud all the time, and
   ordinary ups and downs, are never touched, so mastering isn't affected. It lets the new level in
   slowly (under 1 dB a second), so a long blast can't talk its way through. A pause in the music doesn't
   lower what it counts as usual, so the music coming back isn't held.
   **OUTPUT MONITOR's glass panel → EAR GUARD:** 12 dB (strict, the default), 15 dB, or 18 dB (cinematic:
   explosions keep more punch). It can't be switched off.
3. **Output limiter** — nothing goes over the CEILING (0 dBFS by default), including the peaks
   *between* samples that cheap players and streaming can clip.
4. **Last check**
   - a 20 ms fade-in when audio starts, so there's no pop in your headset;
   - if anything ever goes wrong inside (a broken number), that moment is silenced and the rack restarts clean — never a full-volume blast;
   - a hard stop at full scale, which the limiter should never let the sound reach.

COMPARE (the A/B) goes through all of this too.

## How it's checked

`EnhDspTests --mastering` feeds it broken numbers, DC, a +35 dB jump and every unit at maximum on full
noise: it always stays at or under full scale. The EAR GUARD is checked on a +35 dB jump after a quiet
stretch (it comes out at +13.6 dB at most, over 400 ms), and on loud and swelling material it must not touch. `EnhAudioLab check` checks every preset for clipping,
true peak, clicks, DC, surges and silence that doesn't stay silent.

## What it can't do

It can't know how loud your headset is: that's your volume knob. Start low and turn up. The rack keeps
jumps in check and never goes over full scale; how loud full scale is in your ears is up to that knob.

Code: `EnhEngine::process` (`Source/DSP/EnhEngine.cpp`), `EarGuard.h`, `FinalLimiter.h`.
