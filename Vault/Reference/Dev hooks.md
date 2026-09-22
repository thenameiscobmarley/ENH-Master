# Dev hooks

## Offline DSP tests

```sh
build/EnhDspTests_artefacts/Release/EnhDspTests            # everything + CPU benchmark
build/EnhDspTests_artefacts/Release/EnhDspTests --events crates [seed]
build/EnhDspTests_artefacts/Release/EnhDspTests --diagnose
build/EnhDspTests_artefacts/Release/EnhDspTests --analyze file.wav
build/EnhDspTests_artefacts/Release/EnhDspTests --units | --presets | --bass | --limiter | --cpu
build/EnhDspTests_artefacts/Release/EnhDspTests --golden write|check file
```

- **no arguments** - the full suite: detection accuracy against synthetic scenes (`quiet`, `game`,
  `varied`, `crates`), EQ behaviour, harmonic generation, MULTIPLY/STRENGTH, stability at
  44.1/48/96 kHz and at block sizes 1, 7, 33, 480, 1024. Ends with `ALL PASSED (0 failures)`.
- **`--events <scene> [seed]`** - every accepted and rejected event with its reasons, against ground
  truth ([[Footstep detection]]).
- **`--diagnose`** - the same for live audio, plus the EQ curve and harmonic centres every 5 s.
- **`--analyze file.wav`** - run a recording through and report.
- **`--units`** - LEVEL, loudness (EBU cases), output limiter, MIX BALANCER (incl. loudness keeper).
- **`--presets`** - every preset in the preset file through the engine; **`--bass`** THD and pumping;
  **`--limiter`** the anti-duck scenes; **`--cpu`** the benchmark.
- **`--golden write|check`** - bit-exact output check, for optimisations that must not change a sample.

## UI environment variables

| Variable | Effect |
|---|---|
| `PAD_UI_TEST_PARAMS="clarity=0.8;footstep=1"` | writes normalised values as host automation after 1.5 s |
| `PAD_UI_TEST_SIZE=520x250` | initial editor size |
| `PAD_UI_TEST_STATS=1` | frame timing on stderr every 5 s, and logs when rendering pauses/resumes |
| `PAD_UI_TEST_MINIMISE="7,17"` | minimise at 7 s, restore at 17 s |
| `PAD_UI_TEST_HOVER="x,y"` | shows the hover loupe at that point (anchored, for reproducible screenshots) |
| `PAD_UI_TEST_DEMO=1` | animates the live displays and meters without audio |
| `PAD_UI_TEST_FOCUS=<unit>` | starts walked up to a unit: 0 enhancer, 1 tone & space, 2 compressor, 3 leveler, 4 limiter, 5 level control, 6 mix balancer, 7 output monitor |
| `PAD_UI_TEST_MAX_DETAIL=<0-3>` | caps the geometry detail level |
| `PAD_UI_DUMP_ARTWORK=<dir>` | writes every printed panel and `clearances.txt` (print overlapping hardware) |
| `ENH_MASTER_PRESETS=<file>` | use this preset file instead of `~/.config/ENH Master/presets.json` |

## Screenshots

`build/shot.sh out.png [ENV=…]` launches the standalone, grabs the window and re-encodes it.
`build/hovertest.sh name:x,y[,down|,up] …` drives a real pointer over the window (window-relative
logical pixels) and grabs a shot per step - that is how hover, drag-lock and the loupe were verified.

Related: [[Rendering and performance]], [[The loupe]].
