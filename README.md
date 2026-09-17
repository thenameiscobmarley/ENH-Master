# PvP Adaptive Dynamics — Phase 1 (UI prototype, **no DSP**)

JUCE VST3 plugin shell with a real-time 3D hardware UI (OpenGL 3.2 core).
Audio is passed through untouched. Every parameter is a UI-facing placeholder.

## Build (Linux)

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # JUCE_PATH defaults to ~/JUCE
cmake --build build -j1                                   # -j1 on low-RAM machines
```

The VST3 is copied to `~/.vst3/PvP Adaptive Dynamics.vst3`.
A Standalone app (`build/PvPAdaptiveDynamics_artefacts/Release/Standalone/`) is also
built for quick UI iteration; disable with `-DPAD_BUILD_STANDALONE=OFF`.

In Carla: *Add Plugin → Refresh (VST3) → PvP Adaptive Dynamics*, then *Show GUI*.

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
    HardwareView.*         GL context, mouse picking, gestures, frame pacing, scope text
    SharedUIState.h        atomics between message thread and GL thread
    Controls/ControlAnimation.h   knob spring (same path for all sources), switch snap
    Render/                math, VAO/shader/texture wrappers, uber-shader
    Scene/                 layout, camera, procedural geometry, panel artwork, renderer
```

## Controls

| Control | Parameter(s) |
|---|---|
| 8 main knobs | `adaptDepth response bandLeveling maskDucking exciterDrive transientFocus stepFocus outputGain` |
| Kp / Kd knobs | `pdKp_<target>` / `pdKd_<target>` for the **focused** target (click a main knob to focus it) |
| REACT COMP | `reactionComp` |
| MASKING / FOOTSTEP switches | `modeMasking` / `modeFootstep` |

Mouse: drag vertically (Shift/Ctrl = fine), wheel to nudge, double-click to reset.

Movement colours (LED ring + pointer): **gold = user**, **cyan = host automation**,
**violet = self-tune** (reserved; write via `ParameterBridge::setValueWithSource(..., ControlSource::selfTune)`).
All sources use the identical critically-damped animation.

## Performance notes (J4105 / UHD 600)

~3–4k triangles, ~60 draw calls, one uber-shader, no shadow maps or post-processing.
Textures: panel decal 2048×1024 RGBA (1024×512 via config), dial 512² R8, scope text 512×336 R8.
Rendering drops to `idleFrameRate` when nothing moves and the mouse is outside.

`ui-config.json` keys: `frameRate`, `idleFrameRate`, `msaaSamples` (0/2/4), `anisotropy`,
`panelTextureWidth` (1024/2048), `parallaxAmount` (0–2), `reduceMotion`.

Measured on the J4105 / UHD 600 inside Carla: ~9 % of one core while idle, ~97 MB RSS for the bridge process.

## Dev test hooks (no effect unless set)

- `PAD_UI_TEST_PARAMS="focus=6;modeMasking=1;stepFocus=0.9"` — about 1.5 s after the editor opens,
  writes normalised values as *host automation* (and sets PD focus), to exercise the UI without a host.
- `PAD_UI_TEST_SIZE=520x280` — initial editor size.

Example: `PAD_UI_TEST_PARAMS="modeFootstep=1" carla-single vst3 ~/.vst3/"PvP Adaptive Dynamics.vst3"`
