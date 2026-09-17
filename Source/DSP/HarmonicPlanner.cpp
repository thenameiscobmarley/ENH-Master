#include "HarmonicPlanner.h"

namespace enh::dsp
{
    namespace
    {
        constexpr float bandsPerOctave = (numBands - 1) / 8.643856f;   // 40 Hz .. 16 kHz = log2 (400) octaves

        float levelAt (const std::array<float, numBands>& level, int count, float position) noexcept
        {
            const float p = std::clamp (position, 0.0f, (float) (count - 1));
            const int i = std::min ((int) p, count - 2);
            const float f = p - (float) i;
            return level[(size_t) i] + (level[(size_t) (i + 1)] - level[(size_t) i]) * f;
        }
    }

    void HarmonicPlanner::prepare (const BandAnalyzer& analyzer)
    {
        activeCount = analyzer.getActiveCount();

        auto range = [this] (double lo, double hi)
        {
            Range r { numBands, -1 };
            for (int k = 0; k < activeCount; ++k)
            {
                const double hz = BandAnalyzer::centreHz (k);
                if (hz >= lo && hz <= hi) { r.lo = std::min (r.lo, k); r.hi = std::max (r.hi, k); }
            }
            return r;
        };

        depthRange = range (70.0, 500.0);
        clarityRange = range (600.0, 5000.0);
        bassRange = range (35.0, 140.0);
        reset();
    }

    void HarmonicPlanner::reset()
    {
        depth = { 180.0f, 0.0f, 0.0f, 0.0f };
        clarity = { 2000.0f, 0.0f, 0.0f, 0.0f };
        bassHz = 60.0f;
        depthTrack = { std::log2 (depth.hz), 0.0f, 0.0f, false };
        clarityTrack = { std::log2 (clarity.hz), 0.0f, 0.0f, false };
        bassLog = std::log2 (bassHz);
    }

    void HarmonicPlanner::plan (const std::array<float, numBands>& level, float loudest, Range r, Band& band,
                                Tracker& track, float speed, float dt) noexcept
    {
        float& logHz = track.logHz;
        double sum = 0.0, weighted = 0.0;
        float best = 0.0f;

        for (int k = r.lo; k <= r.hi; ++k)
        {
            const float present = saturate01 ((level[(size_t) k] - (loudest - 30.0f)) / 30.0f);
            const float second = levelAt (level, activeCount, (float) k + bandsPerOctave);
            const float third = levelAt (level, activeCount, (float) k + 1.585f * bandsPerOctave);
            const float harmonicDb = 0.6f * second + 0.4f * third;

            // Natural spectra fall a few dB per octave; a larger gap means missing harmonics
            const float deficit = saturate01 ((level[(size_t) k] - harmonicDb - 4.0f) / 10.0f);
            const float score = present * (0.2f + 0.8f * deficit);

            const double w = std::pow ((double) score, 8.0);
            sum += w;
            weighted += w * std::log2 (BandAnalyzer::centreHz (k));
            best = std::max (best, score);
        }

        if (sum > 1.0e-9)
        {
            const float tau = 0.6f * std::pow (0.2f, speed);   // 0.6 s .. 0.12 s: follows the material closely
            logHz += ((float) (weighted / sum) - logHz) * (1.0f - std::exp (-dt / tau));
        }

        band.hz = std::pow (2.0f, logHz);
        band.need += (best - band.need) * (1.0f - std::exp (-dt / 0.08f));

        // Harmonic-to-source ratio at the current centre: rising ratio = harmonics thinning out
        const float centre = (logHz - (float) std::log2 (40.0)) * bandsPerOctave;
        const float source = levelAt (level, activeCount, centre);
        const float ratio = source - (0.6f * levelAt (level, activeCount, centre + bandsPerOctave)
                                      + 0.4f * levelAt (level, activeCount, centre + 1.585f * bandsPerOctave));
        if (! track.primed)
        {
            track.ratioFast = track.ratioSlow = ratio;
            track.primed = true;
        }
        track.ratioFast += (ratio - track.ratioFast) * (1.0f - std::exp (-dt / 0.12f));
        track.ratioSlow += (ratio - track.ratioSlow) * (1.0f - std::exp (-dt / 1.5f));

        const float rescue = saturate01 ((track.ratioFast - track.ratioSlow - 1.0f) / 4.0f);
        band.rescue = std::max (rescue, band.rescue * std::exp (-dt / 0.4f));
        band.amount = std::max (band.need, band.rescue);
    }

    void HarmonicPlanner::update (const BandAnalyzer& a, float speed, float dt) noexcept
    {
        if (activeCount < 2)
            return;

        // Blend of the long-term spectrum and the last ~0.5 s, so plans follow scene changes
        std::array<float, numBands> level {};
        float loudest = -120.0f;
        for (int k = 0; k < activeCount; ++k)
        {
            level[(size_t) k] = 0.25f * a.ltasDb[(size_t) k] + 0.75f * a.mediumDb[(size_t) k];
            loudest = std::max (loudest, level[(size_t) k]);
        }

        if (a.fullShortDb < -75.0f)
        {
            for (auto* b : { &depth, &clarity })
            {
                b->need *= std::exp (-dt / 0.2f);
                b->rescue *= std::exp (-dt / 0.2f);
                b->amount = std::max (b->need, b->rescue);
            }
            return;
        }

        if (depthRange.hi >= depthRange.lo)
            plan (level, loudest, depthRange, depth, depthTrack, speed, dt);
        if (clarityRange.hi >= clarityRange.lo)
            plan (level, loudest, clarityRange, clarity, clarityTrack, speed, dt);

        // Bass fundamental: power centroid of the lowest bands
        double p = 0.0, pw = 0.0;
        for (int k = bassRange.lo; k <= bassRange.hi; ++k)
        {
            const double e = std::pow (10.0, a.mediumDb[(size_t) k] * 0.1);
            p += e;
            pw += e * std::log2 (BandAnalyzer::centreHz (k));
        }
        if (p > 1.0e-10)
            bassLog += ((float) (pw / p) - bassLog) * (1.0f - std::exp (-dt / 0.3f));
        bassHz = std::pow (2.0f, bassLog);
    }
}
