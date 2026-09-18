#include "AdaptiveEQ.h"
#include <complex>
#include <vector>

namespace enh::dsp
{
    namespace
    {
        double magnitudeDb (const BiquadCoeffs& c, double sr, double hz)
        {
            const std::complex<double> z1 = std::polar (1.0, -2.0 * pi * hz / sr), z2 = z1 * z1;
            const auto num = (double) c.b0 + (double) c.b1 * z1 + (double) c.b2 * z2;
            const auto den = 1.0 + (double) c.a1 * z1 + (double) c.a2 * z2;
            return 20.0 * std::log10 (std::abs (num) / std::abs (den));
        }
    }

    void AdaptiveEQ::prepare (double sr, double controlRate, const BandAnalyzer& analyzer, int)
    {
        activeCount = analyzer.getActiveCount();

        for (int k = 0; k < numBands; ++k)
        {
            const auto i = (size_t) k;
            const double hz = BandAnalyzer::centreHz (k);
            active[i] = analyzer.isActive (k);
            designers[i].setup (sr, hz, filterQ);
            octave[i] = (float) std::log2 (hz / 1000.0);

            // 1 = free cuts, 0 = the footstep / voice / harmonic midrange where cuts are limited
            midGuard[i] = 1.0f - saturate01 ((float) std::min (std::log2 (hz / 180.0), std::log2 (7000.0 / hz)) / 0.5f);

            // The extreme bands often roll off naturally: fill holes there only half as much
            edgeLift[i] = hz < 60.0 || hz > 12000.0 ? 0.5f : 1.0f;

            midRegion[i] = active[i] && hz >= 250.0 && hz <= 2000.0;
            trebleRegion[i] = active[i] && hz >= 2500.0 && hz <= 10000.0;
            bassRegion[i] = active[i] && hz >= 40.0 && hz <= 160.0;
            trebleWeight[i] = saturate01 ((float) std::log2 (hz / 1500.0) / 1.5f);
            bassWeight[i] = saturate01 ((float) std::log2 (250.0 / hz) / 1.3f);
        }

        // Overlap matrix: response (dB per dB) at band centre i of filter j
        const int n = activeCount;
        std::vector<double> B ((size_t) (n * n));
        for (int j = 0; j < n; ++j)
        {
            const auto c = BiquadCoeffs::peaking (sr, BandAnalyzer::centreHz (j), filterQ, 6.0);
            for (int i = 0; i < n; ++i)
                B[(size_t) (i * n + j)] = magnitudeDb (c, sr, BandAnalyzer::centreHz (i)) / 6.0;
        }

        // solve = (B'B + lambda I)^-1 B'
        constexpr double lambda = 0.02;
        std::vector<double> A ((size_t) (n * n), 0.0), R ((size_t) (n * n), 0.0);
        for (int r = 0; r < n; ++r)
            for (int c = 0; c < n; ++c)
            {
                double s = r == c ? lambda : 0.0;
                for (int i = 0; i < n; ++i)
                    s += B[(size_t) (i * n + r)] * B[(size_t) (i * n + c)];
                A[(size_t) (r * n + c)] = s;
                R[(size_t) (r * n + c)] = B[(size_t) (c * n + r)];   // B'
            }

        for (int col = 0; col < n; ++col)   // Gauss-Jordan (A is symmetric positive definite)
        {
            const double d = A[(size_t) (col * n + col)];
            for (int c = 0; c < n; ++c)
            {
                A[(size_t) (col * n + c)] /= d;
                R[(size_t) (col * n + c)] /= d;
            }

            for (int r = 0; r < n; ++r)
            {
                if (r == col) continue;
                const double f = A[(size_t) (r * n + col)];
                for (int c = 0; c < n; ++c)
                {
                    A[(size_t) (r * n + c)] -= f * A[(size_t) (col * n + c)];
                    R[(size_t) (r * n + c)] -= f * R[(size_t) (col * n + c)];
                }
            }
        }

        for (auto& row : overlap) row.fill (0.0f);
        for (auto& row : solve) row.fill (0.0f);
        for (int r = 0; r < n; ++r)
            for (int c = 0; c < n; ++c)
            {
                overlap[(size_t) r][(size_t) c] = (float) B[(size_t) (r * n + c)];
                solve[(size_t) r][(size_t) c] = (float) R[(size_t) (r * n + c)];
            }

        for (int d = -5; d <= 5; ++d)
            localWeight[(size_t) (d + 5)] = (float) std::exp (-0.5 * (d / 2.5) * (d / 2.5));

        // Analysis delay (short follower attack) + half a control period
        lookahead = (float) (0.008 + 0.5 / controlRate);
        reset();
    }

    void AdaptiveEQ::reset()
    {
        for (auto& ch : states)
            for (auto& s : ch)
                s.reset();

        for (int k = 0; k < numBands; ++k)
        {
            controllers[(size_t) k].reset();
            coeffs[(size_t) k] = designers[(size_t) k].make (0.0f);
        }

        filterGain.fill (0.0f);
        response.fill (0.0f);
        curve.fill (0.0f);
        addCurve.fill (0.0f);
        analysisCountdown = 0;
        lastDesigned.fill (0.0f);
        activity = 0.0f;
    }

    void AdaptiveEQ::analyseSource (const BandAnalyzer& a, const FootstepDetector& steps, const Settings& s) noexcept
    {
        const int n = activeCount;
        const float clarity = s.normalize;
        const bool silent = a.fullShortDb < -80.0f;

        // --- what is actually in the source ----------------------------------------------------
        float loudest = -120.0f;
        for (int k = 0; k < n; ++k)
            loudest = std::max (loudest, a.ltasDb[(size_t) k]);

        std::array<float, numBands> presence {}, target {};
        for (int k = 0; k < n; ++k)
        {
            const auto i = (size_t) k;
            presence[i] = saturate01 ((a.ltasDb[i] - (loudest - 45.0f)) / 12.0f) * saturate01 ((a.ltasDb[i] + 95.0f) / 10.0f);
        }

        // Treble and bass balance, both measured against the midrange (which they never move).
        // Levels are per constant-Q band, so pink noise reads 0 dB for both.
        auto meanDb = [&] (const std::array<bool, numBands>& region)
        {
            double sum = 0.0; int count = 0;
            for (int k = 0; k < n; ++k)
                if (region[(size_t) k]) { sum += std::pow (10.0, a.ltasDb[(size_t) k] * 0.1); ++count; }
            return count > 0 ? (float) (10.0 * std::log10 (sum / count + 1.0e-20)) : -120.0f;
        };

        const float mid = meanDb (midRegion);
        trebleBalance = meanDb (trebleRegion) - mid;
        bassBalance = meanDb (bassRegion) - mid;

        // Normal programme spans a wide range; only a clearly muffled / harsh / thin / boomy source is rebalanced
        const float trebleExcess = trebleBalance - std::clamp (trebleBalance, -8.0f, 1.0f);
        const float bassExcess = bassBalance - std::clamp (bassBalance, -3.0f, 8.0f);

        for (int k = 0; k < n; ++k)
        {
            const auto i = (size_t) k;

            // Local-linear smoothing of the spectrum (follows natural roll-offs at the edges)
            double lw = 0, lx = 0, ly = 0, lxx = 0, lxy = 0;
            for (int j = std::max (0, k - 5); j <= std::min (n - 1, k + 5); ++j)
            {
                if (j == k) continue;
                const double w = localWeight[(size_t) (j - k + 5)] * (presence[(size_t) j] + 0.05);
                const double x = j - k, y = a.ltasDb[(size_t) j];
                lw += w; lx += w * x; ly += w * y; lxx += w * x * x; lxy += w * x * y;
            }
            const double ldet = lw * lxx - lx * lx;
            const float smooth = std::abs (ldet) > 1.0e-9 ? (float) ((ly * lxx - lx * lxy) / ldet) : (float) (ly / std::max (1.0e-9, lw));

            const float residual = a.ltasDb[i] - smooth;
            const float local = residual > 0.0f ? -0.85f * softRamp (residual - 1.0f, 2.0f)
                                                : 0.85f * softRamp (-residual - 1.0f, 2.0f) * presence[i] * edgeLift[i];

            // A balance lift only raises bands that actually carry content
            float tilt = -0.8f * (trebleExcess * trebleWeight[i] + bassExcess * bassWeight[i]);
            if (tilt > 0.0f)
                tilt *= presence[i];
            const float burst = -0.35f * softRamp (a.shortDb[i] - a.ltasDb[i] - 8.0f, 3.0f);

            target[i] = std::clamp (local, -6.0f, 6.0f) + std::clamp (tilt, -5.0f, 5.0f) + std::max (burst, -4.0f);
        }

        // Anchor the curve on the midrange (content-weighted), so corrections elsewhere never drag
        // the footstep / voice region down; auto gain in AnalogStage handles overall level
        float mean = 0.0f, weight = 0.0f;
        for (int k = 0; k < n; ++k)
            if (midRegion[(size_t) k])
            {
                mean += target[(size_t) k] * (presence[(size_t) k] + 0.02f);
                weight += presence[(size_t) k] + 0.02f;
            }
        mean /= std::max (1.0e-3f, weight);

        for (int k = 0; k < n; ++k)
        {
            const auto i = (size_t) k;
            float t = 1.6f * clarity * (target[i] - mean);   // NORM 30 / ADD 10 corrects 1.6x as hard as v4

            // Midrange guard: cuts limited to -3 dB (-1.5 dB in the core); none on the current footstep's bands
            if (t < 0.0f)
            {
                t = std::max (t, -3.0f * (0.5f + 0.5f * midGuard[i]) - 9.0f * midGuard[i] * midGuard[i]);
                if (s.footstepMode)
                    t *= 1.0f - saturate01 (steps.dynamicWeight[i] / 0.3f);
            }

            curve[i] = silent ? 0.0f : t;

            // ADD: lift where the planner found definition / body lacking, plus upward detail
            // (quiet decays and distant sounds), only in bands that carry content
            if (s.boost > 0.001f && ! silent)
            {
                const double hz = BandAnalyzer::centreHz (k);
                // Lift MORE where the harmonics are already rich (low need): the exciter then still has
                // strong material to generate from, instead of both backing off together
                const float definition = 10.0f * octaveBell (hz, s.clarity.hz, 1.1) * (0.55f + 0.45f * (1.0f - s.clarity.need));
                const float body = 7.0f * octaveBell (hz, s.depth.hz, 1.0) * (0.55f + 0.45f * (1.0f - s.depth.need));
                const float aboveFloor = saturate01 ((a.mediumDb[i] - a.floorDb[i] - 6.0f) / 6.0f);
                const float detail = std::min (4.0f, 0.5f * softRamp (a.mediumDb[i] - a.shortDb[i] - 2.0f, 4.0f)) * aboveFloor;
                addCurve[i] = std::min (12.0f, s.boost * (definition + body + detail) * presence[i]);
            }
            else
            {
                addCurve[i] = 0.0f;
            }
        }
    }

    void AdaptiveEQ::update (const BandAnalyzer& a, const FootstepDetector& steps, const Settings& s, float dt) noexcept
    {
        const int n = activeCount;
        const float footConfidence = s.footstepMode ? steps.getConfidence() : 0.0f;
        std::array<float, numBands> target {};

        // The source analysis follows slow spectra: ~375 Hz is plenty
        if (--analysisCountdown <= 0)
        {
            analysisCountdown = 4;
            analyseSource (a, steps, s);
        }

        // Light smoothing across frequency, then footstep priority on top
        for (int k = 0; k < n; ++k)
        {
            const auto i = (size_t) k;
            const float l = curve[(size_t) std::max (0, k - 1)], r = curve[(size_t) std::min (n - 1, k + 1)];
            target[i] = std::clamp ((std::clamp (0.15f * l + 0.7f * curve[i] + 0.15f * r, -12.0f, 12.0f) + addCurve[i]
                                     + footConfidence * (6.0f * steps.dynamicWeight[i] - 3.0f * steps.competitorWeight[i]))
                                    * std::clamp (s.strength, 0.0f, 5.0f), -24.0f, 24.0f);
        }

        // --- PD controllers ---------------------------------------------------------------------
        const float kpBase = 1.5f * std::pow (40.0f / 1.5f, s.speed);
        constexpr float kd = 0.35f;
        const float slopeSmoothing = 1.0f - std::exp (-dt / 0.02f);

        for (int k = 0; k < n; ++k)
        {
            const auto i = (size_t) k;
            const float sigma = std::min (a.sigmaDb[i], 12.0f);
            const float selfTune = std::clamp (0.6f + sigma / 8.0f, 0.6f, 2.2f);
            const auto& ctl = controllers[i];
            const float kp = kpBase * selfTune * (target[i] < ctl.value ? 2.0f : 1.0f);
            controllers[i].step (target[i], kp, kd, dt, lookahead, slopeSmoothing, 24.0f);
        }

        // --- solve filter gains so the summed response matches the curve ------------------------
        float absSum = 0.0f;
        for (int r = 0; r < n; ++r)
        {
            float g = 0.0f;
            for (int c = 0; c < n; ++c)
                g += solve[(size_t) r][(size_t) c] * controllers[(size_t) c].value;
            filterGain[(size_t) r] = std::clamp (g, -30.0f, 30.0f);
        }

        for (int r = 0; r < n; ++r)
        {
            float v = 0.0f;
            for (int c = 0; c < n; ++c)
                v += overlap[(size_t) r][(size_t) c] * filterGain[(size_t) c];
            response[(size_t) r] = v;
            absSum += std::abs (v);

            if (std::abs (filterGain[(size_t) r] - lastDesigned[(size_t) r]) > 0.01f)
            {
                coeffs[(size_t) r] = designers[(size_t) r].make (filterGain[(size_t) r]);
                lastDesigned[(size_t) r] = filterGain[(size_t) r];
            }
        }

        activity = saturate01 (absSum / (float) std::max (1, n) / 3.0f);
    }

    void AdaptiveEQ::process (float* const* channels, int numChannels, int start, int n) noexcept
    {
        const int chans = std::min (numChannels, maxChannels);

        if (chans == 2)
        {
            // Left and right side by side, band by band: the compiler pairs the two channels'
            // arithmetic (identical results to one channel after the other)
            float* __restrict l = channels[0] + start;
            float* __restrict r = channels[1] + start;
            for (int k = 0; k < activeCount; ++k)
            {
                const auto& co = coeffs[(size_t) k];
                auto& sl = states[0][(size_t) k];
                auto& sr2 = states[1][(size_t) k];
                float l1 = sl.z1, l2 = sl.z2, r1 = sr2.z1, r2 = sr2.z2;
                for (int i = 0; i < n; ++i)
                {
                    const float xl = l[i], xr = r[i];
                    const float yl = co.b0 * xl + l1, yr = co.b0 * xr + r1;
                    l1 = co.b1 * xl - co.a1 * yl + l2;  r1 = co.b1 * xr - co.a1 * yr + r2;
                    l2 = co.b2 * xl - co.a2 * yl;       r2 = co.b2 * xr - co.a2 * yr;
                    l[i] = yl; r[i] = yr;
                }
                sl.z1 = l1; sl.z2 = l2; sr2.z1 = r1; sr2.z2 = r2;
            }
            return;
        }

        for (int c = 0; c < chans; ++c)
        {
            auto* data = channels[c] + start;
            auto& st = states[(size_t) c];

            for (int k = 0; k < activeCount; ++k)
            {
                const auto& co = coeffs[(size_t) k];
                auto& s = st[(size_t) k];

                for (int i = 0; i < n; ++i)
                    data[i] = s.process (co, data[i]);
            }
        }
    }
}
