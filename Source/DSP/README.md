# DSP backend — reserved (Phase 2+)

This folder is intentionally empty of code. **Phase 1 contains no DSP.**

When DSP is added it belongs here, owned by `PluginProcessor`:

- `processBlock` currently passes audio through untouched; the backend will be
  called from there.
- Parameters are read by ID from `Source/Parameters/ParameterSpecs.h`
  (use `AudioProcessorValueTreeState::getRawParameterValue` on the audio thread).
- Current parameters: `clarity`, `adaptSpeed`, `modeFootstep`. The earlier full set
  (per-target PD gains, masking mode, etc.) is preserved at git tag
  `backup/phase1-top-panel-full-controls` for when those features return.
- Any parameter written by future self-tuning must go through
  `ParameterBridge::setValueWithSource (index, value, ControlSource::selfTune)`
  so the UI animates it on the same path and tints it as self-tuned.
- DSP configuration files (hot reload + validation) go in `Source/Config`, next to
  the existing UI config loader, not in the audio thread.
