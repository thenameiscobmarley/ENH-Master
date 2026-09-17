#include "SubEnhancer.h"

namespace enh::dsp
{
    void SubEnhancer::prepare (double sampleRate, double)
    {
        sr = sampleRate;
        lowCoeffs = BiquadCoeffs::lowPass (sr, 100.0, 0.7071);        // LR4 = two Butterworth sections
        shelfCoeffs = BiquadCoeffs::lowShelf (sr, 85.0, 0.7, 0.0);
        punchCoeffs = BiquadCoeffs::peaking (sr, 55.0, 1.2, 0.0);
        subFollower.setup (sr, 0.020, 0.250);
        fullFollower.setup (sr, 0.020, 0.250);
        reset();
    }

    void SubEnhancer::reset()
    {
        lowState1.reset();
        lowState2.reset();
        subFollower.env = fullFollower.env = 0.0f;

        for (auto& c : channels)
            c = Channel {};

        lift.reset();
        harmonicMix = harmonicTarget = harmonicStep = 0.0f;
        lastPunchDb = lastShelfDb = 1000.0f;
        designedBassHz = 0.0f;
        harmonicLow = SvfCoeffs::make (sr, 110.0, 0.7071);
        harmonicBand = SvfCoeffs::make (sr, 170.0, 0.8);
        shelfCoeffs = BiquadCoeffs::lowShelf (sr, 85.0, 0.7, 0.0);
        punchCoeffs = BiquadCoeffs::peaking (sr, 55.0, 1.2, 0.0);
    }

    void SubEnhancer::update (const Settings& s, float dt) noexcept
    {
        const float subDb = subFollower.db();
        const float fullDb = fullFollower.db();

        const float ceiling = s.boost ? 11.0f : 8.0f;
        const float dominance = saturate01 ((subDb - fullDb + 6.0f) / 12.0f);   // 1 = sub already dominates
        const float present = saturate01 ((subDb + 70.0f) / 10.0f);             // no lift on silence

        const float strength = std::clamp (s.strength, 0.0f, 5.0f);
        const float target = std::min (24.0f, s.amount * ceiling * (1.0f - 0.35f * dominance) * present * strength);
        const float kp = 1.5f * std::pow (40.0f / 1.5f, s.speed) * (target < lift.value ? 2.0f : 1.0f);
        const float slopeSmoothing = 1.0f - std::exp (-dt / 0.02f);
        const float liftDb = lift.step (target, kp, 0.35f, dt, 0.02f, slopeSmoothing, 24.0f);

        if (std::abs (liftDb - lastShelfDb) > 0.02f)
        {
            shelfCoeffs = BiquadCoeffs::lowShelf (sr, 85.0, 0.7, liftDb);
            lastShelfDb = liftDb;
        }

        const float punchDb = s.boost ? std::min (12.0f, 4.5f * s.amount * strength) : 0.0f;
        if (std::abs (punchDb - lastPunchDb) > 0.02f)
        {
            punchCoeffs = BiquadCoeffs::peaking (sr, 55.0, 1.2, punchDb);
            lastPunchDb = punchDb;
        }

        // Harmonic split / band follow the bass fundamental that is actually playing
        if (std::abs (s.bassHz - designedBassHz) > 0.01f * s.bassHz)
        {
            harmonicLow = SvfCoeffs::make (sr, std::clamp (1.6 * s.bassHz, 70.0, 180.0), 0.7071);
            harmonicBand = SvfCoeffs::make (sr, std::clamp (2.6 * s.bassHz, 110.0, 330.0), 0.8);
            designedBassHz = s.bassHz;
        }

        harmonicTarget = std::min (3.0f, s.amount * (s.boost ? 0.9f : 0.55f) * present * strength);
        drive = s.boost ? 3.2f : 2.2f;
    }

    void SubEnhancer::process (float* const* data, int numChannels, int start, int n) noexcept
    {
        // Linear ramp of the harmonic mix across the segment
        harmonicStep = (harmonicTarget - harmonicMix) / (float) std::max (1, n);
        const float mixStart = harmonicMix;
        const int chans = std::min (numChannels, maxChannels);

        for (int c = 0; c < chans; ++c)
        {
            auto& ch = channels[(size_t) c];
            auto* x = data[c] + start;
            float mix = mixStart;

            for (int i = 0; i < n; ++i)
            {
                const float in = x[i];

                // Harmonics from the isolated sub band
                const float sub = ch.split2.process (harmonicLow, ch.split1.process (harmonicLow, in).low).low;
                // Pure distortion products (odd from the tanh residual, even from the square law);
                // the band-pass keeps the harmonic region and removes DC
                const float shaped = (std::tanh (sub * drive) / drive - sub) * 4.0f + 0.6f * drive * sub * sub;
                const float harmonics = ch.hBand2.process (harmonicBand, ch.hBand1.process (harmonicBand, shaped).band).band;

                float y = ch.shelf.process (shelfCoeffs, in);
                y = ch.punch.process (punchCoeffs, y);
                x[i] = y + harmonics * mix;
                mix += harmonicStep;
            }
        }

        harmonicMix = harmonicTarget;
    }
}
