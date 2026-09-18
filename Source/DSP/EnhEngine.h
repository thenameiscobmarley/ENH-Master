#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "BandAnalyzer.h"
#include "SpectralAnalyzer.h"
#include "HarmonicPlanner.h"
#include "FootstepDetector.h"
#include "AdaptiveEQ.h"
#include "SubEnhancer.h"
#include "AnalogStage.h"
#include "Seraph.h"
#include "DynamicCompressor.h"
#include "SpectralLeveler.h"
#include "SpectralLimiter.h"
#include "SpectrumScope.h"
#include "FinalLimiter.h"
#include "LoudnessMeter.h"
#include "MixBalancer.h"
#include "EngineMeters.h"

namespace enh::dsp
{
    /** ENH Master signal chain.

        LEVEL (the rack's working level: everything after it hears it, the leveler's target moves with it)
        input ─► analysis: 24 bands + long-term spectrum, FFT tonality,      [feed-forward]
          │                 footstep classifier, harmonic planner, sub follower
          │
          └─► source-dependent EQ (+ footstep lift) ─► sub enhancer ─► analog stage ─► UPWARD LEVELER
                                                                       (auto gain, 2x adaptive
                                                                        depth/clarity exciters,
                                                                        colour, ceiling)
              ─► SPECTRAL LIMITER ─► MIX BALANCER ─► ADAPTIVE COMPRESSOR ─► TONE & SPACE ─► output limiter ─► out
                                                                                             (loudness meter)
                 (abnormal spectral   (broadband, keyed on   (tone & texture,
                  excess, from the     what is left)          space & width,
                  shared analysis)                            loudness hold)

        Control updates run at ~1.5 kHz; audio runs through IIR filters only, so the added
        latency is just the oversampling filters (reported to the host).
        Real-time safe: no allocation or locking in process().
    */
    class EnhEngine
    {
    public:
        struct Parameters
        {
            float normalize = 0.5f;   // 0..1 CLARITY correction strength (both modes)
            float boost = 0.0f;       // 0..1 CLARITY enhancement (ADD mode only)
            float adaptSpeed = 0.4f;  // 0..1
            float sub = 0.0f;         // 0..1
            bool subBoost = false;
            bool footstep = false;
            float strength = 1.0f;    // ENH STRENGTH: 0 = no effect .. 5 = five times the effect
            SpectralLeveler::Settings lumen {};   // UPWARD LEVELER: lifts quiet material
            SpectralLimiter::Settings limiter {}; // SPECTRAL LIMITER: cuts abnormal spectral excess
            DynamicCompressor::Settings tide {};  // ADAPTIVE COMPRESSOR: adaptive-threshold compressor
            Seraph::Settings seraph {};
            float levelDb = 0.0f;                 // LEVEL: the rack's working level (-24 .. +12 dB), first in the chain
            MixBalancer::Settings balancer {};    // MIX BALANCER: rides six band faders for the balance
        };

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset();
        void process (juce::AudioBuffer<float>&, const Parameters&) noexcept;

        int getLatencySamples() const noexcept { return analog.getLatencySamples() + output.getLatencySamples(); }

        /** The loudness meter's RESET (any thread): integrated loudness and true-peak hold start again. */
        void resetLoudness() noexcept { loudnessResetPending.store (true, std::memory_order_relaxed); }
        const MixBalancer& getBalancer() const noexcept { return balancer; }
        const LoudnessMeter& getLoudness() const noexcept { return loudness; }
        const ScopeFifo& getBalancerInputScope() const noexcept { return scopeBalIn; }
        const ScopeFifo& getBalancerOutputScope() const noexcept { return scopeBalOut; }
        const FinalLimiter& getOutputLimiter() const noexcept { return output; }
        const EngineMeters& getMeters() const noexcept { return meters; }

        /** For tests: detector events since reset. */
        int getFootstepEventCount() const noexcept { return steps.getEventCount(); }
        float getFootstepConfidence() const noexcept { return steps.getConfidence(); }
        const FootstepDetector::Trace& getFootstepTrace() const noexcept { return steps.trace; }
        const HarmonicPlanner& getHarmonicPlan() const noexcept { return planner; }
        const AdaptiveEQ& getEQ() const noexcept { return eq; }
        const FootstepDetector& getFootstepDetector() const noexcept { return steps; }
        void setFootstepAdaptive (bool on) noexcept { steps.setAdaptive (on); }   // tests
        const Seraph& getSeraph() const noexcept { return seraph; }
        const DynamicCompressor& getCompressor() const noexcept { return tide; }
        const SpectralLeveler& getLeveler() const noexcept { return lumen; }
        const SpectralLimiter& getLimiter() const noexcept { return limiter; }

        /** Analyser taps: the audio thread only copies samples in, the editor does the FFT. */
        const ScopeFifo& getInputScope() const noexcept { return scopeIn; }
        const ScopeFifo& getOutputScope() const noexcept { return scopeOut; }

    private:
        void controlTick (const Parameters&) noexcept;
        void processChunk (juce::AudioBuffer<float>&, int start, int n, const Parameters&) noexcept;

        BandAnalyzer analyzer;
        SpectralAnalyzer spectrum;
        FootstepDetector steps;
        HarmonicPlanner planner;
        AdaptiveEQ eq;
        SubEnhancer sub;
        AnalogStage analog;
        SpectralLeveler lumen;
        SpectralLimiter limiter;
        DynamicCompressor tide;
        Seraph seraph;
        ScopeFifo scopeIn, scopeOut, scopeBalIn, scopeBalOut;
        MixBalancer balancer;
        LoudnessMeter loudness;
        std::atomic<bool> loudnessResetPending { false };
        float levelGain = 1.0f, levelDbNow = 0.0f;
        EngineMeters meters;

        /** The one output limiter: lookahead, holds through a bass cycle, never wobbles inside one. */
        FinalLimiter output;

        double sampleRate = 48000.0;
        int maxBlock = 512;
        int controlInterval = 32, samplesToTick = 32, planCountdown = 0;
        float controlDt = 1.0f / 1500.0f;

        float strength = 1.0f, normalize = 0.5f, boost = 0.0f, speed = 0.4f, subAmount = 0.0f, transient = 0.0f;
    };
}
