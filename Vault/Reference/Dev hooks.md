# Dev hooks

## Offline DSP tests

```sh
build/EnhDspTests_artefacts/Release/EnhDspTests            # everything + CPU benchmark
build/EnhDspTests_artefacts/Release/EnhDspTests --events crates [seed]
build/EnhDspTests_artefacts/Release/EnhDspTests --diagnose
build/EnhDspTests_artefacts/Release/EnhDspTests --analyze file.wav
```

- **no arguments** - the full suite: detection accuracy against synthetic scenes (`quiet`, `game`,
  `varied`, `crates`), EQ behaviour, harmonic generation, MULTIPLY/STRENGTH, stability at
  44.1/48/96 kHz and at block sizes 1, 7, 33, 480, 1024. Ends with `ALL PASSED (0 failures)`.
- **`--events <scene> [seed]`** - every accepted and rejected event with its reasons, against ground
  truth ([[Footstep detection]]).
- **`--diagnose`** - the same for live audio, plus the EQ curve and harmonic centres every 5 s.
- **`--analyze file.wav`** - run a recording through and report.

## UI environment variables

| Variable | Effect |
|---|---|
| `PAD_UI_TEST_PARAMS="clarity=0.8;footstep=1"` | writes normalised values as host automation after 1.5 s |
| `PAD_UI_TEST_SIZE=520x250` | initial editor size |
| `PAD_UI_TEST_STATS=1` | frame timing on stderr every 5 s, and logs when rendering pauses/resumes |
| `PAD_UI_TEST_MINIMISE="7,17"` | minimise at 7 s, restore at 17 s |
| `PAD_UI_TEST_HOVER="x,y"` | shows the hover loupe at that point (anchored, for reproducible screenshots) |
| `PAD_UI_TEST_DEMO=1` | animates TONE & SPACE's live display without audio |

## Screenshots

`build/shot.sh out.png [ENV=…]` launches the standalone, grabs the window and re-encodes it.
`build/hovertest.sh name:x,y[,down|,up] …` drives a real pointer over the window (window-relative
logical pixels) and grabs a shot per step - that is how hover, drag-lock and the loupe were verified.

Related: [[Rendering and performance]], [[The loupe]].
