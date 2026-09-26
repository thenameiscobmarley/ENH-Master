# What changed

> 🔎 **[Searchbar](Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Newest first. Versions are **MASSIVE.BIG.MEDIUM.SMALL**: when one number goes up, the ones after it
stay. Downloads: [Releases](https://github.com/thenameiscobmarley/ENH-Master/releases).

## 3.7.13.13 — a calmer rack, and a clearer delay

- **The analog engine behaves like circuits, not curves.** In CHARACTER every model now has its own
  behaviour, not a different amount of the same distortion:
  - its own harmonic fingerprint (2nd to 5th);
  - transients that hit the core harder than the sustain (tape softens them, valve squeezes, the
    console stays controlled);
  - iron with hysteresis and memory (it saturates sooner for a while after a hot passage);
  - a coupling capacitor and heat that move the operating point after loud passages and recover
    over seconds;
  - lows fed hotter into the iron and cut back after (thick, compressed lows);
  - a top end that closes a little only when driven hard.
  Quiet music stays nearly clean (under 0.25 % at -40 dBFS at COLOUR 0). Left and right differ by a
  hair even when "matched".
  COLOUR turned end to end at once no longer clicks (the models' voicing follows it in short steps),
  and the iron's memory builds up over a few milliseconds instead of at once (it clicked at DRIVE 9
  with GRIT).
- **ADAPTIVE COMPRESSOR:** the gain cell is no longer a perfect multiplier: it bends gently the harder
  it works, lags a hair on steep edges, and remembers heavy work for a second or two.
- **ADAPTIVE ENHANCER:** its exciters add mostly the harmonic that is missing (2nd or 3rd, judged
  separately), and their share follows the band's level.
- **New: the rack's output amplifier**, a last, very subtle analog stage (a soft curve and a top that
  closes a little only on hot passages), so all the units sound like one signal path. It only runs
  while an analog unit is in: TRANSPARENT stays bit-for-bit transparent.
- **New: the LUNCHBOX**, a 500-series side rack on a walnut stand, in dark charcoal hardware with light
  engraved print, modelled on the classic British console gear of the 1970s:
  - **CLASS-A EQ:** the inductor low shelf with its overshoot and dip, a mid bell that sharpens as you
    boost (with HI Q), a gentle 12 kHz shelf with no cramping near 20 kHz, an 18 dB/oct high-pass, and
    **IRON**: transformer saturation that grows at low frequencies, and a class-A stage.
  - **DE-HARSH:** a split-band de-esser for piercing 2.5 - 6.5 kHz peaks (gunshots, glass, sibilance),
    with a soft knee and a release that adapts; untouched until it cuts.
  - **CROSSFEED:** the classic Bauer crossfeed for headphones; centred sound passes exactly.
  - An OUTPUT meter and one empty slot. All out by default, zero latency. The camera glides across to
    it and back. See [LUNCHBOX](Vault/Devices/LUNCHBOX.md).
- **New: a POWER strip** at the bottom of the rack, with eight US outlets on its front: the rack's plugs
  in six of them, two empty (slots and ground hole showing), a lit mains switch. The cords drape out of
  the plugs to the floor and round the case; the strip itself is plugged into a wall socket.
- **Cables:** thick black XLRs run behind the case and across the floor in a loose bundle to the
  lunchbox, where each one plugs in with its connector.
- **Meters:** the thick bezels are gone: the glass sits flush in the panel.
- **Every unit restyled after real studio hardware**, from reference photos: its panel finish (brushed
  aluminium, black anodised, cream and off-white paint, navy, hammertone blue), its print, its meter
  glow and its knobs - thirteen new knob models matched to photographs (fluted top hats, knurled
  aluminium, fluted skirts, small ribbed, smooth matte black, red anodised trim, the vintage channel's
  maroon and greys, coloured 500-series caps), each with its own gloss, collar and painted line.
- **FOOTSTEP RADAR's BOOST goes to 34 dB** (it was 12): far, quiet steps can be made unmistakable. Near,
  loud steps still get only a fraction of it, and EAR GUARD still holds any jump. The STEP LIFT meter
  reads the real lift now, 0 - 36 dB.
- **Lighting:** shadows and ambient occlusion are ray traced (once, when the rack is built) against every
  knob, switch and button and the case's walnut cheeks, and the rack is lit by the room it reflects.
  Reflections are sharp like still water, with much less haze. About 1 ms a frame on a UHD 600.
- **Website: the rack unit designer** - bend text along a curve or round any part (a knob, a meter);
  printed scales numbered at every step, numbers that turn with the dial, and your own number of steps
  and sweep; Ctrl+C / X / V, box-select, groups, and changes made to several parts at once.
- The wet look can be turned down or off in `ui-config.json` (`wetCoat`, 0 - 1) on a weak GPU.
- **FOOTSTEP RADAR tuned on a real match.** Close footsteps in a loud mix are as loud and as broad as a
  gunshot, and the radar took them for shots: on a real recording it found 3 of 11 close running steps.
  Now 7, and still nothing on the gunfire. A shot is top-heavy (10 - 15 dB stronger in the high bands), a
  footstep lands with weight (3 - 9 dB stronger low down): the radar tells them apart by that. A walker's
  beat stays on the first hit of each step (a game's heel and toe), and a step that lands on the walker's
  beat is no longer mistaken for a rattle. On the test bench: two walkers at once 85 -> 92 %, around
  explosions 72 -> 76 %; every check still passes.

- **Fixed: EAR GUARD let short spikes through.** Its 400 ms average was right, but a +35 dB blast's first
  10 ms came through at +27 dB, and a held blast stuttered between +6 and +20 dB every 400 ms. It now
  spends its allowance evenly: the first 10 ms stay under +20 dB and a held blast stays within about 3 dB.
  A single shot up to +20 dB still passes untouched. New tests check both. (Found by the website's demo,
  which runs the same maths.)
- **Simple view:** right-click the rack. It shows the five units you turn yourself, bigger; the six
  that work by themselves are put away (and keep working, as the preset set them). New installs
  start in Simple view; right-click again for all 11 units.
- **Units that are OUT rest in shade**, so you see at once what is working (less so when you walk up to one).
- **Zooming with the wheel no longer jumps** to whatever unit the pointer crossed: it keeps the unit you
  started on, and the camera glides between units instead of snapping.
- **The magnifier is gentler up close:** 2.4x from the whole rack, about 1.3x walked up to a unit.
  Walking up also calms the view's sway with the mouse and lets the room behind fall a little darker.
- **Fixed:** the dots and rings on the enhancer's and MIX BALANCER's displays grew relative to the display the
  further away the rack was (they were sized in screen pixels). They are printed at a fixed size on the display now.
- **Every glass panel says how much its unit delays the sound** (most units: none; OUTPUT MONITOR: the whole rack's).
- **A wet, lacquered look:** every glossy surface has a clear coat now. It mirrors the room the rack is in
  sharply (a big soft white light behind you, as in a product photo, the window with its bars, a pendant,
  a warm lamp, walnut), faintly face-on and strongly at grazing angles, with a tight glint of the key light.
  Knobs, glass and lacquered panels are the wettest; brushed metal only gets a soft sheen. The room is
  worked out once at start; the coat costs one texture read per pixel.
- **Router: DELAY readout** while the rack is in, with its parts when you hover it. The rack itself adds
  12 ms (checked by the tests at every sample rate, and measured, not just reported).
- **Fixed (Linux router):** the rack's audio could open at 8 kHz with a big buffer (low quality, and
  470 ms late). It now opens at 48 kHz with a 256-sample buffer, at start and on INSERT, unless you chose otherwise.
- **Fixed: COLOUR on CHARACTER was too subtle to hear at full.** Its top half now opens up: at 10 each
  model's voicing is 8x as designed (it was 4x) and the models are fed up to twice their clean point, so
  you hear warm harmonics (2 - 4 %), not just a gentle EQ. Up to 5 nothing changed, so presets sound the same.
- **Every zip has HOW-TO-CHECK.txt and SCRIPTS-SHA256.txt** besides CHECKSUMS.txt. (A zip can't hold its
  own fingerprint: check the zip itself against SHA256SUMS.txt on the release page.)
- Website redesigned, with a gallery of the test graphs.

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
