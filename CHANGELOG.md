# What changed

> 🔎 **[Searchbar](Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Newest first. Versions are **MASSIVE.BIG.MEDIUM.SMALL**: when one number goes up, the ones after it
stay. Downloads: [Releases](https://github.com/thenameiscobmarley/ENH-Master/releases).

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
