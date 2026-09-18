# ENH Master

Adaptive clarity / footstep / sub-bass enhancer for game audio and music production (VST3, Linux),
with a real-time 3D hardware UI. Built with JUCE; tested in Carla on an Intel J4105.

Five processors in one plugin, in signal order: the **ADAPTIVE ENHANCER** (adaptive EQ, generated
harmonics, sub, footstep priority), the **UPWARD LEVELER** (three-band, lifts quiet material), the
**SPECTRAL LIMITER** (cuts abnormal spectral excess where it is, so a bass hit does not duck the whole
mix), the **ADAPTIVE COMPRESSOR** (threshold follows the programme) and **TONE & SPACE** (tone, space
and loudness hold).

Unit names describe what each unit does. Until September 2026 they were called ENH MASTER, LUMEN,
TIDE and SERAPH (SILK / HALO / HEAVEN). Parameter IDs keep those old names, so saved sessions and
automation still load; only the names the host and the panels show have changed.

![ENH Master](docs/screenshot.png)

![Hovering a label: the loupe and the value callout](docs/screenshot-hover.png)

*Five units in a curved case, bottom to top in signal order: ADAPTIVE ENHANCER (clarity, sub,
footsteps and the analyser), UPWARD LEVELER, SPECTRAL LIMITER, ADAPTIVE COMPRESSOR, TONE & SPACE.*

## Build

The easy way is the builder script. It checks your tools and offers to download JUCE and
[HardwareKit](https://github.com/thenameiscobmarley/HardwareKit) if they're missing. Then it asks two
questions and builds:

```sh
./build.sh
```

- **How many CPU cores:** 1 (slowest, lightest on memory), all of them (fastest), or a number you choose.
- **Replace or copy:**
  - *replace* installs over `~/.vst3/ENH Master.vst3`, the copy your DAW loads.
  - *copy* puts a separate build in `dist/ENH-Master-<version>-<date>/` and leaves the installed
    plugin alone.

You can also skip the questions with flags: `./build.sh --jobs all --mode replace`, or `--jobs 2`,
`--mode copy`, `--tests` (run the DSP tests afterwards), `--clean` (build from scratch), and `-y` (use
the defaults). See `./build.sh --help`. If the compiler runs out of memory with many cores, it offers
to retry with half as many. The build log is `build/builder.log`.

By hand, the same thing:

```sh
git clone https://github.com/juce-framework/JUCE ~/JUCE
git clone https://github.com/thenameiscobmarley/HardwareKit ../HardwareKit   # or pass -DHARDWAREKIT_PATH=

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # JUCE_PATH defaults to ~/JUCE
cmake --build build -j"$(nproc)"                          # all cores; add -DENH_COPY_PLUGIN=OFF to not install
build/EnhDspTests_artefacts/Release/EnhDspTests           # offline DSP tests + CPU benchmark
```

Installs `~/.vst3/ENH Master.vst3`. Carla: *Add Plugin → Refresh (VST3) → ENH Master*.

**Linux/X11 only right now.** `PORTING-TO-WINDOWS.md` lists the two files that need a platform
version and includes a ready-made prompt you can hand to a coding AI to do the port.

**Documentation:** `Vault/` is an Obsidian vault (open that folder as a vault) with a tutorial from
install to tuning, plus notes on the DSP and the renderer. Start at `Vault/00 Start Here.md`.

## Reading the panels

The four units sit on an arc centred on the viewer, so however many are stacked, every panel faces
the camera head on and nothing is foreshortened. Scroll or click a panel to walk up to a unit;
click the case to step back to the whole rack.

Hover anything - printed text, a knob, a button - and a **fisheye loupe** appears over it: the scene is re-rendered
zoomed in (about 2.2x) behind a glass lens, so the magnified print is genuinely sharp rather than stretched pixels.
It magnifies about the cursor - what is under the pointer stays under the pointer - is slightly transparent, and
locks onto a control while you drag it. Controls also show a small name + value pill under the lens, and their
value arc lights up around the knob.

## Presets

Nine factory presets set the whole rack at once. They appear in the host's program list, and on the
rack itself: PRESET PREV / NEXT in the ADAPTIVE ENHANCER's maker block. The name shows on the
analyser for a few seconds (and while a PRESET button is hovered). Every preset is a complete
state: anything it does not list goes back to its default.

**The presets live in a local file, not in the plugin**, so they can be edited and re-tuned (by hand
or by an AI) without rebuilding:

| | |
|---|---|
| Linux | `~/.config/ENH Master/presets.json` |
| Windows | `%APPDATA%\ENH Master\presets.json` |
| macOS | `~/Library/ENH Master/presets.json` |
| any | `$ENH_MASTER_PRESETS` overrides the path |

- The plugin writes the factory presets there the first time it runs.
- After that the file is the source. Edits are picked up the next time you press PREV / NEXT or the
  host lists its programs. There's no need to restart.
- The file has a `parameters` block listing every parameter's range, default, units and what its
  0 / 1 or choice numbers mean.
- Values are clamped to their range and unknown names are skipped. A file that doesn't parse is
  ignored, and the last good list stays in use.
- Delete the file to get the factory presets back. They're in `Source/Parameters/FactoryPresets.h`.

`EnhDspTests --presets` tests the same file, so a re-tuned file can be checked as it stands.

| Preset | For |
|---|---|
| DEFAULT | every unit at its default |
| COMPETITIVE FOOTSTEPS | PvP: footstep priority, quiet detail lifted, explosions cut where they are, TONE only (no reverb smearing position), stereo width untouched |
| IMMERSIVE GAMES | single-player / cinematic: sub lift, space and width, hits still controlled |
| NIGHT MODE | quiet listening: small level range, sudden loud events held down hard |
| BASS HEAVY, PROTECTED | big low end (SUB + BOOST) without pumping: deep spectral limiting, bass mono |
| VOICE & STREAMING | speech first: intelligibility, even level, no tail |
| MUSIC: WARM MASTER | tape warmth, a touch of room, leveler OUT so music keeps its dynamics |
| MUSIC: WIDE & AIRY | open top, wide image, a lush modulated hall |
| TRANSPARENT (ALL OUT) | reference: everything bypassed (the enhancer's subsonic filter and the output safety limiter stay) |

`EnhDspTests --presets` runs every preset through the engine on the synthetic game scene and a bass
hit, IN and OUT of the SPECTRAL LIMITER. Every preset is stable and under full scale, and in every
one the limiter reduces how much the 2 kHz detail ducks under the hit. Numbers from the last run
(dip of the detail, limiter IN / OUT):

| Preset | IN | OUT |
|---|---|---|
| DEFAULT | -1.1 dB | -1.4 dB |
| COMPETITIVE FOOTSTEPS | -1.8 dB | -2.0 dB |
| IMMERSIVE GAMES | -0.4 dB | -1.0 dB |
| NIGHT MODE | -0.8 dB | -1.7 dB |
| BASS HEAVY, PROTECTED | -4.4 dB | -5.4 dB |
| VOICE & STREAMING | -1.1 dB | -1.3 dB |
| MUSIC: WARM MASTER | -0.8 dB | -1.3 dB |
| MUSIC: WIDE & AIRY | -0.4 dB | -0.7 dB |

With the whole rack running, what is left is the rack's output limiter catching a hot mix. The
spectral limiter alone takes the dip from -1.8 to -0.3 dB.

Two level-matching loops now hold still while the SPECTRAL LIMITER is handling a localised spike:

- **UPWARD LEVELER band gains.** Its complementary crossover lets a big bass hit read as a louder
  mid and top band, so it used to pull the lift on footsteps and detail back on every hit.
- **The enhancer's auto gain.** It used to chase the sub-enhanced spike.

## SPECTRAL LIMITER - anti-pumping dynamic EQ (1U)

Sits between the leveler and the compressor. The compressor's detector is broadband, so a sudden
bass hit used to pull everything down with it, footsteps and detail included. This unit handles the
hit where it is, in this order:

1. **Localised excess:** up to three moving cuts follow the offending region. Each is a bell, or a
   shelf when the excess runs off the bottom or top of the spectrum. Nothing else moves.
2. **Headroom threatened** (stage peak above CEILING): the same region is cut deeper, by as much as
   that region's share of the energy says is needed.
3. **Broadband:** gain reduction only when the abnormal energy covers most of the spectrum *and*
   headroom is threatened.

The compressor is keyed through the same cuts, made deeper: while a localised event is being handled
here, the compressor does not duck the whole mix for it as well. That's an adaptive version of the
side-chain high-pass on a bus compressor. With nothing flagged, the key is the audio itself.

Detection reuses the ADAPTIVE ENHANCER's 24-band analyser, adding no second analyser. Per band it adds:

- a rolling baseline that barely moves while the band is flagged;
- what is *normal* for that band: the 97th percentile of its excursions over the last 10 s, not
  counting the newest 1.5 s;
- leakage rejection, so a 70 Hz hit does not also count as 400 Hz.

A kick drum teaches it that its hits are normal, so normal bass is left alone. Nothing is hard-coded
to a frequency: a 5 kHz whistle is cut at 5 kHz.

| Control | What it does |
|---|---|
| RANGE | deepest spectral cut, 0-18 dB |
| RELEASE | how fast a cut lets go, 30-600 ms (attack follows it, 1.5-8 ms) |
| CEILING | headroom protection threshold at this stage, -12..0 dBFS |
| IN | hardware bypass |

The meters read the deepest spectral cut (0-18 dB) and the broadband protection (0-12 dB). The
analyser on the ADAPTIVE ENHANCER shows where it is cutting: a magenta curtain hanging from the top of
the plot at the real frequency and depth (amber for broadband), and a `LIMIT -9 dB @ 68 Hz` readout.

Measured (`EnhDspTests --limiter`), with a huge bass hit over a mix with kicks and a 2 kHz detail band:

| | 2 kHz detail during the hit | compressor gain reduction |
|---|---|---|
| limiter OUT | -1.8 dB | 14.6 dB |
| limiter IN | -0.3 dB | 10.4 dB |

- The hit gets an 8-9 dB cut around 68 Hz; the kicks get 0.00 dB.
- The cut releases within 0.7 s.
- A broadband event gets no spectral cut (0.1 dB), only 2.9 dB of broadband protection.
- The result is identical at block sizes 7, 128 and 1024.
- Cost: about 1.1 % of one J4105 core at 48 kHz.

## ADAPTIVE COMPRESSOR (1U)

Two controls, no threshold knob. The threshold, ratio, knee and ballistics all follow the
programme: loudness now and over the last seconds, crest factor, spectral tilt, transient density
and a running estimate of the loud part of the material. Peaky material gets a higher threshold and
a gentler ratio so transients survive; dense material is held down.

| Control | What it does |
|---|---|
| MIX | wet / dry, after auto make-up, so the blend does not change the level |
| RESPONSE | how fast it reacts and how deep into the programme the threshold sits |
| IN | hardware bypass |

The VU reads gain reduction, 0-12 dB.

## UPWARD LEVELER - three-band (1U)

Lifts quiet material toward a target, per band, because "quiet" is rarely true of a whole signal at
once. LR4 splits (which sum back to the input exactly), a per-band estimate of what is loud here
and now, a noise floor that settles on the quiet moments, and gates on absolute level and
modulation so hiss and room tone are never lifted.

| Control | What it does |
|---|---|
| TARGET | the level quiet material is brought toward (-36 to -6 dBFS) |
| RESPONSE | how quickly it follows, and how hard the slew limits bite |
| IN | hardware bypass |

Three VUs read the lift in the low, mid and high bands, 0-18 dB.

## Device masters

Each unit has two small master knobs:

| Control | Parameter | What it does |
|---|---|---|
| MULTIPLY | `enhMultiply` / `seraphMultiply` (TONE & SPACE) | 0-3x. Multiplies every knob on that device before the DSP sees it (1.5x makes CLARITY 20 behave as 30). TONE & SPACE's OUTPUT gain is not multiplied; ENH Master has no gain knob. Values may pass a knob's printed end and are clamped to what the processing can take. |
| STRENGTH | `enhStrength` / `seraphStrength` | 0-5. How hard that device's processing hits: EQ moves, generated harmonics, sub lift, resonance dips, air, body, width, tail level and shimmer. 0 = the device does nothing; time settings (DECAY, TONE, ADAPT) are not scaled. |

## TONE & SPACE (top unit)

A purple finishing processor at the top of the case, processing everything below it. **LOUDNESS** is
its level policy: one knob with two printed scales and a button to swap them. In HOLD it measures
what came in and what is going out and works the output back toward the input, so the effect is
loud enough to hear and never louder than the music. In LIFT + HOLD it adds gain first and then
holds *that* steady, for sources that are quiet to begin with. **POWER**: OFF (true bypass) /
TONE (tone & texture only) / +SPACE (TONE + SPACE). Zero latency.

**AUTO + HEAVEN** (right of the display): press AUTO and the unit tunes its own heaven. It listens to the
programme over a few seconds (sustained or percussive, mono or wide, dull or bright, thin or full in the
low end) and moves REVERB, DECAY, SHIMMER, SPACE TONE, WIDTH, AIR and SUB toward what that programme
wants: long, shimmering and wide for pads and ambient music, shorter and drier for busy percussive material,
more air on dull sources, more SUB on thin ones. **HEAVEN** sets how far it goes, from 0 (your knobs) to
10 (all AUTO). The move is smoothed over seconds, so the space never jumps. You can watch it work: the
knobs turn themselves to what AUTO is applying, and turn back to your settings when AUTO is off. Your
saved settings never change, and a knob you are holding shows your own value. **MATCH** does the same
with OUTPUT: the knob shows the level-match gain it is adding.

One unified front (no channel split): a live display in the middle, the ten knobs in one row along the bottom,
the six I / O rocker switches (I = on) in a grid on the right, lamp and POWER on the left. The display shows, live:

- **left**: SMOOTH's resonance dips across 150 Hz – 16 kHz, right now;
- **right**: a pair of L / R bars per process - SMOOTH, AIR, WARMTH, BODY, TAPE, LEVEL (auto gain, centre = 0 dB),
  WIDTH, SPACE, SHIMMER - each showing how much that process is changing that channel (energy of the change
  relative to the channel's own input, −42 … −6 dB).

`PAD_UI_TEST_DEMO=1` animates the display without audio (dev only).

**TONE - tone & texture**

| Control | Parameter | What it does |
|---|---|---|
| SMOOTH | `silkSmooth` | 28 detection bands (150 Hz–16 kHz). A band that suddenly sticks out of its own spectral neighbourhood gets a narrow dynamic dip, only while it sticks out. A smooth spectrum is left alone. |
| AIR | `silkAir` | high shelf + generated 10–20 kHz harmonics; more on dull sources, less while SMOOTH is busy |
| WARMTH | `silkWarmth` | envelope-normalised low-mid harmonics + gentle triode curve |
| BODY | `silkBody` | low-mid fullness at 180 Hz, only as much as the source is thin |
| OUTPUT | `silkOutput` | trim, ±12 dB |
| PROTECT | `silkProtect` | lifts a dip the moment an attack arrives and halves dips in 1–4.5 kHz (footsteps keep their bite) |
| TAPE | `silkTape` | pre-emphasised soft saturation that rounds harsh transients |
| SUB | `silkSub` | heaven for the low end: a clean 80 Hz shelf, a warm envelope-normalised 2nd harmonic (so the bass is felt on small speakers too) and a soft mono bloom that swells in the gaps after bass notes. All three back off as the bass gets loud, so a big low end is never pushed into the limiter. |
| MATCH | `silkAuto` | loudness-matched output (the rocker was labelled AUTO before 1.2.0) |

**LOUDNESS** is TONE & SPACE's level policy: one knob with two printed scales and a button to swap them.
In HOLD it measures what came in and what is going out and works the output back toward the
input, so the effect is loud enough to hear and never louder than the music. In LIFT + HOLD it
adds gain first and then holds *that* steady, for sources that are quiet to begin with.

LOUDNESS measures K-weighted power (bass counts less, as it does for the ear) over ~2 s and moves its gain
over ~3 s, so a bass note no longer pulls the whole mix down and lets it back up when it stops (pumping).
The rack ends in one lookahead output limiter (1.5 ms lookahead, gain held for 25 ms, 150 ms release, ceiling
-0.5 dBFS): its gain never moves inside a bass cycle, so loud bass is not crushed. SPACE also has early reflections (sparse stereo
taps, 7-37 ms) ahead of the dense tail.

**SPACE - space & width**

| Control | Parameter | What it does |
|---|---|---|
| WIDTH | `haloWidth` | 0–200 %; near-mono sources get decorrelated width added to the side only, so the mono sum never changes |
| SPACE | `haloSpace` | amount of an 8-line FDN reverb (Hadamard mixing, per-line damping, 4 diffusers, 18 ms pre-delay), fed from the mid above 200 Hz |
| DECAY | `haloDecay` | 0.3–8 s |
| SHIMMER | `haloShimmer` | pitch-shifted copies fed back into the tail: an octave up and, quieter, an octave and a fifth up |
| TONE | `haloTone` | dark plate … airy hall |
| DUCK | `haloDuck` | tail ~10 dB lower while the music is busy, blooms in the gaps |
| BASS MONO | `haloBassMono` | Linkwitz-Riley split, side removed below 120 Hz |
| MOD | `haloMod` | slow delay modulation for a lusher tail, and a slow drift of the tail around the stereo field (~20 s a cycle) |

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

## Footstep detection adapts to the game

The classifier still rejects crates, gunshots, voices and rattles. Two things now adapt to the
programme:

- **How fast steps die away** is learnt from the steps it accepts. In a reverberant game, or on wood
  and carpet, steps decay more slowly, and the decay test scales with them (never below about half
  the default). The 110 ms sustain check scales the same way.
- **A second look at 70 ms** is given to an event that failed only on decay: over a longer window,
  with no lift while it waits.

Both apply only to real impacts:
- the event peaks within 12 ms of its onset (a syllable swells over tens of ms);
- it stands at least 14 dB above its own background;
- it is outside tonal activity.

Under a voice, the default rules stand.

Reverberant rooms (RT about 0.6 s, `EnhDspTests`):

| | Seed 5 | Seed 88 |
|---|---|---|
| Fixed rules | 60 % | 50 % |
| Adaptive | 73 % | 67 % |

Voice false time in those rooms is unchanged, and every other detection test holds.

## ADAPTIVE COMPRESSOR: dual release

A slow follower carries the average gain reduction; a fast one takes only what a transient needs,
and gives it back in about 50 ms. In the test, a steady tone under a kick every 0.5 s sits 0.69 dB
off its level on average, against 1.02 dB with the old single release: 32 % less pumping at the
same average gain reduction.

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

Knob styles in use: every knob is `chickenHeadKnob` (black bakelite, beak ending at the ticks); POWER is a
`chickenHead` selector (the long-beak version).

## Anti-aliasing

- **Main view:** 4x MSAA by default. `msaaSamples` in `~/.config/ENHMaster/ui-config.json` takes
  0 / 2 / 4 / 8. On an Intel UHD 600, 8x costs noticeably more late frames, so 4x stays the default.
- **Loupe:** now rendered multisampled too (it used to be aliased).
- **Procedural textures** (HardwareKit): brushed and powder-coat noise, lacquer flake and knob grip
  ridges are band-limited by pixel footprint. Where a pixel covers many cells they fade to their
  average instead of crawling or forming moire as the camera moves.

## The rack

The front mounting rails are straight segments, one per unit, as in a real curved cabinet. Each
lies flat against the back of its unit's faceplate, meeting its neighbours in the middle of each gap.
They are zinc-plated, with square rack holes under every ear screw. The units sit 0.03 inside the
cheeks' arc, and a contact shadow runs where the ears clamp to the rails.

## Hardware styles

HardwareKit now has 50 knob styles, 5 switch styles and 5 button styles. Most knob styles are
recipes (skirt, body shape, grip carving, cap, pointer, materials) built by one generator at every
level of detail:

- console knobs with coloured caps;
- vintage: Marconi, bakelite, cream radio, chrome-skirt cones, wing pointers;
- machined metal: silver, black, gunmetal, brass, crosshatch, stepped, chrome dome;
- instrument collets and Eurorack knobs;
- rubber knobs and pointer bars;
- hi-fi discs;
- guitar top-hats and speed knobs.

Switches: I / O rocker (standard, red, wide), bat toggle, paddle toggle. Buttons: square, round,
wide, chrome bezel, soft dome. `PAD_UI_TEST_GALLERY=1` lays every style out on the enhancer (dev only).

Every knob on the rack is a black bakelite chicken head: a round back end that tapers to a
pointed beak, with a painted white line along it. The knobs' beaks stop at their printed ticks, and the
POWER selector's reaches out over its positions. Plastics are satin and the pointer lines are paint, not
glowing. The panels follow the same idea: a model number instead of feature lists, plain black meter
bezels, single-colour displays, and no animated signal-flow arrows.

In use:

| Unit | Knobs | Switches and buttons |
|---|---|---|
| every unit | black chicken heads (POWER: the long-beak selector version) | |
| ADAPTIVE ENHANCER | | square buttons |
| TONE & SPACE | | chrome-bezel LIFT button, round AUTO button with its own LED |
| SPECTRAL LIMITER | | red I / O rocker |

The rockers are 30 % larger than in 1.1.

## Detail and resolution

- **Detail levels:** every knob, selector, push button and switch is built at four levels (32, 64,
  128 and 256 segments round). Each frame, each control is drawn at the level its size on screen
  calls for, including inside the zoomed loupe. So walking up to the rack, or scrolling to zoom,
  brings in finer geometry.
- **Carved detail** (levels 2 and 3) is real geometry, not painted on:
  - knurled and ribbed grips and flutes;
  - brushed inserts and trim rings;
  - set screws;
  - anodised cap inserts in muted unit colours: petrol (compressor), bronze (leveler), oxblood
    (limiter).
- **Switches:** I / O rocker switches, with the I end pressed in when on (see Hardware styles).
- **Screws:** pan-head ear screws on washers.
- **Printed panels:** 4096 px wide, mipmapped, so the GPU uses the resolution the camera needs and
  the print sharpens as you get closer. Scale rings are 1024 px, meter faces up to 1536 px.
- **Speed:** `maxDetail` (0 to 3) in `ui-config.json` caps the finest level on slow GPUs. On a J4105,
  walked up to TONE & SPACE runs at about 38 ms a frame at level 3 and 35 ms at level 1.

## Layout: how units are placed, and LayoutViz

Everything in the scene is in world units, not pixels, and nothing in it depends on the window:

- **Units:** placed on a vertical arc from constants in `Source/UI/Scene/DeviceLayout.h`
  (`unitArcPos` / `unitAngle` / `unitOrigin` / `panelToWorld`).
- **Controls:** panel-local constants (x across, z down the faceplate). Some are calculated as first
  position + step x index. Knob sizes are a fixed base radius times a fixed size multiplier.
- **The window:** only reaches the camera (`CameraRig::build`), which moves back until the whole
  case fits. So on screen everything scales together with the editor size.

`LayoutViz` (built with the plugin, `ENH_BUILD_TOOLS`) uses that same code to draw the layout for any
editor size. It writes an SVG with the window boundary, a pixel grid and rulers, every unit's
projected faceplate, centre and size, and every control's footprint (hover for its panel values). It
also prints the underlying values, each marked FIXED, MULTIPLIER or CALCULATED:

```sh
WIDTH=1200 HEIGHT=700 OUT=layout.svg build/LayoutViz_artefacts/Release/LayoutViz
FOCUS=4 WIDTH=1340 HEIGHT=720 OUT=limiter.svg build/LayoutViz_artefacts/Release/LayoutViz   # walked up to unit 4
```

It is analysis only; it does not change the plugin.

## UI performance

Knobs light a value arc while hovered or moving, LEDs and lamps cast soft halos, and clicks are ignored unless the
plugin window is really the topmost window under the pointer (so clicking in an app that covers it cannot move knobs).

Rendering pauses completely while the editor window is minimised or hidden (checked a few times a second through
JUCE's peer state and the X server, so a minimised plugin host is noticed too); audio processing is unaffected.
Measured on the standalone with all four units running and the analyser live: 23 % of one core
visible, 8 % minimised (audio only), rendering resumes on restore. The DSP itself is 16.8 % of that
at 48 kHz; the rest is the renderer, which is draw-call bound rather than fill bound.

Continuous repainting locked to vsync, paced on the render thread (60 fps active / 30 fps idle).
The pointer is polled from X11 each frame on the render thread, so parallax stays smooth even when the
host delivers mouse events late. Config: `~/.config/ENHMaster/ui-config.json`.

## Dev hooks (no effect unless set)

- `PAD_UI_TEST_PARAMS="clarity=0.8;footstep=1"` – writes normalised values as host automation after 1.5 s
- `PAD_UI_TEST_SIZE=520x250` – initial editor size
- `PAD_UI_TEST_STATS=1` – frame-timing statistics on stderr every 5 s (also logs when rendering pauses / resumes)
- `PAD_UI_TEST_MINIMISE="7,17"` – minimises the window at 7 s and restores it at 17 s
- `PAD_UI_TEST_HOVER="x,y"` – shows the hover loupe at that point
- `PAD_UI_TEST_DEMO=1` – animates TONE & SPACE's live display and the SPECTRAL LIMITER's meters and analyser curtain without audio
- `PAD_UI_TEST_STATS=1` also logs the framebuffer's actual MSAA sample count
- `PAD_UI_TEST_FOCUS=<unit>` – starts walked up to one unit (0 enhancer, 1 tone & space, 2 compressor, 3 leveler, 4 limiter)
- `PAD_UI_DUMP_ARTWORK=<dir>` – writes every printed panel with its text boxes and the hardware footprints from the layout code, plus `clearances.txt` listing any print that overlaps or crowds hardware, borders or other print

## Backups

Git tags `backup/phase1-top-panel-full-controls`, `backup/front-panel-2-knobs`, `backup/enh-master-v3-footstep-ranges`, …;
source tarballs and built bundles in `~/Projects/PvPAdaptiveDynamics-backups/`.
