#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "BandAnalyzer.h"
#include "FootstepDetector.h"
#include "AdaptiveEQ.h"
#include "SubEnhancer.h"
#include "AnalogStage.h"
#include "EngineMeters.h"

namespace enh::dsp
{
    /** ENH Master signal chain.

        input ─► analysis (24 bands, footstep detector, sub follower)       [feed-forward]
          │
          └─► adaptive spectral EQ ─► footstep focus EQ ─► sub enhancer ─► analog stage ─► out
                                                                          (colour, auto gain,
                                                                           2x exciter/saturation/ceiling)

        Control updates run at ~1.5 kHz; audio runs through IIR filters only, so the added
        latency is just the oversampling filters (reported to the host).
        Real-time safe: no allocation or locking in process().
    */
    class EnhEngine
    {
    public:
        struct Parameters
        {
            float clarity = 0.5f;     // 0..1
            float adaptSpeed = 0.4f;  // 0..1
            float sub = 0.0f;         // 0..1
            bool subBoost = false;
            bool footstep = false;
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

    private:
        void controlTick (const Parameters&) noexcept;
        void processChunk (juce::AudioBuffer<float>&, int start, int n, const Parameters&) noexcept;

        BandAnalyzer analyzer;
        FootstepDetector steps;
        AdaptiveEQ eq;
        SubEnhancer sub;
        AnalogStage analog;
        EngineMeters meters;

        // Footstep focus EQ (static regions, faded in with the mode)
        struct FocusChannel { BiquadState thump, scuff; };
        std::array<FocusChannel, 2> focus {};
        BiquadCoeffs thumpCoeffs, scuffCoeffs;
        float focusAmount = -1.0f;

        double sampleRate = 48000.0;
        int maxBlock = 512;
        int controlInterval = 32, samplesToTick = 32;
        float controlDt = 1.0f / 1500.0f;

        float clarity = 0.5f, speed = 0.4f, subAmount = 0.0f, focusTarget = 0.0f, transient = 0.0f;
    };
}
