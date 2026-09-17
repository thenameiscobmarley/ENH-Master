#include "AdaptiveEQ.h"

namespace enh::dsp
{
    void AdaptiveEQ::prepare (double sr, double controlRate, const BandAnalyzer& analyzer, int)
    {
        activeCount = 0;

        for (int k = 0; k < numBands; ++k)
        {
            const auto i = (size_t) k;
            const double hz = BandAnalyzer::centreHz (k);
            active[i] = analyzer.isActive (k);

            if (active[i])
                activeCount = k + 1;

            designers[i].setup (sr, hz, 1.6);
            coeffs[i] = designers[i].make (0.0f);

            // Where lifting buried content helps intelligibility most
            const float w = 0.45f
                          + 0.35f * octaveBell (hz, 110.0, 1.0)     // bass definition
                          - 0.25f * octaveBell (hz, 380.0, 0.8)     // leave low-mid mud alone
                          + 0.55f * octaveBell (hz, 2800.0, 1.4)    // vocal presence / detail
                          + 0.30f * octaveBell (hz, 10000.0, 1.0);  // air
            detailWeight[i] = std::clamp (w, 0.15f, 1.2f);
        }

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

        activity = 0.0f;
    }

    void AdaptiveEQ::update (const BandAnalyzer& a, const FootstepDetector& steps, const Settings& s, float dt) noexcept
    {
        const float clarity = s.clarity;
        const float footConfidence = s.footstepMode ? steps.getConfidence() : 0.0f;

        // ADAPT SPEED: proportional gain 1.5 .. 40 per second (log)
        const float kpBase = 1.5f * std::pow (40.0f / 1.5f, s.speed);
        constexpr float kd = 0.35f;
        const float slopeSmoothing = 1.0f - std::exp (-dt / 0.02f);

        const bool silent = a.fullShortDb < -80.0f;
        float absSum = 0.0f;

        for (int k = 0; k < activeCount; ++k)
        {
            const auto i = (size_t) k;

            // Spectral neighbourhood (±3 bands, triangular weights)
            float refSum = 0.0f, refW = 0.0f;
            for (int j = std::max (0, k - 3); j <= std::min (activeCount - 1, k + 3); ++j)
            {
                if (j == k) continue;
                const float w = (float) (4 - std::abs (j - k));
                refSum += a.mediumDb[(size_t) j] * w;
                refW += w;
            }

            const float reference = refW > 0.0f ? refSum / refW : a.mediumDb[i];
            const float sigma = std::min (a.sigmaDb[i], 12.0f);

            const float prominence = a.mediumDb[i] - reference;   // steady masking by this band
            const float burst = a.shortDb[i] - a.mediumDb[i];     // momentary burst in this band

            // Self-tuned threshold: busier bands get more tolerance before being cut
            const float cutThreshold = 3.0f + 0.4f * sigma;
            const float cut = 0.55f * softRamp (prominence - cutThreshold, 4.0f)
                            + 0.40f * softRamp (burst - 5.0f, 4.0f);

            const float aboveFloor = saturate01 ((a.mediumDb[i] - a.floorDb[i] - 6.0f) / 6.0f);
            const float boost = 0.50f * softRamp (-prominence - 2.5f, 4.0f) * aboveFloor * detailWeight[i];

            float target = silent ? 0.0f
                                  : std::clamp (clarity * (boost - cut), -10.0f * clarity, 8.0f * clarity);

            // Footstep priority: lift step regions, duck their usual maskers
            target += footConfidence * (6.0f * steps.stepWeight[i] - 2.5f * steps.competitorWeight[i]);

            // Cuts react faster than lifts (no pumping noise up between events);
            // bands with more movement get a faster controller.
            const auto& ctl = controllers[i];
            const float selfTune = std::clamp (0.6f + sigma / 8.0f, 0.6f, 2.2f);
            const float kp = kpBase * selfTune * (target < ctl.value ? 2.5f : 1.0f);

            const float gain = controllers[i].step (target, kp, kd, dt, lookahead, slopeSmoothing, 15.0f);
            coeffs[i] = designers[i].make (gain);
            absSum += std::abs (gain);
        }

        activity = saturate01 (absSum / (float) std::max (1, activeCount) / 4.0f);
    }

    void AdaptiveEQ::process (float* const* channels, int numChannels, int start, int n) noexcept
    {
        const int chans = std::min (numChannels, maxChannels);

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
