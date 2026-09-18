#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DspMath.h"

namespace enh::dsp
{
    /** TIDE - a compressor with two controls and no threshold knob.

        The threshold, ratio, knee and release all follow the programme. What the detector
        measures, every control tick:

          - loudness now (fast RMS) and over the last few seconds (slow RMS), K-weighted-ish
            so the measure matches what is actually loud to a listener;
          - crest factor (peak over RMS): peaky material gets a higher threshold and a softer
            ratio, so transients survive; dense material gets a lower one;
          - spectral tilt (low versus high energy): bass-heavy material is compressed a little
            less aggressively, which is what stops it from pumping the whole mix;
          - transient density: how often onsets arrive, which sets how fast release may be;
          - a running estimate of the loud part of the programme (a cheap percentile tracker),
            so the threshold sits under the music rather than under one static number.

        The detector listens through a 90 Hz side-chain high-pass (12 dB/oct), as engineers set a bus
        compressor's SC filter: steady bass no longer drives the gain reduction that pulls the
        whole mix down and lets it swell back when the bass stops.

        Dual release (program-dependent, as on the classic bus compressors): a slow follower carries
        the average gain reduction the programme needs; a fast one takes only the momentary extra a
        transient asks for, and gives it back in ~50 ms. A kick then dips the mix for a moment
        instead of pulling the sustained parts down and letting them swell back (pumping).

        RESPONSE (0..1) scales how quickly all of that adapts and how hard the ballistics are.
        MIX is a straight wet/dry blend, applied after auto make-up so the blend does not
        change the level.

        Real-time safe: fixed state, no allocation, no locking.
    */
    class DynamicCompressor
    {
    public:
        struct Settings
        {
            float mix = 0.6f;        // 0..1
            float response = 0.5f;   // 0..1
            bool active = false;     // the plugin's parameter default is In; raw settings stay inert
        };

        struct Readout
        {
            float gainReductionDb = 0.0f;   // current GR (>= 0)
            float thresholdDb = -20.0f;     // where the adaptive threshold sits now
            float ratio = 2.0f;
            float inputDb = -100.0f, outputDb = -100.0f;
            float crest = 1.0f;             // measured crest factor
            float adaptivity = 0.0f;        // 0..1 how much the threshold is moving
        };

        void prepare (double sampleRate, int numChannels);
        void reset();
        /** channels: pointers to the first sample of each channel in this chunk. key: what the detector
            listens to instead of the audio (same layout), or nullptr to key on the audio itself. */
        void process (float* const* channels, int numChannels, int numSamples, const Settings&,
                      const float* const* key = nullptr) noexcept;

        const Readout& getReadout() const noexcept { return readout; }

        /** Tests: the single-follower ballistics this unit had before, for comparison. */
        void setDualRelease (bool on) noexcept { dualRelease = on; }

    private:
        void updateDetector (float peak, float rms, const Settings&) noexcept;

        double sr = 48000.0;
        int channels = 2;

        // detector state
        float fastRms = 1.0e-6f, slowRms = 1.0e-6f, peakEnv = 1.0e-6f;
        float loudEstimateDb = -18.0f;      // percentile-style estimate of "the loud part"
        float tilt = 0.0f;                  // -1 dark .. +1 bright
        float lowEnergy = 0.0f, highEnergy = 0.0f;
        float lowState = 0.0f, highState = 0.0f;
        float onsetRate = 0.0f, lastFlux = 0.0f;
        float thresholdDb = -20.0f, ratio = 2.0f, kneeDb = 6.0f;
        float gainDb = 0.0f, makeupDb = 0.0f;
        float slowDb = 0.0f, fastDb = 0.0f;   // dual release: average + transient gain reduction
        BiquadCoeffs scHp;                    // side-chain high-pass
        std::array<BiquadState, 2> scState {};
        float attackCoeff = 0.0f, releaseCoeff = 0.0f, slowAttackCoeff = 0.0f, slowReleaseCoeff = 0.0f, fastReleaseCoeff = 0.0f;
        bool dualRelease = true;
        float controlPhase = 0.0f;

        Readout readout {};
    };
}
