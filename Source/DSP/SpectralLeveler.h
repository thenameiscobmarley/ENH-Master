#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>

#include "DspMath.h"

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
        band gains hold still: the spike is the limiter's to deal with, not a reason to stop lifting.

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
            int lift = 0;            // methods (MethodRegistry.h): 0 standard, 1 gentle (0.6x), 2 big (1.3x)
            int gate = 0;            // 0 at -58 dBFS, 1 at -66, 2 at -50
            int balance = 0;         // 0 voiced, 1 mid focus, 2 flat
            bool active = false;     // the plugin's parameter default is In; raw settings stay inert
        };

        struct Readout
        {
            std::array<float, numBands> gainDb {};        // applied per band
            std::array<float, numBands> levelDb { -100.0f, -100.0f, -100.0f };
            std::array<float, numBands> quietThreshDb {}; // where "quiet" currently sits
            std::array<float, numBands> loudDb {}, floorDb {};   // what it takes for loud and for the floor
            std::array<float, numBands> gate {}, wantedDb {};    // how open the gates are, and the lift it wants
            float totalGainDb = 0.0f;
            float activity = 0.0f;                        // 0..1 how much lifting is happening
        };

        void prepare (double sampleRate, int numChannels);
        void reset();
        /** channels: pointers to the first sample of each channel in this chunk. */
        void process (float* const* channels, int numChannels, int numSamples, const Settings&) noexcept;

        const Readout& getReadout() const noexcept { return readout; }

    private:
        /** A real Linkwitz-Riley 4th-order split: two cascaded Butterworth sections for the low half and
            two more for the high half, both taken from the filter rather than by subtracting one from the
            other. Subtraction looked like a crossover and was not one: the sections shift the phase, so
            taking the low band away from the signal left almost all of the bass behind in the "rest".
            A 50 Hz note came out of the midrange band at -1 dB, and the leveler read every bass note as a
            loud midrange - it pulled its lift down on bass and took seconds to give it back. */
        struct Lr4
        {
            SvfState lp1, lp2, hp1, hp2;
            void reset() noexcept { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
            float low (const SvfCoeffs& c, float x) noexcept  { return lp2.process (c, lp1.process (c, x).low).low; }
            float high (const SvfCoeffs& c, float x) noexcept { return hp2.process (c, hp1.process (c, x).high).high; }
        };

        /** The 2nd-order allpass that an LR4 split adds: the band that skips a split goes through it so
            the three bands still sum flat. */
        struct Allpass
        {
            SvfState st;
            void reset() noexcept { st.reset(); }
            float process (const SvfCoeffs& c, float x) noexcept { return x - 2.0f * st.process (c, x).band; }
        };

        struct BandState
        {
            float rms = 1.0e-6f;
            float loudDb = -30.0f;
            float floorDb = -70.0f;
            float eventDb = -70.0f;   // the loudest this band has been lately
            float gainDb = 0.0f;
            float holdS = 0.0f;
        };

        double sr = 48000.0;
        int channels = 2;
        SvfCoeffs lowCoeffs {}, highCoeffs {};                // the two crossover frequencies
        std::array<Lr4, 2> lowSplit {}, highSplit {};         // per channel
        std::array<Allpass, 2> lowDelay {};                   // the low band through the second split's allpass
        std::array<BandState, numBands> bands {};
        float controlPhase = 0.0f;
        Readout readout {};

        void updateBands (const Settings&) noexcept;
    };
}
