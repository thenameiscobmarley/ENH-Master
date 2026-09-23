#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DspMath.h"
#include "HarmonicPlanner.h"

namespace enh::dsp
{
    /** Output stage, 2x oversampled around every non-linearity.

        Base rate : subsonic high-pass and loudness-matched auto gain (K-weighted in vs. out),
                    so enhancement is heard as detail rather than as level. No fixed tonal
                    shelves: tone is the adaptive EQ's job.
        2x rate   : two adaptive harmonic exciters (DEPTH and CLARITY) whose centres and amounts
                    come from HarmonicPlanner, transformer/valve colour, DC blocking and a soft
                    ceiling at about -0.2 dBFS.

        Exciter: the planned source band is isolated with a moving state-variable band-pass,
        normalised by its own envelope, and fed through Chebyshev polynomials (T2 = 2x^2-1 gives
        exactly the 2nd harmonic of a normalised partial, T3 = 4x^3-3x the 3rd). The result is
        scaled back to the band's level and high-passed above the source, so only new harmonics
        are added - at the same relative strength for quiet and loud sounds.
    */
    class AnalogStage
    {
    public:
        struct Settings
        {
            float boost = 0.0f;          // ADD mode amount (0 in NORM: nothing is added)
            float transient = 0.0f;      // 0..1, how "attacky" the programme is right now
            float footstep = 0.0f;       // detector confidence (only when the mode is on)
            HarmonicPlanner::Band depth, clarityBand;
            float strength = 1.0f;       // device STRENGTH (0..5)
            bool holdLevel = false;      // SPECTRAL LIMITER is handling a localised event: auto gain holds
            int harmonics = 0;           // HARMONICS method (MethodRegistry.h): 0 Chebyshev 2+3, 1 even, 2 odd
        };

        void prepare (double sampleRate, int maxBlockSize, int numChannels);
        void reset();
        int getLatencySamples() const noexcept;

        /** Loudness of the unprocessed input (call before any processing). */
        void measureInput (const float* const* channels, int numChannels, int numSamples) noexcept;

        void process (juce::dsp::AudioBlock<float> block, const Settings&) noexcept;

        float getAutoGainDb() const noexcept { return autoGainDb; }
        float getPeakDb() const noexcept     { return peakDb; }

    private:
        static constexpr int maxChannels = 2;

        struct KWeighting
        {
            BiquadState hp, shelf;
            float meanSquare = 0.0f;
        };

        struct BaseChannel
        {
            BiquadState subsonic;
        };

        struct Exciter
        {
            SvfState pre1, pre2, post1, post2, top;
            float env = 0.0f;
            float settled = 0.0f;   // the envelope, slowly: where it is below env, the partial has just begun
        };

        struct ExciterCoeffs
        {
            SvfCoeffs pre, post, top;
            float w2 = 0.6f, w3 = 0.4f;
            void design (double osr, float hz) noexcept;
        };

        struct OsChannel
        {
            Exciter depth, clarity;
            float dcX = 0.0f, dcY = 0.0f;
        };

        static inline float excite (Exciter&, const ExciterCoeffs&, float in, float attack, float release, float settle, float w2, float w3) noexcept;

        // HARMONICS: the exciters' 2nd / 3rd weights glide to the chosen method's over 30 ms. The output is
        // linear in the weights, so the glide is an exact crossfade between the two methods.
        struct Weights { float depthW2, depthW3, clarityW2, clarityW3; };
        static Weights weightsFor (int method) noexcept;
        Weights weightsFrom {}, weightsTo {};
        int harmonicsMethod = -1, glidePos = 0, glideLen = 1;

        double sr = 48000.0;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

        BiquadCoeffs kHp, kShelf, subsonic;
        ExciterCoeffs depthCoeffs, clarityCoeffs;
        float depthHz = 0.0f, clarityHz = 0.0f;
        float msCoeff = 0.0f, dcCoeff = 0.9995f, envAttack = 0.0f, envRelease = 0.0f, envSettle = 0.0f;
        float colourNow = -1.0f;   // the colour last block (-1: none yet), ramped from

        KWeighting inputK, outputK;
        std::array<BaseChannel, maxChannels> base {};
        std::array<OsChannel, maxChannels> os {};

        float autoGainDb = 0.0f, appliedGain = 1.0f;
        float inRefDb = -60.0f;   // the input's level when it is playing, for the gap hold
        float depthMix = 0.0f, clarityMix = 0.0f;
        float peakDb = -100.0f;
    };
}
