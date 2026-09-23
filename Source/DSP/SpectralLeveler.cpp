#include "SpectralLeveler.h"

#include <cmath>
#include <algorithm>

namespace enh::dsp
{
    static inline float levelToDb (float x) noexcept { return 20.0f * std::log10 (std::max (1.0e-7f, x)); }

    void SpectralLeveler::prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        channels = std::max (1, numChannels);

        // 200 Hz and 2.2 kHz splits: low weight, midrange detail (where footsteps live), air.
        // Butterworth sections (Q = 1/sqrt 2); two of them cascaded make each LR4 half.
        lowCoeffs = SvfCoeffs::make (sr, 200.0, 0.70710678);
        highCoeffs = SvfCoeffs::make (sr, 2200.0, 0.70710678);
        slowCoeff = (float) (1.0 - std::exp (-1.0 / (0.120 * sr)));
        reset();
    }

    void SpectralLeveler::reset()
    {
        for (auto& c : lowSplit) c.reset();
        for (auto& c : highSplit) c.reset();
        for (auto& c : lowDelay) c.reset();
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
        // BAND BALANCE, LIFT and GATE methods (the defaults are the leveler's own numbers)
        constexpr std::array<float, numBands> voiced { -4.0f, 0.0f, -1.5f }, midFocus { -7.0f, 0.0f, -3.0f }, flat { 0.0f, 0.0f, 0.0f };
        const auto& bandOffset = s.balance == 1 ? midFocus : s.balance == 2 ? flat : voiced;
        // These are what a band may be lifted by at most. They were set when the "crossover" left most of
        // the low end in every band, so a lift was always partly a lift of the bass and never reached its
        // limit. With a real split the bands are clean and the same numbers lift far harder - a bass note's
        // own harmonics were being brought up with them - so they come down to match.
        constexpr std::array<float, numBands> standardLift { 6.0f, 12.0f, 9.0f }, gentleLift { 3.6f, 7.2f, 5.4f }, bigLift { 7.8f, 15.6f, 11.7f };
        const auto& maxGain = s.lift == 1 ? gentleLift : s.lift == 2 ? bigLift : standardLift;
        const float liftScale = s.lift == 1 ? 0.6f : s.lift == 2 ? 1.3f : 1.0f;
        const float gateDb = s.gate == 1 ? 66.0f : s.gate == 2 ? 50.0f : 58.0f;

        float sum = 0.0f, active = 0.0f;

        for (int b = 0; b < numBands; ++b)
        {
            auto& st = bands[(size_t) b];
            const float levelDb = levelToDb (st.rms) - s.levelDb;   // as if LEVEL were 0 dB

            // Loud and floor are followed on a 120 ms level. The 10 ms one jumps by several dB on anything
            // noise-like (rain, wind, an engine, a dense mix): loud caught its peaks, the floor its dips, and
            // the gap between them read as movement - steady sound was lifted as if it were programme.
            // A footstep still moves the 120 ms level by most of its height.
            const float slowDb = levelToDb (st.slow) - s.levelDb;

            // What counts as loud in this band, right now: quick to rise, slow to fall.
            const float up = 1.0f - std::exp (-dt / (0.10f + 0.20f * (1.0f - resp)));
            const float down = 1.0f - std::exp (-dt / (2.5f - 1.2f * resp));
            st.loudDb += (slowDb > st.loudDb ? up : down) * (slowDb - st.loudDb);

            // Noise floor: slow to rise, quick to fall, so it settles on the quiet moments of the material.
            // It rises over seconds rather than half a minute: a band that holds still (hiss, the harmonics
            // that ride on a steady tone) closes its own modulation gate instead of being lifted for ever.
            // Coming out of silence (the start, a pause, the gap between two songs) there is no floor
            // yet: for the first 0.8 s it follows the level up quickly (80 ms), and no new lift starts
            // until it has been found. Starting from -70 dB instead, anything steady read as "moving"
            // for ten seconds and was lifted by the full amount (pink noise +12 dB, the compressor
            // pulling it back down), then slowly let go: a surge and a drift every time sound started.
            if (levelDb + gateDb < 0.0f)
                st.onsetS = onsetTime;
            else if (st.onsetS > 0.0f)
                st.onsetS = std::max (0.0f, st.onsetS - dt);
            const float fUp = 1.0f - std::exp (-dt / (st.onsetS > 0.0f ? 0.08f : 4.0f));
            const float fDown = 1.0f - std::exp (-dt / 0.3f);
            st.floorDb += (slowDb > st.floorDb ? fUp : fDown) * (slowDb - st.floorDb);

            // What counts as quiet here: under the target, but above the floor and moving.
            const float quietDb = std::min (st.loudDb - 6.0f, s.targetDb + bandOffset[(size_t) b]);
            const float targetDb = s.targetDb + bandOffset[(size_t) b];

            // Two gates, both measured rather than set:
            //  - absolute: hiss and silence are never worth lifting;
            //  - modulation: a band that never moves is room tone, not programme material.
            const float absGate = std::clamp ((levelDb + gateDb) / 8.0f, 0.0f, 1.0f);
            const float modGate = std::clamp ((st.loudDb - st.floorDb - 1.5f) / 3.5f, 0.0f, 1.0f);
            const float gate = absGate * (0.3f + 0.7f * modGate);

            // The loudest this band has been lately: up at once, forgotten over ten seconds.
            // On the 120 ms level, as loud is: against the 10 ms one, the first moments of any hit looked
            // far above loud and the lift backed off for them.
            st.eventDb = std::max (slowDb, st.eventDb + (slowDb - st.eventDb) * (1.0f - std::exp (-dt / 10.0f)));

            // One-sided: lift what is below the target, leave what is above it alone. A band that
            // already has loud passages of its own only gets a fraction of the lift - its quiet
            // moments are part of the music, not a fault to be corrected. How big a fraction depends on
            // how far the band's loud moments sit above where it is now: material that is simply quiet
            // is lifted in full, while an ambience that an explosion lands on top of is not raised to
            // the target - that only flattens the explosion and drives it into the limiter.
            float wanted = 0.0f;
            if (levelDb < targetDb)
            {
                const float deficit = targetDb - levelDb;
                const float range = std::max (0.0f, st.eventDb - st.loudDb);
                const float relative = std::clamp (1.0f - (range - 10.0f) / 15.0f, 0.25f, 1.0f);
                // LIFT scales how much of the way it goes, not only how far it may: with the ceiling alone,
                // BIG did nothing wherever the standard lift did not reach its ceiling. Never past the target.
                wanted = std::min ({ deficit * relative * gate * liftScale, deficit, maxGain[(size_t) b] });
            }

            // Hold after loud material: do not start lifting the moment a loud passage ends.
            if (levelDb > targetDb)
                st.holdS = 0.25f + 0.35f * (1.0f - resp);
            else
                st.holdS = std::max (0.0f, st.holdS - dt);

            if (st.holdS > 0.0f || st.onsetS > 0.0f)
                wanted = std::min (wanted, st.gainDb);

            // Slew limit in dB per second: this is what keeps it from pumping.
            const float slewUp = (2.5f + 9.0f * resp) * dt;
            const float slewDown = (5.0f + 16.0f * resp) * dt;
            const float delta = wanted - st.gainDb;
            if (! s.holdGains)
            {
                st.gainDb += std::clamp (delta, -slewDown, slewUp);
                // The lift of the last moments (let go at 4 dB/s): what the hold goes back to
                st.recentDb = std::max (st.gainDb, st.recentDb - 4.0f * dt);
            }
            else
            {
                // The limiter has a spike: the lift is what it was just before it. The spike's first
                // milliseconds, before the limiter had it, may already have pulled the lift down - the
                // hold used to keep that dip for the whole event; now the lift comes back to where it was.
                st.gainDb += std::clamp (st.recentDb - st.gainDb, 0.0f, slewUp);
            }
            st.gainDb = std::clamp (st.gainDb, 0.0f, maxGain[(size_t) b]);

            readout.gainDb[(size_t) b] = st.gainDb;
            readout.levelDb[(size_t) b] = levelDb;
            readout.quietThreshDb[(size_t) b] = quietDb;
            readout.loudDb[(size_t) b] = st.loudDb;
            readout.floorDb[(size_t) b] = st.floorDb;
            readout.gate[(size_t) b] = gate;
            readout.wantedDb[(size_t) b] = wanted;
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
            // Each band's level: the average of the channels' own levels, not the level of their sum.
            // The sum reads wide and uncorrelated material (ambience, reverb, a wide synth) 3 dB under
            // what it is, and anything out of phase as silence, and lifted it for that.
            std::array<float, numBands> summed { 0.0f, 0.0f, 0.0f };
            std::array<std::array<float, numBands>, 2> split {};

            for (int c = 0; c < ch && c < 2; ++c)
            {
                const float x = data[c][i];

                // LR4 splits, both halves taken from the filter. The low band goes through the second
                // split's allpass so that low + mid + high is still flat.
                const float lowRaw = lowSplit[(size_t) c].low (lowCoeffs, x);
                const float rest = lowSplit[(size_t) c].high (lowCoeffs, x);
                const float low = lowDelay[(size_t) c].process (highCoeffs, lowRaw);
                const float mid = highSplit[(size_t) c].low (highCoeffs, rest);
                const float high = highSplit[(size_t) c].high (highCoeffs, rest);

                split[(size_t) c] = { low, mid, high };
                for (int b = 0; b < numBands; ++b)
                    summed[(size_t) b] += std::abs (split[(size_t) c][(size_t) b]);
            }

            for (int b = 0; b < numBands; ++b)
            {
                const float v = summed[(size_t) b] / (float) std::min (ch, 2);
                auto& st = bands[(size_t) b];
                st.rms += (v - st.rms) * 0.002f;
                st.slow += (v - st.slow) * slowCoeff;
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
