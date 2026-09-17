#include "BandAnalyzer.h"

namespace enh::dsp
{
    void BandAnalyzer::prepare (double sr, double)
    {
        activeCount = 0;

        for (int k = 0; k < numBands; ++k)
        {
            const double hz = centreHz (k);
            active[(size_t) k] = hz < 0.42 * sr;

            if (active[(size_t) k])
                activeCount = k + 1;

            coeffs[(size_t) k] = BiquadCoeffs::bandPass (sr, hz, 2.5);
            transient[(size_t) k].setup (sr, 0.0008, 0.015);   // ~290 dB/s release
            shortTerm[(size_t) k].setup (sr, 0.008, 0.045);    // ~95 dB/s release
            medium[(size_t) k].setup (sr, 0.150, 0.450);
        }

        fullTransient.setup (sr, 0.0008, 0.015);
        fullShort.setup (sr, 0.010, 0.060);
        reset();
    }

    void BandAnalyzer::reset()
    {
        for (int k = 0; k < numBands; ++k)
        {
            state[(size_t) k].reset();
            transient[(size_t) k].env = shortTerm[(size_t) k].env = medium[(size_t) k].env = 0.0f;
            shortDb[(size_t) k] = mediumDb[(size_t) k] = meanDb[(size_t) k] = -120.0f;
            floorDb[(size_t) k] = -90.0f;
            varDb[(size_t) k] = 0.0f;
            sigmaDb[(size_t) k] = 0.0f;
        }

        fullTransient.env = fullShort.env = 0.0f;
        fullShortDb = fullTransientDb = programMaxDb = -120.0f;
    }

    void BandAnalyzer::update (float dt) noexcept
    {
        const float statsK = 1.0f - std::exp (-dt / 1.5f);     // ~1.5 s rolling window
        const float floorFall = 1.0f - std::exp (-dt / 0.4f);
        const float floorRise = 1.0f - std::exp (-dt / 6.0f);

        for (int k = 0; k < activeCount; ++k)
        {
            const auto i = (size_t) k;
            shortDb[i] = shortTerm[i].db();
            mediumDb[i] = medium[i].db();

            // Rolling mean / variance of the short-term level (self-tuning statistics)
            const float d = shortDb[i] - meanDb[i];
            meanDb[i] += d * statsK;
            varDb[i] += (d * d - varDb[i]) * statsK;
            sigmaDb[i] = std::sqrt (std::max (0.0f, varDb[i]));

            // Noise floor: falls quickly, rises slowly
            const float f = floorDb[i];
            floorDb[i] = f + (mediumDb[i] - f) * (mediumDb[i] < f ? floorFall : floorRise);
        }

        fullShortDb = fullShort.db();
        fullTransientDb = fullTransient.db();
        programMaxDb = std::max (fullShortDb, programMaxDb - 4.0f * dt);
    }
}
