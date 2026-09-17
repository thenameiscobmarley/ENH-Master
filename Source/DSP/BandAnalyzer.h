#pragma once

#include "DspMath.h"
#include "EngineMeters.h"

namespace enh::dsp
{
    /** 24 overlapping log-spaced analysis bands (40 Hz .. 16 kHz, ~3/8 octave apart) on the
        mono input, with transient/short/medium power followers per band plus rolling
        statistics (noise floor, mean, deviation) used for self-tuning. */
    class BandAnalyzer
    {
    public:
        void prepare (double sampleRate, double controlRate);
        void reset();

        static double centreHz (int band) noexcept { return 40.0 * std::pow (400.0, band / (double) (numBands - 1)); }
        bool isActive (int band) const noexcept    { return active[(size_t) band]; }

        inline void push (float mono) noexcept
        {
            for (int k = 0; k < activeCount; ++k)
            {
                const float y = state[(size_t) k].process (coeffs[(size_t) k], mono);
                const float p = y * y;
                transient[(size_t) k].push (p);
                shortTerm[(size_t) k].push (p);
                medium[(size_t) k].push (p);
            }

            const float p = mono * mono;
            fullTransient.push (p);
            fullShort.push (p);
        }

        /** Control-rate update of dB values and rolling statistics.
            ltasSeconds: integration time of the long-term spectrum (follows ADAPT). */
        void update (float dt, float ltasSeconds = 2.5f) noexcept;

        // Per-band, valid after update()
        std::array<float, numBands> transientDb {}, shortDb {}, mediumDb {}, floorDb {}, sigmaDb {};

        /** Long-term average spectrum of the programme (power-averaged, frozen during silence). */
        std::array<float, numBands> ltasDb {};

        int getActiveCount() const noexcept { return activeCount; }

        // Raw followers for group measurements
        std::array<PowerFollower, numBands> transient {}, shortTerm {}, medium {};
        PowerFollower fullTransient, fullShort;

        float fullShortDb = -120.0f, fullTransientDb = -120.0f;
        float programMaxDb = -120.0f;   // decaying peak of the short-term program level

    private:
        std::array<BiquadCoeffs, numBands> coeffs {};
        std::array<BiquadState, numBands> state {};
        std::array<bool, numBands> active {};
        std::array<float, numBands> meanDb {}, varDb {}, ltasPower {};
        int activeCount = 0;
        float learnedSeconds = 0.0f;
    };
}
