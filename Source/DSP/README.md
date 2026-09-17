# DSP (ENH Master)

Owned by `PluginProcessor` via `EnhEngine`. Everything here is real-time safe: all state is
allocated in `prepare()`, `process()` never allocates or locks. No JUCE UI dependencies, so the
offline test tool (`Tests/EnhDspTests.cpp`) builds against these files directly.

| File | Role |
|---|---|
| `DspMath.h` | biquads (RBJ), cheap peaking redesign, power followers, helpers |
| `PDController.h` | proportional-derivative gain follower with latency compensation |
| `BandAnalyzer.*` | 24 log bands, transient/short/medium followers, noise floor, rolling variance |
| `FootstepDetector.*` | onset + spectral-shape + rhythm footstep confidence |
| `AdaptiveEQ.*` | masking-aware per-band targets → PD controllers → 24 dynamic peaking filters |
| `SubEnhancer.*` | dynamic low shelf, 55 Hz punch (BOOST), band-limited bass harmonics |
| `AnalogStage.*` | colour EQ, auto gain, 2x oversampled exciter / saturation / ceiling |
| `EnhEngine.*` | chain + ~1.5 kHz control ticks + UI meters |
| `EngineMeters.h` | atomics published to the UI |

When changing the detector or leveler, run `EnhDspTests` (and `EnhDspTests --diagnose` for
per-step traces). Parameters written by future self-tuning should go through
`ParameterBridge::setValueWithSource (..., ControlSource::selfTune)`.
