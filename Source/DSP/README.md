# DSP (ENH Master)

Owned by `PluginProcessor` via `EnhEngine`. Everything here is real-time safe: all state is
allocated in `prepare()`, `process()` never allocates or locks. No JUCE UI dependencies, so the
offline test tool (`Tests/EnhDspTests.cpp`) builds against these files directly.

| File | Role |
|---|---|
| `DspMath.h` | biquads (RBJ), cheap peaking redesign, power followers, helpers |
| `PDController.h` | proportional-derivative gain follower with latency compensation |
| `BandAnalyzer.*` | 24 log bands, transient/short/medium followers, long-term spectrum, noise floor, rolling variance |
| `SpectralAnalyzer.*` | short-time FFT: tonality / persistent peaks, rising-energy tonality, flatness |
| `FootstepDetector.*` | event classifier: onset, decay, noisiness, clutter, context, rhythm + fingerprint |
| `HarmonicPlanner.*` | where harmonics help: DEPTH / CLARITY exciter centres + amounts, bass fundamental |
| `AdaptiveEQ.*` | source-derived curve (balance, resonances/holes, bursts) + footstep lift → PD → solved peaking cascade |
| `SubEnhancer.*` | dynamic low shelf, 55 Hz punch (BOOST), bass harmonics following the bass note |
| `AnalogStage.*` | auto gain, 2x oversampled adaptive exciters (Chebyshev, envelope-normalised) / colour / ceiling |
| `EnhEngine.*` | chain + ~1.5 kHz control ticks + UI meters |
| `EngineMeters.h` | atomics published to the UI |

When changing the detector or EQ, run `EnhDspTests` (`--events <scene>` lists every decision with
its reasons; `--analyze file.wav` does the same for a real capture). Parameters written by future self-tuning should go through
`ParameterBridge::setValueWithSource (..., ControlSource::selfTune)`.
