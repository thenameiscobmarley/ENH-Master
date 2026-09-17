# DSP backend — reserved (Phase 2+)

This folder is intentionally empty of code. **Phase 1 contains no DSP.**

When DSP is added it belongs here, owned by `PluginProcessor`:

- `processBlock` currently passes audio through untouched; the backend will be
  called from there.
- Parameters are read by ID from `Source/Parameters/ParameterSpecs.h`
  (use `AudioProcessorValueTreeState::getRawParameterValue` on the audio thread).
- Per-target PD gains already exist as parameters: `pdKp_<targetId>` / `pdKd_<targetId>`.
- Any parameter written by future self-tuning must go through
  `ParameterBridge::setValueWithSource (index, value, ControlSource::selfTune)`
  so the UI animates it on the same path and tints it as self-tuned.
- DSP configuration files (hot reload + validation) go in `Source/Config`, next to
  the existing UI config loader, not in the audio thread.
