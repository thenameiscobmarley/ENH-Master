# Source/DSP — the sound

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Everything that touches audio. It's real-time safe: memory is set up in `prepare()`, and `process()`
never allocates or waits. No UI code here, so the tests build against it directly.

| File | What it is |
|---|---|
| `EnhEngine.*` | the whole chain, in order, plus meters and safety |
| `ParameterMapping.h` | knob values → settings for each stage |
| `MethodRegistry.h` | every glass-panel setting and its text |
| `BandAnalyzer.*`, `SpectralAnalyzer.*` | measuring the sound |
| `FootstepDetector.*` | recognising footsteps |
| `HarmonicPlanner.*`, `AdaptiveEQ.*`, `SubEnhancer.*`, `AnalogStage.*` | the ADAPTIVE ENHANCER |
| `SpectralLeveler.*` | UPWARD LEVELER |
| `DeepSub.h` | DEEP SUB |
| `SpectralLimiter.*` | SPECTRAL LIMITER |
| `MixBalancer.h` | MIX BALANCER |
| `DynamicCompressor.*` | ADAPTIVE COMPRESSOR |
| `Seraph.*` | TONE & SPACE |
| `Character.h` | CHARACTER |
| `LoudnessTarget.h`, `LoudnessMeter.h`, `FinalLimiter.h` | loudness, metering, the output limiter |
| `DspMath.h`, `PDController.h` | filters and helpers |

After changing anything here, run `scripts/selftest.sh`. Find any function: [Searchbar](../../Searchbar.md).
