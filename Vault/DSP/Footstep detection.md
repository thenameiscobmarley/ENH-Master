# Footstep detection

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

How the [FOOTSTEP RADAR](../Devices/FOOTSTEP%20RADAR.md) finds a step. It doesn't boost a frequency
band: it **recognises each sound** by its shape over time, and then lifts only the ones that are steps.

## 1. Listening

The sound is split into 6 bands: the thump (90 Hz), the body (250 Hz), the low mids (630 Hz), the
click (1.6 kHz), the scuff (3.8 kHz) and the air (8 kHz). For each band it keeps:

- a **background**: what the band does when nothing is happening (the quiet 20 % of the last 2 seconds),
  and how much it flickers on its own;
- a **fast** level (follows a step's attack in half a millisecond) and a **short** one (shows how long
  the impact really lasts).

## 2. Something happens

A sudden rise over the background in several bands starts an **event**. A steady sound can't start
one, however loud. Up to four events are judged at once, so a step that lands while another sound is
still being judged (the other walker's step, a word) gets a judgement of its own.

## 3. Is it a step? (decided 50 ms after it starts)

Band by band, it checks the things that make an impact:

| Clue | A step | Not a step |
|---|---|---|
| how fast it arrives | 1–5 ms | a syllable swells over 15+ ms |
| how fast it dies away | clearly, within 45 ms | music and wind stay |
| how long it lasts | 8–40 ms | a UI click: 1–3 ms |
| is it a tone? | noise (a scuff, a thump) | a reload's ring, a chime, a voice, a bass note |
| weight behind it | a thump in the low bands | a click alone |
| how loud, how wide | quiet or medium | a gunshot or blast: loud in every band |
| on top of something | still rises clearly out of it | a rumble's own flicker |

Over a voice or music, the rise is measured against what was already there, and a step inside a
syllable is found by the steep climb it makes out of it.

## 4. Walkers

Found steps are grouped into **walkers** by sound, place and pace. A walker who is due to step makes
the radar more ready to hear it, and vouches for faint steps that fit. Three faint, alike sounds in a
steady walking rhythm become a walker too: that's how a step as quiet as the background is found.

## 5. The lift

- 2.5 ms of look-ahead, so the lift starts with the step's attack.
- Only the step's own bands are lifted, by how much each one carries of it.
- **Near** steps: a small dry lift. **Far** steps: more lift, and a room behind them (the further, the
  more room and the later it arrives), so your ears can tell near from far.
- 130 ms in, it checks once more: a sound that's still going (not a step) is let go at once.

## Results (`EnhDspTests --radar table`)

| Scene | Found | False alarms / min |
|---|---|---|
| near, mid distance (10 surfaces) | 100 % | 0 |
| far (-50 dBFS) | 96 % | 0.7 |
| very far (-60 dBFS) | 98 % | 0 |
| running, sneaking, from behind | 95–100 % | 0 |
| two walkers at once | 85 % | 0 |
| music, speech, gunfire, explosions, rain, reloads, clicks alone | — | 0–4 |

Still hard: steps as loud as the background (about 20 %), steps 16 dB under a drum beat (50 %, and the
beat's hits get lifted too), and steps under a voice 10 dB louder than them (about 50 %).

Code: `FootstepRadar.cpp` (`controlTick`, `decide`, `confirm`, `rhythmOf`). Tests: `Tests/RadarTests.h`.
