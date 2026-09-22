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
| `compare A.wav B.wav --out DIR` | the same report and pictures for two files |
| `suite --out DIR` | every scene through a set of presets, and `summary.txt` with one line each |

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

Related: [[Dev hooks]], [[Methods]].
