# Dev hooks

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

For developers. None of these do anything unless you set them.

## EnhDspTests modes

| Mode | What it does |
|---|---|
| *(none)* | everything |
| `--events <scene>` | every footstep decision, with reasons |
| `--diagnose` / `--analyze file.wav` | the same for live audio / a recording |
| `--units` · `--presets` · `--bass` · `--limiter` | parts of the suite |
| `--character` · `--mastering` · `--methods` | CHARACTER, COMPARE / STEREO / safety, every glass-panel setting |
| `--zipper` · `--alias` · `--str0` · `--fuzz [s] [seed]` | clicks, harshness, STRENGTH 0, random knobs |
| `--methods-doc` | rewrites [Methods](Methods.md) |
| `--golden write\|check file` | bit-exact output check |
| `--cpu` | CPU benchmark (`CPU_BREAKDOWN=1` per stage) |

## UI settings (environment variables)

| Variable | Effect |
|---|---|
| `PAD_UI_TEST_SIZE=1000x740` | window size |
| `PAD_UI_TEST_DEMO=1` | animates the screens without audio |
| `PAD_UI_TEST_FOCUS=<n>` | start zoomed on a unit: 0 enhancer, 1 tone & space, 2 compressor, 3 leveler, 4 spectral limiter, 5 level, 6 balancer, 7 monitor, 8 deep sub, 9 character |
| `PAD_UI_TEST_PANEL=<unit>[,row[,choice]]` | open a glass panel |
| `PAD_UI_TEST_HOVER="x,y"` / `PAD_UI_TEST_HOVER_CONTROL=<id>` | show the loupe / outline a control |
| `PAD_UI_TEST_STATS=1` | frame timing every 5 s |
| `PAD_UI_TEST_PARAMS="clarity=0.8;footstep=1"` | set values after 1.5 s |
| `PAD_UI_DUMP_ARTWORK=<dir>` | write every panel's print and the layout audit (`clearances.txt`) |
| `ENH_MASTER_PRESETS=<file>` | use another preset file |

The full list of every variable, with a link to where it's read, is in the [Searchbar](../../Searchbar.md#dev-settings).

## Scripts

- `build/shot.sh out.png [ENV=…]` — screenshot the app.
- `build/stats.sh <label> [ENV=…]` — frame-time numbers.
- `build/hovertest.sh …` — drive a real mouse over the window.
