#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

namespace enh::dsp
{
    /** LUMEN - lifts quiet material toward a target level, per frequency band.

        Two controls: TARGET (dBFS) and RESPONSE. Everything else is measured.

        It works per band (low / mid / high, split with LR4 crossovers so the bands sum flat)
        because "quiet" is rarely true of a whole signal at once: footsteps and detail can be
        20 dB under a loud low end, and a broadband AGC would never find them.

        Per band it keeps:
          - a short-term loudness measure;
          - a slow estimate of that band's loud level, so "quiet" is relative to this material
            and this moment, not to a fixed number;
          - a noise-floor estimate, so hiss and room tone are never lifted.

        Gain is limited in size (maxGainDb), in speed (slew, dB/s) and by a hold after a loud
        passage, which is what stops pumping and breathing. Loud material is left alone: the
        gain curve is one-sided.

        While the SPECTRAL LIMITER after it is handling a localised abnormal event (a bass hit), the
        band gains hold still: its complementary crossover lets a big bass hit read as a louder
        midrange and top, and the lift on footsteps and detail used to be pulled back every time -
        the leveler "ducked". The spike is the limiter's to deal with.

        Real-time safe: fixed state, no allocation, no locking.
    */
    class SpectralLeveler
    {
    public:
        static constexpr int numBands = 3;

        struct Settings
        {
            float targetDb = -18.0f;
            float response = 0.5f;   // 0..1
            bool holdGains = false;  // SPECTRAL LIMITER is handling a localised spike: band gains stay put
            float levelDb = 0.0f;    // the rack's LEVEL: levels are read as if it were 0 dB, so nothing here fights it
            bool active = false;     // the plugin's parameter default is In; raw settings stay inert
        };

        struct Readout
        {
            std::array<float, numBands> gainDb {};        // applied per band
            std::array<float, numBands> levelDb { -100.0f, -100.0f, -100.0f };
            std::array<float, numBands> quietThreshDb {}; // where "quiet" currently sits
            float totalGainDb = 0.0f;
            float activity = 0.0f;                        // 0..1 how much lifting is happening
        };

        void prepare (double sampleRate, int numChannels);
        void reset();
        /** channels: pointers to the first sample of each channel in this chunk. */
        void process (float* const* channels, int numChannels, int numSamples, const Settings&) noexcept;

        const Readout& getReadout() const noexcept { return readout; }

    private:
        struct Crossover
        {
            // Linkwitz-Riley 4th order: two cascaded one-pole pairs per split
            std::array<float, 4> lpA {}, lpB {};
            void reset() noexcept { lpA.fill (0.0f); lpB.fill (0.0f); }
            float lowpass (float x, float k, bool second) noexcept;
        };

        struct BandState
        {
            float rms = 1.0e-6f;
            float loudDb = -30.0f;
            float floorDb = -70.0f;
            float gainDb = 0.0f;
            float holdS = 0.0f;
        };

        double sr = 48000.0;
        int channels = 2;
        float lowK = 0.0f, highK = 0.0f;    // crossover coefficients
        std::array<Crossover, 2> lowSplit {}, highSplit {};   // per channel
        std::array<BandState, numBands> bands {};
        float controlPhase = 0.0f;
        Readout readout {};

        void updateBands (const Settings&) noexcept;
    };
}
