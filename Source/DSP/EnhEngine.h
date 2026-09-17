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
#include "EngineMeters.h"

namespace enh::dsp
{
    /** ENH Master signal chain.

        input ─► analysis: 24 bands + long-term spectrum, FFT tonality,      [feed-forward]
          │                 footstep classifier, harmonic planner, sub follower
          │
          └─► source-dependent EQ (+ footstep lift) ─► sub enhancer ─► analog stage ─► SERAPH ─► out
                                                                       (auto gain, 2x adaptive   (SILK tone &
                                                                        depth/clarity exciters,   texture, HALO
                                                                        colour, ceiling)          space & width)

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
            Seraph::Settings seraph {};
        };

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset();
        void process (juce::AudioBuffer<float>&, const Parameters&) noexcept;

        int getLatencySamples() const noexcept { return analog.getLatencySamples(); }
        const EngineMeters& getMeters() const noexcept { return meters; }

        /** For tests: detector events since reset. */
        int getFootstepEventCount() const noexcept { return steps.getEventCount(); }
        float getFootstepConfidence() const noexcept { return steps.getConfidence(); }
        const FootstepDetector::Trace& getFootstepTrace() const noexcept { return steps.trace; }
        const HarmonicPlanner& getHarmonicPlan() const noexcept { return planner; }
        const AdaptiveEQ& getEQ() const noexcept { return eq; }
        const FootstepDetector& getFootstepDetector() const noexcept { return steps; }
        const Seraph& getSeraph() const noexcept { return seraph; }

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
        Seraph seraph;
        EngineMeters meters;

        double sampleRate = 48000.0;
        int maxBlock = 512;
        int controlInterval = 32, samplesToTick = 32, planCountdown = 0;
        float controlDt = 1.0f / 1500.0f;

        float strength = 1.0f, normalize = 0.5f, boost = 0.0f, speed = 0.4f, subAmount = 0.0f, transient = 0.0f;
    };
}
