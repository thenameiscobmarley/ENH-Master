# Testing and tools

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

## Test everything at once

```sh
scripts/selftest.sh            # all of the below; PASS / FAIL per step, logs in build/lab/selftest/
scripts/selftest.sh --build    # build first
scripts/selftest.sh --quick    # skip the slow steps
```

## EnhDspTests — the DSP tests

`build/EnhDspTests_artefacts/Release/EnhDspTests` runs everything (about 10 minutes). One part at a
time: `--character`, `--mastering`, `--presets`, `--limiter`, `--bass`, `--units`, `--methods`,
`--zipper` (clicks), `--alias` (harshness), `--fuzz`, `--cpu`. See [Dev hooks](Dev%20hooks.md).

## EnhAudioLab — listen with your eyes

Plays sound through the real rack and writes audio, a report and pictures.

| Command | What it does |
|---|---|
| `check` | **the self-test**: every preset through every scene, 182 runs in ~3 min |
| `render --scene NAME --preset P --out DIR` | audio, report, spectrogram, spectrum, waveform |
| `contrib …` | what each unit does to each band |
| `ducks …` | who turns what down, over time |
| `trace …` | every unit's own gain, every 10 ms |
| `suite --out DIR` | the scenes through a set of presets, one line each |
| `scenes` | lists the test sounds |

`--set param=value` changes any setting; `--in file.wav` uses your own audio.

## `check`, the audio self-test

It fails when:

- **a rule breaks** — clipping, true peak over 0, a click, a broken number, DC, a surge when sound
  starts, silence that doesn't go silent, leaning to one side, mono turning un-mono, phase flips;
- **anything changes** from `Tests/lab-baseline.json` — loudness, dynamics, pumping, low end,
  distortion, stereo. After a change you *meant*, accept it with `EnhAudioLab check --update`.

`--only TEXT` runs just the matching runs.

## Scenes

game · music · drumsbass · bassline · bassduck · steps · gaps · explosion · quiet · voice · sweep ·
tones · impulses · pink · start · wide · antiphase · silence

Code: `Tools/AudioLab.cpp`, `Tests/EnhDspTests.cpp`, `scripts/selftest.sh`.
