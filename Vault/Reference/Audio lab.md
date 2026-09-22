# Audio lab

`Tools/AudioLab.cpp`, built as `EnhAudioLab` with the other tools (`ENH_BUILD_TOOLS`). It is how the
rack is *listened to* beyond pass/fail tests: audio through the real engine (with any preset, knob or
processing method), written out as audio, text and pictures. Back to [[00 Start Here]].

## Commands

| Command | What it writes |
|---|---|
| `scenes` | the built-in test signals |
| `render --scene NAME \| --in FILE.wav [--preset P] [--set id=value]... --out DIR` | `in.wav`, `out.wav`, `report.txt`, `spectrogram.png`, `spectrum.png`, `waveform.png`, `lowend.png` |
| `contrib ...` | `contrib.txt` / `contrib.png`: each unit's effect per band (the rack as set minus the rack with that unit out) |
| `ducks ...` | `ducks.txt` / `ducks.png`: each unit's effect on low / mid / high **over time** - who ducks what, when |
| `trace ...` | `trace.txt` / `trace.png`: every unit's **own gain**, read out of the engine every 10 ms, with how far each moved and when it moved most |
| `compare A.wav B.wav --out DIR` | the same report and pictures for two files |
| `suite --out DIR` | every scene through a set of presets, and `summary.txt` with one line each |

`trace` is the one to reach for when something moves and it is not clear what: `ducks` measures the
output with a unit taken out, `trace` reads the units' own decisions (the leveler's lift per band and
what it is reading, the six balancer faders, compressor gain reduction, the spectral limiter's cuts,
MATCH, the output limiter, the enhancer's auto gain, footstep confidence). Two scenes exist to be read
with it: **bassduck** (the detail and footsteps never change; loud bass only from 3 to 5 s) and **gaps**
(music, silence, music). Anything that moves in those is the rack reacting, not the material.

`--set` takes any parameter ID: knobs in their own units, switches 0 / 1, processing methods by index
(`--set tideDetector=1`). `--seconds` sets a scene's length. Run loops in **bash**, not zsh: zsh does
not split `$args` into words, so a `--set` in a variable is silently lost.

## The report

Sample peak, true peak (4x), RMS, BS.1770 integrated / max momentary / max short-term loudness, loudness
range, crest factor, side / mid width, correlation, DC, samples over 0 dBFS; third-octave bands in and
out with the change; the low end 10-200 Hz in seven bands; the gain the rack applied per band over time
(mean, spread, 5th-95th percentile: pumping); new clicks (sample-to-sample discontinuities the input did
not have, allowing for the rack's gain); on `tones`, THD at 1 kHz / 100 Hz / 40 Hz, the strongest
non-harmonic component (aliasing) and the 60 Hz + 7 kHz IMD sidebands.

## What it found (1.3.4.1)

See the README's *What listening found and fixed*: PROTECT and AIR / BODY clicks, the exciter's crack on
attacks, true-peak overs, the MIX BALANCER taking footsteps back, and MATCH pulling the mix down after big
hits. Each was found here, traced with `ducks` and bisection (`--set unitActive=0`), fixed, and checked
again here.

Then *Why the mix ducked under bass*, found with `trace` on **bassduck** and **gaps**: the leveler's
crossover was a subtraction rather than a split (a 50 Hz note read 1 dB down in the midrange band, so
every bass note pulled the lift on detail down), the compressor learnt the silence in a pause and
clamped the music by 20 dB when it came back, and MATCH and the enhancer's auto gain kept integrating
through gaps. The sign to watch for: in `trace`, a unit that moves while what it is reading does not.

Related: [[Dev hooks]], [[Methods]].
