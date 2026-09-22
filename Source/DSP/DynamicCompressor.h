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

        Stages (1.1.4.1): the sample path is split into three stages, each with swappable methods
        (MethodRegistry.h has the names and what each does to the sound):
          1. detector     what level the gain computer sees      PKR (default), RMS
          2. gain         threshold / ratio / knee -> target gain  ADT (the adaptive computer above)
          3. smoothing    target gain -> applied gain           DRL (default), SRL
        Only the active method runs. A switch crossfades for 30 ms (both run while it does; the new
        one starts from the gain being applied, so nothing jumps). Every method is zero-latency.

        Real-time safe: fixed state, no allocation, no locking.
    */
    class DynamicCompressor
    {
    public:
        struct Settings
        {
            float mix = 0.6f;        // 0..1
            float response = 0.5f;   // 0..1
            int detector = 0;        // Detector (MethodRegistry.h)
            int sideChain = 0;       // SideChain
            int gain = 0;            // Gain computer
            int smoothing = 0;       // Smoothing
            int makeup = 0;          // Make-up (0 auto 65 %, 1 full 90 %, 2 none)
            bool active = false;     // the plugin's parameter default is In; raw settings stay inert
        };

        enum Detector  { detectorPkr = 0, detectorRms, detectorKwt, numDetectors };
        enum SideChain { sideChain90 = 0, sideChain150, sideChainFull, numSideChains };
        enum Gain      { gainAdaptive = 0, gainSoft, gainHard, numGains };
        enum Smoothing { smoothingDrl = 0, smoothingSrl, smoothingOpto, numSmoothings };

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

        /** Tests: the single-follower ballistics this unit had before, for comparison (forces SRL). */
        void setDualRelease (bool on) noexcept { dualRelease = on; }

        /** The methods running now (after any crossfade has finished). */
        int getDetector() const noexcept  { return detector; }
        int getSmoothing() const noexcept { return smoothing; }
        int getSideChain() const noexcept { return sideChain; }
        int getGain() const noexcept      { return gain; }

    private:
        void updateDetector (float peak, float rms, const Settings&) noexcept;

        // The stages' methods
        float detectLevel (int method, float peak, float rms) const noexcept;
        float smooth (int method, float targetGainDb) noexcept;
        float gainFor (int method, float levelDb) const noexcept;
        void seedSmoothing (int method, float fromGainDb) noexcept;

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
        float slowDb = 0.0f, fastDb = 0.0f;   // DRL: average + transient gain reduction
        float singleDb = 0.0f;                // SRL: one follower
        float rmsWindowEnergy = 0.0f, rmsWindowCoeff = 0.0f;   // RMS: its own 50 ms power window
        float kwtEnergy = 0.0f;                               // KWT: the same, K-weighted
        BiquadCoeffs kwtShelf;
        BiquadState kwtState;
        float optoDb = 0.0f;                                  // OPT: its follower
        // Side-chain filters per method (H90 uses scHp / scState, as before); FUL passes straight through
        BiquadCoeffs scHp150;
        std::array<BiquadState, 2> scState150 {};
        int sideChain = sideChain90, fadingSideChain = -1, sideChainFadeLeft = 0;
        int gain = gainAdaptive, fadingGain = -1, gainFadeLeft = 0;

        // Method switching: the method that is fading out and how many samples of the fade are left
        int detector = detectorPkr, smoothing = smoothingDrl;
        int fadingDetector = -1, fadingSmoothing = -1;
        int detectorFadeLeft = 0, smoothingFadeLeft = 0, fadeLength = 1440;
        BiquadCoeffs scHp;                    // side-chain high-pass
        std::array<BiquadState, 2> scState {};
        float attackCoeff = 0.0f, releaseCoeff = 0.0f, slowAttackCoeff = 0.0f, slowReleaseCoeff = 0.0f, fastReleaseCoeff = 0.0f;
        bool dualRelease = true;
        float controlPhase = 0.0f;

        Readout readout {};
    };
}
