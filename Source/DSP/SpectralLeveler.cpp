#include "SpectralLeveler.h"

#include <cmath>
#include <algorithm>

namespace enh::dsp
{
    static inline float levelToDb (float x) noexcept { return 20.0f * std::log10 (std::max (1.0e-7f, x)); }
    static inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

    float SpectralLeveler::Crossover::lowpass (float x, float k, bool second) noexcept
    {
        auto& st = second ? lpB : lpA;
        // two cascaded one-poles = 2nd order Butterworth-ish; called twice for LR4
        st[0] += k * (x - st[0]);
        st[1] += k * (st[0] - st[1]);
        return st[1];
    }

    void SpectralLeveler::prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        channels = std::max (1, numChannels);

        // 200 Hz and 2.2 kHz splits: low weight, midrange detail (where footsteps live), air
        const auto coeff = [this] (float hz) { return 1.0f - std::exp (-2.0f * 3.14159265f * hz / (float) sr); };
        lowK = coeff (200.0f);
        highK = coeff (2200.0f);
        reset();
    }

    void SpectralLeveler::reset()
    {
        for (auto& c : lowSplit) c.reset();
        for (auto& c : highSplit) c.reset();
        for (auto& b : bands) b = {};
        controlPhase = 0.0f;
        readout = {};
    }

    void SpectralLeveler::updateBands (const Settings& s) noexcept
    {
        const float dt = 1.0f / 1500.0f;
        const float resp = std::clamp (s.response, 0.0f, 1.0f);

        // Per-band target: the low band is deliberately allowed less lift than the midrange,
        // because lifting quiet bass is what makes a mix sound woolly.
        constexpr std::array<float, numBands> bandOffset { -4.0f, 0.0f, -1.5f };
        constexpr std::array<float, numBands> maxGain { 9.0f, 18.0f, 14.0f };

        float sum = 0.0f, active = 0.0f;

        for (int b = 0; b < numBands; ++b)
        {
            auto& st = bands[(size_t) b];
            const float levelDb = levelToDb (st.rms);

            // What counts as loud in this band, right now: quick to rise, slow to fall.
            const float up = 1.0f - std::exp (-dt / (0.10f + 0.20f * (1.0f - resp)));
            const float down = 1.0f - std::exp (-dt / (2.5f - 1.2f * resp));
            st.loudDb += (levelDb > st.loudDb ? up : down) * (levelDb - st.loudDb);

            // Noise floor: very slow to rise, quick to fall, so it settles on the quiet moments
            // of the material rather than creeping up to meet a steady signal.
            const float fUp = 1.0f - std::exp (-dt / 25.0f);
            const float fDown = 1.0f - std::exp (-dt / 0.3f);
            st.floorDb += (levelDb > st.floorDb ? fUp : fDown) * (levelDb - st.floorDb);

            // What counts as quiet here: under the target, but above the floor and moving.
            const float quietDb = std::min (st.loudDb - 6.0f, s.targetDb + bandOffset[(size_t) b]);
            const float targetDb = s.targetDb + bandOffset[(size_t) b];

            // Two gates, both measured rather than set:
            //  - absolute: hiss and silence are never worth lifting;
            //  - modulation: a band that never moves is room tone, not programme material.
            const float absGate = std::clamp ((levelDb + 58.0f) / 8.0f, 0.0f, 1.0f);
            const float modGate = std::clamp ((st.loudDb - st.floorDb - 1.5f) / 3.5f, 0.0f, 1.0f);
            const float gate = absGate * (0.3f + 0.7f * modGate);

            // One-sided: lift what is below the target, leave what is above it alone. A band that
            // already has loud passages of its own only gets a fraction of the lift - its quiet
            // moments are part of the music, not a fault to be corrected.
            float wanted = 0.0f;
            if (levelDb < targetDb)
            {
                const float deficit = targetDb - levelDb;
                const float ownHeadroom = std::max (0.0f, st.loudDb - targetDb);
                const float relative = std::clamp (1.0f - ownHeadroom / 12.0f, 0.25f, 1.0f);
                wanted = std::min (deficit * relative * gate, maxGain[(size_t) b]);
            }

            // Hold after loud material: do not start lifting the moment a loud passage ends.
            if (levelDb > targetDb)
                st.holdS = 0.25f + 0.35f * (1.0f - resp);
            else
                st.holdS = std::max (0.0f, st.holdS - dt);

            if (st.holdS > 0.0f)
                wanted = std::min (wanted, st.gainDb);

            // Slew limit in dB per second: this is what keeps it from pumping.
            const float slewUp = (2.5f + 9.0f * resp) * dt;
            const float slewDown = (5.0f + 16.0f * resp) * dt;
            const float delta = wanted - st.gainDb;
            st.gainDb += std::clamp (delta, -slewDown, slewUp);
            st.gainDb = std::clamp (st.gainDb, 0.0f, maxGain[(size_t) b]);

            readout.gainDb[(size_t) b] = st.gainDb;
            readout.levelDb[(size_t) b] = levelDb;
            readout.quietThreshDb[(size_t) b] = quietDb;
            sum += st.gainDb;
            active = std::max (active, st.gainDb / maxGain[(size_t) b]);
        }

        readout.totalGainDb = sum / (float) numBands;
        readout.activity = active;
    }

    void SpectralLeveler::process (float* const* data, int numChannels, int numSamples, const Settings& s) noexcept
    {
        const int n = numSamples;
        const int ch = std::min (channels, numChannels);
        if (n <= 0 || ch <= 0)
            return;

        if (! s.active)
        {
            for (auto& b : bands)
                b.gainDb += (0.0f - b.gainDb) * 0.05f;
            readout.totalGainDb = 0.0f;
            readout.activity = 0.0f;
            return;
        }

        const float controlStep = (float) (1500.0 / sr);

        for (int i = 0; i < n; ++i)
        {
            std::array<float, numBands> summed { 0.0f, 0.0f, 0.0f };
            std::array<std::array<float, numBands>, 2> split {};

            for (int c = 0; c < ch && c < 2; ++c)
            {
                const float x = data[c][i];

                // LR4 splits: lowpass twice for the band edge, the remainder carries the rest
                const float low = lowSplit[(size_t) c].lowpass (lowSplit[(size_t) c].lowpass (x, lowK, false), lowK, true);
                const float rest = x - low;
                const float mid = highSplit[(size_t) c].lowpass (highSplit[(size_t) c].lowpass (rest, highK, false), highK, true);
                const float high = rest - mid;

                split[(size_t) c] = { low, mid, high };
                for (int b = 0; b < numBands; ++b)
                    summed[(size_t) b] += split[(size_t) c][(size_t) b];
            }

            for (int b = 0; b < numBands; ++b)
            {
                const float v = summed[(size_t) b] / (float) std::min (ch, 2);
                auto& st = bands[(size_t) b];
                st.rms += (std::abs (v) - st.rms) * 0.002f;
            }

            controlPhase += controlStep;
            if (controlPhase >= 1.0f)
            {
                controlPhase -= 1.0f;
                updateBands (s);
            }

            for (int c = 0; c < ch && c < 2; ++c)
            {
                float y = 0.0f;
                for (int b = 0; b < numBands; ++b)
                    y += split[(size_t) c][(size_t) b] * dbToGain (bands[(size_t) b].gainDb);

                data[c][i] = y;
            }

            // Mono or >2 channels: follow channel 0's treatment
            for (int c = 2; c < ch; ++c)
                data[c][i] = data[0][i];
        }
    }
}
