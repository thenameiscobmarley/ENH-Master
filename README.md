# ENH Master

Adaptive clarity / footstep / sub-bass enhancer for game audio (VST3, Linux),
with a real-time 3D hardware UI. Built with JUCE; tested in Carla on an Intel J4105.

## Build

Needs JUCE 8 and the [HardwareKit](../../../HardwareKit) module checked out next to this repository:

```sh
git clone https://github.com/juce-framework/JUCE ~/JUCE
git clone <HardwareKit repo> ~/Projects/HardwareKit     # or pass -DHARDWAREKIT_PATH=

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # JUCE_PATH defaults to ~/JUCE
cmake --build build -j1                                   # -j1: JUCE needs a lot of RAM per job
build/EnhDspTests_artefacts/Release/EnhDspTests           # offline DSP tests + CPU benchmark
```

Installs `~/.vst3/ENH Master.vst3`. Carla: *Add Plugin → Refresh (VST3) → ENH Master*.

**Linux/X11 only right now.** `PORTING-TO-WINDOWS.md` lists the two files that need a platform
version and includes a ready-made prompt you can hand to a coding AI to do the port.

**Documentation:** `Vault/` is an Obsidian vault (open that folder as a vault) with a tutorial from
install to tuning, plus notes on the DSP and the renderer. Start at `Vault/00 Start Here.md`.

## Reading the panels

Hover anything - printed text, a knob, a button - and a **fisheye loupe** appears over it: the scene is re-rendered
zoomed in (about 2.2x) behind a glass lens, so the magnified print is genuinely sharp rather than stretched pixels.
It magnifies about the cursor - what is under the pointer stays under the pointer - is slightly transparent, and
locks onto a control while you drag it. Controls also show a small name + value pill under the lens, and their
value arc lights up around the knob.

## Device masters

Each unit has two small master knobs:

| Control | Parameter | What it does |
|---|---|---|
| MULTIPLY | `enhMultiply` / `seraphMultiply` | 0-3x. Multiplies every knob on that device before the DSP sees it (1.5x makes CLARITY 20 behave as 30). SERAPH's OUTPUT gain is not multiplied; ENH Master has no gain knob. Values may pass a knob's printed end and are clamped to what the processing can take. |
| STRENGTH | `enhStrength` / `seraphStrength` | 0-5. How hard that device's processing hits: EQ moves, generated harmonics, sub lift, resonance dips, air, body, width, tail level and shimmer. 0 = the device does nothing; time settings (DECAY, TONE, ADAPT) are not scaled. |

## SERAPH (upper unit)

A purple finishing processor racked above ENH Master, processing its output. **POWER**: OFF (true bypass) /
SILK (tone & texture only) / HEAVEN (SILK + HALO). Zero latency.

One unified front (no channel split): a live display in the middle, the ten knobs in one row along the bottom,
the six toggles (up = on) in a grid on the right, lamp and POWER on the left. The display shows, live:

- **left**: SMOOTH's resonance dips across 150 Hz – 16 kHz, right now;
- **right**: a pair of L / R bars per process - SMOOTH, AIR, WARMTH, BODY, TAPE, LEVEL (auto gain, centre = 0 dB),
  WIDTH, SPACE, SHIMMER - each showing how much that process is changing that channel (energy of the change
  relative to the channel's own input, −42 … −6 dB).

`PAD_UI_TEST_DEMO=1` animates the display without audio (dev only).

**SILK - tone & texture**

| Control | Parameter | What it does |
|---|---|---|
| SMOOTH | `silkSmooth` | 28 detection bands (150 Hz–16 kHz). A band that suddenly sticks out of its own spectral neighbourhood gets a narrow dynamic dip, only while it sticks out. A smooth spectrum is left alone. |
| AIR | `silkAir` | high shelf + generated 10–20 kHz harmonics; more on dull sources, less while SMOOTH is busy |
| WARMTH | `silkWarmth` | envelope-normalised low-mid harmonics + gentle triode curve |
| BODY | `silkBody` | low-mid fullness at 180 Hz, only as much as the source is thin |
| OUTPUT | `silkOutput` | trim, ±12 dB |
| PROTECT | `silkProtect` | lifts a dip the moment an attack arrives and halves dips in 1–4.5 kHz (footsteps keep their bite) |
| TAPE | `silkTape` | pre-emphasised soft saturation that rounds harsh transients |
| AUTO | `silkAuto` | loudness-matched output |

SERAPH's stages end in an output limiter (instant gain-down above -0.7 dBFS, 80 ms recovery, then a soft clip), so
MULTIPLY 3x with STRENGTH 5 on both units still lands at 0.92 peak. HALO also has early reflections (sparse stereo
taps, 7-37 ms) ahead of the dense tail.

**HALO - space & width**

| Control | Parameter | What it does |
|---|---|---|
| WIDTH | `haloWidth` | 0–200 %; near-mono sources get decorrelated width added to the side only, so the mono sum never changes |
| SPACE | `haloSpace` | amount of an 8-line FDN reverb (Hadamard mixing, per-line damping, 4 diffusers, 18 ms pre-delay), fed from the mid above 200 Hz |
| DECAY | `haloDecay` | 0.3–8 s |
| SHIMMER | `haloShimmer` | octave-up pitch shift fed back into the tail |
| TONE | `haloTone` | dark plate … airy hall |
| DUCK | `haloDuck` | tail ~10 dB lower while the music is busy, blooms in the gaps |
| BASS MONO | `haloBassMono` | Linkwitz-Riley split, side removed below 120 Hz |
| MOD | `haloMod` | slow delay modulation for a lusher tail |

## Controls

| Control | Parameter | What it does |
|---|---|---|
| MODE | `clarityMode` | Lever down = **NORM** (normalise only), lever up = **ADD + NORM** (normalise and add enhancement) |
| CLARITY (NORM) | `clarityNorm` | 0–30: strength of the source-dependent correction + loudness match; adds nothing (no harmonics, no lift) |
| CLARITY (ADD) | `clarityAdd` | 0–10: correction **plus** adaptive DEPTH/CLARITY harmonics and a detail lift where the planner finds definition/body missing |
| ADAPT | `adaptSpeed` | How fast every adaptive gain, the long-term spectrum and the harmonic centres follow the material |
| SUB | `sub` | Dynamic sub lift + psychoacoustic bass harmonics (tuned to the playing bass note) |
| +BOOST | `subBoost` | Higher sub ceiling, 55 Hz punch, more harmonics |
| FOOTSTEP | `footstep` | Footstep classification → lifts the bands the step actually rose in, ducks nearby maskers |

CLARITY is one knob with two scales: pressing MODE swaps the printed scale (white 0–30 for NORM, amber 0–10 for
ADD + NORM) and the knob turns to that mode's own stored setting. NORM 30 and ADD 10 normalise equally hard.

Front panel (Pro-XL style): charcoal faceplate with outlined CLARITY / SUB / FOOTSTEP / METER sections,
small black knobs over fixed printed scales, latching push buttons with an LED above (MODE has NORM + ADD LEDs),
and LED ladders: DETECT (footstep confidence), OUT (output peak, -30…0 dB), ENH (live enhancement activity).
Knobs: drag vertically (Shift/Ctrl = fine), wheel to nudge, double-click to reset. The pointer line tints by who
moved it while moving: gold user, cyan host automation, violet self-tune.

The display shows the applied adaptive EQ response (24 bands, 40 Hz – 16 kHz) and flashes on detected footsteps.
The lid vents glow with enhancement activity (pink) and detected footsteps (lime).

## Signal chain (Source/DSP)

```
input ─► analysis: BandAnalyzer (24 log bands + long-term spectrum), SpectralAnalyzer (FFT tonality),
  │                FootstepDetector (event classifier), HarmonicPlanner, sub follower
  └─► AdaptiveEQ (source-derived curve + footstep lift, solved 24-band peaking cascade)
      ─► SubEnhancer ─► AnalogStage
         (subsonic HP, loudness-matched auto gain,
          2x oversampled adaptive DEPTH/CLARITY exciters + transformer colour + soft ceiling)
```

- **No preset curve.** There is no built-in "smile": on pink noise the EQ stays within ±0.1 dB. The curve comes from the
  programme's long-term spectrum: resonances are cut and holes filled where they are, and treble (2.5–10 kHz) and bass
  (40–160 Hz) are rebalanced only when clearly out of range relative to the midrange. The curve is anchored on the
  midrange; cuts in 250 Hz – 5 kHz are limited to −1.5…−3 dB and never land on the current footstep's bands.
- **Accurate response.** The 24 overlapping peaking filters are solved (regularised least squares), so the applied response
  matches the displayed curve (≤ 0.03 dB in tests).
- **Adaptive harmonics.** HarmonicPlanner scores every band for "real content here, but its 2nd/3rd harmonic region is
  comparatively empty" and centres the DEPTH (sources 70–500 Hz) and CLARITY (600 Hz–5 kHz) exciters there. Each exciter
  isolates its band with a moving SVF, normalises it by its own envelope and uses Chebyshev polynomials (T2/T3), so real
  2nd/3rd harmonics are generated at the same relative strength for quiet and loud sounds.
- **Footstep classifier.** Every onset is an event judged on behaviour, not just frequency: it must decay quickly, be
  noise-like (no persistent spectral peaks in its new energy — latches, hinges, creaks, chimes and voices have them),
  not be one of a burst of similar onsets < 200 ms apart (rattles, rummaging), not follow a rejected ringing/rattling
  event within ~1 s, not be hot/broadband (gunfire). Steps that continue the walking rhythm and match the learned
  spectral fingerprint get the full lift; a lone event a moderate one. Timeline: +4 ms slight provisional lift,
  +42 ms decision, until +160 ms a watch that retracts ringing/sustained events. The 1–4 kHz region is a gentle
  preference for where to lift, not a requirement for detection.
- **PD control:** every adaptive gain follows its target with value' = (Kp·e + Kd·target') / (1 + Kd).
- **Latency:** IIR only; the reported latency is the oversampling filters' (a couple of samples).
- **CPU (J4105, 48 kHz stereo):** ~10–11 % of one core.

## Test results (`EnhDspTests`, synthetic scenes)

- Crate openings (latch ring, lid creak, rummage rattles, lid thud) between walking: v4 flags 3 of 8 crates, only as
  brief blips (none lifted > 150 ms), lift active 3–4 % of crate time — v3 flagged 8/8 and lifted
  54–61 % of crate time. Steps around crates: 96–100 % (v3: 96–100 %).
- Footsteps over ambience with gunshots and voice: 83–92 % classic, 76–86 % mixed surfaces (v3: 86–100 %; misses are
  steps under loud speech); ≤ 1 gunshot flagged; ≤ 6.5 % of voice time flagged. Quiet scene 23/23.
- AutoEQ: pink ±0.1 dB; muffled source +2 dB treble vs bass; thin source the opposite; 600 Hz resonance −2.1 dB;
  1.5 kHz hole +1.8 dB; 1–4 kHz never scooped.
- Harmonics: centres track 220 Hz / 1 kHz / 2.5 kHz sources; 2nd harmonic −15…−21 dB rel. fundamental at full CLARITY
  (−50 dB off), identical at −18 and −42 dBFS input.
- Neutral settings ±0.04 dB; auto gain within ~1.5 dB; SUB +5.6 dB at 50 Hz, +10.3 dB with BOOST; stable at
  44.1/48/96 kHz and block sizes 1–1024; output peak ≤ 0.99.

These are synthetic sounds — real game mixes will differ. To check a real capture:

```sh
build/EnhDspTests_artefacts/Release/EnhDspTests --analyze capture.wav [clarity 0..1]
```

prints every accepted / rejected event with its reasons, the EQ curve and the harmonic centres every 5 s.
`--events quiet|game|varied|crates [seed]` does the same for the synthetic scenes with ground truth.

## Model library

The 3D side lives in a shared JUCE module, **HardwareKit** (`~/Projects/HardwareKit`), so other plugins can reuse it:
geometry primitives, hardware models (knob styles, push buttons, bat toggles, jewel lamps, LEDs, rack screws,
chassis), materials, control animation, X11 pointer / visibility helpers and the fisheye loupe. ENH Master keeps only
what is specific to it: its layout, its printed panels and the two display shaders. See `HardwareKit/README.md`.

Knob styles in use: ENH Master's CLARITY / ADAPT / SUB are `proXl`, its masters `aluminium`; SERAPH's row is
`fluted` (Davies type), its masters `softTouch` with violet caps, and POWER is a `chickenHead` selector.

## UI performance

Knobs light a value arc while hovered or moving, LEDs and lamps cast soft halos, and clicks are ignored unless the
plugin window is really the topmost window under the pointer (so clicking in an app that covers it cannot move knobs).

Rendering pauses completely while the editor window is minimised or hidden (checked a few times a second through
JUCE's peer state and the X server, so a minimised plugin host is noticed too); audio processing is unaffected.
Measured on the standalone: 17 % of one core visible, 8 % minimised (audio only), rendering resumes on restore.

Continuous repainting locked to vsync, paced on the render thread (60 fps active / 30 fps idle).
The pointer is polled from X11 each frame on the render thread, so parallax stays smooth even when the
host delivers mouse events late. Config: `~/.config/ENHMaster/ui-config.json`.

## Dev hooks (no effect unless set)

- `PAD_UI_TEST_PARAMS="clarity=0.8;footstep=1"` – writes normalised values as host automation after 1.5 s
- `PAD_UI_TEST_SIZE=520x250` – initial editor size
- `PAD_UI_TEST_STATS=1` – frame-timing statistics on stderr every 5 s (also logs when rendering pauses / resumes)
- `PAD_UI_TEST_MINIMISE="7,17"` – minimises the window at 7 s and restores it at 17 s
- `PAD_UI_TEST_HOVER="x,y"` – shows the hover loupe at that point
- `PAD_UI_TEST_DEMO=1` – animates SERAPH's live display without audio

## Backups

Git tags `backup/phase1-top-panel-full-controls`, `backup/front-panel-2-knobs`, `backup/enh-master-v3-footstep-ranges`, …;
source tarballs and built bundles in `~/Projects/PvPAdaptiveDynamics-backups/`.
