# PvP Adaptive Dynamics — Phase 1 (UI prototype, **no DSP**)

JUCE VST3 plugin shell with a real-time 3D hardware UI (OpenGL 3.2 core):
a pink rack unit with a pearl faceplate, sitting on a desk, controls on the front.
Audio is passed through untouched. Parameters are UI-facing placeholders.

## Build (Linux)

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # JUCE_PATH defaults to ~/JUCE
cmake --build build -j1                                   # -j1: JUCE needs a lot of RAM per job
```

The VST3 is copied to `~/.vst3/PvP Adaptive Dynamics.vst3`.
A Standalone app is also built (disable with `-DPAD_BUILD_STANDALONE=OFF`).

In Carla: *Add Plugin → Refresh (VST3) → PvP Adaptive Dynamics*, then *Show GUI*.

## Backups

- Git tag `backup/phase1-top-panel-full-controls` — previous top-panel design with the full control set
  (8 knobs, per-target Kp/Kd, masking + footstep switches).
- `~/Projects/PvPAdaptiveDynamics-backups/` — source tarball and built `.vst3` of that version.

## Layout

```
Source/
  PluginProcessor.*        pass-through processor, owns APVTS + ParameterBridge
  PluginEditor.*           thin editor hosting the 3D view (host-resizable)
  Parameters/
    ParameterSpecs.*       single table of every parameter (IDs, ranges, units)
    ParameterBridge.*      thread-safe values + change-source tracking (user/host/self-tune)
  Config/
    UIConfig.*             ~/.config/PvPAdaptiveDynamics/ui-config.json, validated + clamped
  DSP/
    README.md              reserved backend location (intentionally no code)
  UI/
    HardwareView.*         GL context, mouse picking, gestures, display text
    SharedUIState.h        atomics between message thread and GL thread
    Controls/ControlAnimation.h   knob spring (same path for all sources), switch snap
    Render/                math, VAO/shader/texture wrappers, per-material shaders
    Scene/                 layout, camera, procedural geometry, artwork, renderer + frame pacing
```

## Controls

| Control | Parameter | Notes |
|---|---|---|
| CLARITY knob | `clarity` | depth / detail / clarity amount |
| ADAPT SPEED knob | `adaptSpeed` | how fast the future auto-adjustment follows the material |
| FOOTSTEP toggle | `modeFootstep` | lights the vents + footprints lime |

Knobs: the numbered skirt turns with the knob; the value is the number under the fixed
indicator above it. Drag vertically (Shift/Ctrl = fine), wheel to nudge, double-click to reset.

Indicator colour shows who moved the knob: **gold = user**, **cyan = host automation**,
**violet = self-tune** (reserved; write via `ParameterBridge::setValueWithSource(..., ControlSource::selfTune)`).
All sources use the identical animation.

## Performance (J4105 / UHD 600)

- One shader program per material (no per-pixel branching), opaque geometry drawn front-to-back.
- Frame pacing on the render thread: continuous repainting locked to vsync — every refresh while
  the mouse is over the UI or something animates, every second refresh when idle (1 s hold).
- Measured in Carla at 1000×480 with 4× MSAA: 16.7 ms average frame interval, 0–6 late frames per 5 s,
  ~0.3 ms CPU per frame.

`ui-config.json` keys: `frameRate`, `idleFrameRate`, `msaaSamples` (0/2/4), `anisotropy`,
`panelTextureWidth` (1024/2048), `parallaxAmount` (0–2), `reduceMotion`.

## Dev test hooks (no effect unless set)

- `PAD_UI_TEST_PARAMS="clarity=0.8;modeFootstep=1"` — about 1.5 s after the editor opens, writes
  normalised values as *host automation*, to exercise the UI without a host.
- `PAD_UI_TEST_SIZE=520x250` — initial editor size.
- `PAD_UI_TEST_STATS=1` — prints frame-interval statistics to stderr every 5 s.

Example: `PAD_UI_TEST_STATS=1 carla-single vst3 ~/.vst3/"PvP Adaptive Dynamics.vst3"`
