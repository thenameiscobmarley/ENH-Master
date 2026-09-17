#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DspMath.h"

namespace enh::dsp
{
    /** Output stage with a hardware-style character, 2x oversampled around every non-linearity.

        Base rate : subsonic high-pass, transformer low bump, "air" shelf (tracks CLARITY),
                    loudness-matched auto gain (K-weighted in vs. out) so enhancement is heard
                    as detail rather than as level.
        2x rate   : transient-aware harmonic exciter (presence + bass definition bands),
                    transformer/valve colour (level-dependent 2nd/3rd harmonics),
                    DC blocking and a soft ceiling at about -0.2 dBFS.
    */
    class AnalogStage
    {
    public:
        struct Settings
        {
            float clarity = 0.5f;
            float transient = 0.0f;      // 0..1, how "attacky" the programme is right now
            float footstep = 0.0f;       // detector confidence (only when the mode is on)
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
            BiquadState subsonic, bump, air;
        };

        struct OsChannel
        {
            BiquadState hi1, hi2, hiPost, lm1, lmPost;
            float dcX = 0.0f, dcY = 0.0f;
        };

        double sr = 48000.0;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

        BiquadCoeffs kHp, kShelf, subsonic, bump, air, osHi, osHiPost, osLm, osLmPost;
        float airDb = -100.0f, msCoeff = 0.0f, dcCoeff = 0.9995f;

        KWeighting inputK, outputK;
        std::array<BaseChannel, maxChannels> base {};
        std::array<OsChannel, maxChannels> os {};

        float autoGainDb = 0.0f, appliedGain = 1.0f;
        float hiAmount = 0.0f, lmAmount = 0.0f;
        float peakDb = -100.0f;
    };
}
