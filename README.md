# ENH Master

Adaptive clarity / footstep / sub-bass enhancer for game audio (VST3, Linux),
with a real-time 3D hardware UI. Built with JUCE; tested in Carla on an Intel J4105.

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # JUCE_PATH defaults to ~/JUCE
cmake --build build -j1                                   # -j1: JUCE needs a lot of RAM per job
build/EnhDspTests_artefacts/Release/EnhDspTests           # offline DSP tests + CPU benchmark
```

Installs `~/.vst3/ENH Master.vst3`. Carla: *Add Plugin → Refresh (VST3) → ENH Master*.

## Controls

| Control | Parameter | What it does |
|---|---|---|
| CLARITY | `clarity` | Adaptive masking-aware spectral leveling + transient-aware harmonic detail + air |
| ADAPT | `adaptSpeed` | How fast every adaptive gain follows the material (PD controller gain) |
| SUB | `sub` | Dynamic sub lift + psychoacoustic bass harmonics |
| +BOOST | `subBoost` | Higher sub ceiling, 55 Hz punch, more harmonics |
| FOOTSTEP | `footstep` | Footstep detection → lifts step regions, ducks their maskers |

Knobs: the numbered skirt turns; the value is the number under the fixed indicator.
Drag vertically (Shift/Ctrl = fine), wheel to nudge, double-click to reset.
Indicator colour = who moved it: gold user, cyan host automation, violet self-tune.

The display shows the live adaptive EQ curve (24 bands, 40 Hz – 16 kHz) and flashes on detected footsteps.
The lid vents glow with enhancement activity (pink) and detected footsteps (lime).

## Signal chain (Source/DSP)

```
input ─► BandAnalyzer (24 log bands, followers, rolling stats) ─► FootstepDetector
  └─► AdaptiveEQ (24 dynamic peaking filters, per-band PD controllers)
      ─► footstep focus EQ ─► SubEnhancer ─► AnalogStage
         (subsonic HP, transformer bump, air, loudness-matched auto gain,
          2x oversampled exciter + transformer colour + soft ceiling)
```

- **Any frequency:** 24 overlapping peaking bands whose gains form a smooth curve across the spectrum.
- **PD control:** every adaptive gain follows its target with value' = (Kp·e + Kd·target') / (1 + Kd);
  Kp is set by ADAPT and self-tuned per band from rolling variance; targets are extrapolated by the
  analysis delay (latency compensation). Cuts react faster than lifts to avoid pumping.
- **Footstep detector:** onset in the thump (70–260 Hz) / scuff (1.8–7 kHz) regions vs. their own
  background; rejects hot/broadband events (gunfire, explosions), mid-dominated onsets (voice, weapon
  bodies) and sustained tails; 4 ms onset confirmation; walking-rhythm prior.
- **Latency:** IIR only; the reported latency is the oversampling filters' (a couple of samples).
- **CPU (J4105, 48 kHz stereo):** ~8 % of one core.

## Test results (`EnhDspTests`, synthetic scenes)

Footsteps over ambience with gunshots and voice, three random scenes: 96–100 % of steps detected,
0 gunshots flagged, ≤ 0.2 % of voice time flagged. Neutral settings ±0.04 dB; auto gain within ~1 dB
at full CLARITY; SUB +3.8 dB at 50 Hz, +8.4 dB with BOOST; stable at 44.1/48/96 kHz and block sizes 1–1024;
output peak ≤ 0.995. These are synthetic sounds — real game mixes will differ.

## UI performance

Continuous repainting locked to vsync, paced on the render thread (60 fps active / 30 fps idle).
The pointer is polled from X11 each frame on the render thread, so parallax stays smooth even when the
host delivers mouse events late. Config: `~/.config/ENHMaster/ui-config.json`.

## Dev hooks (no effect unless set)

- `PAD_UI_TEST_PARAMS="clarity=0.8;footstep=1"` – writes normalised values as host automation after 1.5 s
- `PAD_UI_TEST_SIZE=520x250` – initial editor size
- `PAD_UI_TEST_STATS=1` – frame-timing statistics on stderr every 5 s

## Backups

Git tags `backup/phase1-top-panel-full-controls`, `backup/front-panel-2-knobs`;
source tarballs and built bundles in `~/Projects/PvPAdaptiveDynamics-backups/`.
