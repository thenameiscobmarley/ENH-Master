# What changed

> 🔎 **[Searchbar](Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Newest first. Versions are **MASSIVE.BIG.MEDIUM.SMALL**: when one number goes up, the ones after it
stay. Downloads: [Releases](https://github.com/thenameiscobmarley/ENH-Master/releases).

## 3.7.12.12 — smarter radar, friendlier app

- **FOOTSTEP RADAR hears a drum machine for what it is.** A drum beat plays on a grid exact to the
  sample; footsteps wobble by milliseconds. Hits on such a grid (shared with other drums or with
  pitched music) aren't lifted: false lifts under music dropped from 68 to 21 a minute.
- **EAR GUARD lets go at once:** after it holds a gunshot down, the quieter sounds right after it
  (footsteps!) are no longer left ducked. A blast still can't sneak back in.
- **The app shows you the way:** the steps are numbered (1 SOURCE, 2 RACK INPUT, 3 LISTEN ON), the
  status line always says what to do next, and a **?** button opens a getting-started strip (it opens
  by itself the first time).

## 3.7.11.12 — precision bands, and an ear guard

- **New: CLARITY's precision bands.** Besides the 24 fixed bands, up to 8 moving bells, each with its own
  frequency, width (Q) and depth: a narrow dip where something rings, a wide one for a build-up, a gentle
  lift into a hole. On the display each band shows as a dot and a bar across its width, moving live.
  In a test, a +10 dB resonance at 2.3 kHz (Q 8) is found at 2299.9 Hz with Q 9.5 and cut 5.6 dB. Glass
  panel **PRECISION**: 8 bands, 4, or off. Notes, hums and test tones are left alone. It replaces an FFT analyser that ran for nothing, so it costs no extra CPU.
- **New: EAR GUARD**, always on. The sound never suddenly gets more than 12 dB louder than it has been:
  a +35 dB blast after a quiet stretch comes out at +13.6 dB. Short sounds are barely touched, and loud
  music that stays loud is never touched, and music coming back after a pause isn't held. 5 ms of
  look-ahead (reported to your DAW). OUTPUT MONITOR's glass panel: 12 dB (default), 15, or 18 dB (cinematic).
- **Fixed:** the enhancer's EQ curve was drawn slightly off in frequency (its bands sit from 40 Hz to
  16 kHz, the printed scale runs from 20 Hz to 20 kHz).
- **Checksums:** every release now has `SHA256SUMS.txt` (every download and every script) and
  `HOW-TO-CHECK.txt`; every zip has `CHECKSUMS.txt` for the files inside it.
- **Website:** [thenameiscobmarley.github.io/ENH-Master](https://thenameiscobmarley.github.io/ENH-Master/).

## 3.7.10.12 — a warmer room, and a faster one

- **New:** a walnut slat wall behind the rack, with a lamp glowing behind it, instead of a black void.
- **Softer window light:** the sun no longer cuts dark bars across the units. Every panel reads evenly,
  with warm light falling across the rack.
- **Faster:** the wall is worked out once when the window opens (about 25 ms), and the full-screen
  "light in the air" pass is gone. On an integrated GPU a frame went from 14.9 to 13.5 ms.
- **Router (Windows):** RACK INPUT now offers **Use VB-Audio Cable (Recommended)** and picks it for
  you; without the cable installed, that entry opens its free download page.
- **Router:** the 3.7.9.12 "follow your device switch" is taken out again: it made Windows' device
  list jump around.

## 3.7.9.12 — the FOOTSTEP RADAR

- **New unit: FOOTSTEP RADAR.** Footsteps have their own unit now: CHARACTER's look in olive drab, with
  SENSITIVITY, BOOST and SPACE, IN and LISTEN switches, and a STEP LIFT meter whose needle kicks on every step.
- **A new way of finding steps**, built from scratch. It recognises each sound by how it starts, dies
  away and rings, follows walkers by sound, place and pace, and judges up to four sounds at once.
  On the test scenes (old detector → radar): near steps 54 → 100 %, far 48 → 96 %, very far 42 → 98 %;
  false lifts on music 158 → 4 a minute, speech 66 → 0, UI clicks 60 → 0.
- **Far steps sound far, near steps sound near.** A far step gets more lift and a room behind it (the
  further, the more room); a near one gets a small, dry lift. Choose the room in the glass panel.
- The ADAPTIVE ENHANCER's FOOTSTEP button moved to the radar; its METER section got a **STEPS** light.
- **Router: switching your output no longer skips the rack.** Pick your headset in Windows' sound
  settings (or the taskbar) while the rack is in, and the rack moves there: it plays to that device and
  takes the default back at once, so everything still goes through it. Apps set to play on that device
  are pulled into the rack too. Taking the rack out leaves you on the device you picked.
- Still hard (and on the list): steps as quiet as the background, and steps well under a drum beat or a voice.

## 3.6.9.12 — COLOUR, and two display fixes

- **New: COLOUR knob on CHARACTER.** You now hear each model's character — its tone and gentle
  harmonics — at any volume, without distortion. Before, the models only really showed once DRIVE made
  them distort. COLOUR 5 is the new default; DRIVE is still there for grit.
- **Fixed:** under the magnifying glass, lit LEDs and lamps showed as big solid squares.
- **Fixed:** the meter lights on the ADAPTIVE ENHANCER flickered, with hard lines through their glow.
  They now rise and fall like real meter LEDs.

## 3.6.8.10 — a real self-test

- **New:** `scripts/selftest.sh` runs every test in one go and says PASS or FAIL per step.
- **New:** `EnhAudioLab check` plays every preset through 18 test scenes and fails if anything clips,
  clicks, surges, drifts, goes out of phase, or sounds different from last time without anyone noticing.
- **Fixed:** clicks on new bass notes with COMPETITIVE FOOTSTEPS (the spectral limiter now eases in its cuts).
- **Fixed:** BASS MONO wiped out the bass of out-of-phase sounds (like rear effects in a surround game).
  That bass is now kept, in the middle.

## 3.6.7.10 — the leveler and compressor listen properly

- **Fixed:** the UPWARD LEVELER jumped +12 dB whenever a steady sound started, then slowly let go. Now it settles gently.
- **Fixed:** wide or out-of-phase sounds were lifted too much, and a wide mix was compressed less than a mono one.
- **Fixed:** LIFT: BIG did nothing on most material.
- Explosions keep much more of their punch (their loudness range went from 1.4 to 4.2 LU on DEFAULT).

## 3.6.6.1 – 3.6.6.10 — cleaner print and no zipper noise

- Knob numbers moved away from their tick marks on every unit.
- Turning any knob fast no longer clicks (TONE, SPACE, HEAVEN, DEEP SUB, the enhancer, the spectral limiter).
- New presets: **MASTERING: ANALOG BUS** and **GAME: ARENA**.

## 3.6.5.1 — every stage audited

- TONE now runs at double speed inside (oversampled), so it stays clean on bright sounds.
  Costs about 6 % of one CPU core and 1 ms of delay.

## 3.6.4.1 — mastering tools and speaker safety

- **COMPARE / A/B:** hear the untouched input at the same loudness, to judge honestly.
- **STEREO:** six units can work on L/R, only the middle, or only the sides.
- **GRIT** switch on CHARACTER: DRIVE with distortion, or DRIVE with colour only.
- **Speaker and headset protection**, always on. See [Safety](Vault/Reference/Safety.md).

## 3.5.4.1 — CHARACTER

- New unit: nine hardware models (consoles, tape, valve, and two made for games and film), two at once with a blend.

## 2.5.4.1 — always-on

- **LOUDNESS TARGET:** one loudness for games, music and calls.
- The router app lives in the tray, can start with your computer, and takes itself out if your headset goes.
- The rack draws much less when a game is in front of it.
- A ready-to-run zip for Windows gamers.

## 1.5.4.1 — the router

- The standalone app becomes a router: it moves the whole system, or chosen apps, into the rack and back,
  and puts everything back even after a crash.

## 1.4.4.1

- Found and fixed why the mix ducked under bass. The standalone can process everything your PC plays.

## 1.3.4.1

- New unit: **DEEP SUB**. Every knob gets its own settings dropdown. EnhAudioLab, a tool that listens to the rack.

## 1.2.4.1

- Every unit gets settings in its glass panel, and every knob gets smoothing, curve and range. **Windows support.**

## 1.1.4.1

- Glass panels and swappable processing methods.

## 1.0.4.1 (was 1.4.1) and earlier

- 1.4: LEVEL CONTROL, MIX BALANCER, OUTPUT MONITOR, the walnut case.
- 1.3: presets in a local file you can edit.
- 1.2: TONE & SPACE AUTO and SUB, no more bass pumping.
- 1.1: SPECTRAL LIMITER, plain unit names, presets.
- 1.0: first release.

The long, technical notes of every version are in the git history of `README.md`.
