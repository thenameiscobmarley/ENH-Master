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
            // All bands at once (vectorised banks; the same arithmetic as band by band)
            alignas (16) float y[numBands], p[numBands];
            filters.process (mono, y, activeCount);
            for (int k = 0; k < activeCount; ++k)
                p[k] = y[k] * y[k];
            transient.push (p, activeCount);
            shortTerm.push (p, activeCount);
            medium.push (p, activeCount);

            const float full = mono * mono;
            fullTransient.push (full);
            fullShort.push (full);
        }

        /** Control-rate update of dB values and rolling statistics.
            ltasSeconds: integration time of the long-term spectrum (follows ADAPT). */
        void update (float dt, float ltasSeconds = 2.5f) noexcept;

        // Per-band, valid after update()
        std::array<float, numBands> transientDb {}, shortDb {}, mediumDb {}, floorDb {}, sigmaDb {};

        /** Long-term average spectrum of the programme (power-averaged, frozen during silence). */
        std::array<float, numBands> ltasDb {};

        int getActiveCount() const noexcept { return activeCount; }

        // Raw followers for group measurements (power: .env[k])
        FollowerBank<numBands> transient {}, shortTerm {}, medium {};
        PowerFollower fullTransient, fullShort;

        float fullShortDb = -120.0f, fullTransientDb = -120.0f;
        float programMaxDb = -120.0f;   // decaying peak of the short-term program level

    private:
        BiquadBank<numBands> filters {};
        std::array<bool, numBands> active {};
        std::array<float, numBands> meanDb {}, varDb {}, ltasPower {};
        int activeCount = 0;
        float learnedSeconds = 0.0f;
    };
}
