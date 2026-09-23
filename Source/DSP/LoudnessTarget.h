#pragma once

#include <array>
#include <cmath>
#include "DspMath.h"
#include "LoudnessMeter.h"

namespace enh::dsp
{
    /** LOUDNESS TARGET (OUTPUT MONITOR's panel): keeps what leaves the rack at one loudness, whatever
        comes in and whatever the units did to it. A game, a song and a voice call come out equally loud.

        It measures its own input (K-weighted, BS.1770, like the meter) on a 3 s window and sets a gain of
        target - level: open loop, so it never chases itself. What it learnt from the mix ducking under
        bass and MATCH coming back from silence several decibels down:
          - it holds still through a burst (0.4 s level 6 dB or more over the 3 s level): an explosion
            is not a reason to turn the game down for the next ten seconds,
          - it holds still in a gap (0.4 s level 20 dB or more under what the material usually is, or
            under -60 LUFS), so a pause never becomes the new normal and nothing is pumped up out of
            the noise floor,
          - it moves slowly (6 s), except for the first seconds of material after a start, when it
            learns quickly (0.5 s, for 2 s, on a level corrected for its window still filling) so a new
            session does not spend ten seconds at the wrong level,
          - no more than 12 dB either way, and it never lifts material quieter than -50 LUFS.
        The output limiter after it looks after full scale. No latency; switching glides.

        Method 0 is OFF (what the rack always did). */
    class LoudnessTarget
    {
    public:
        static constexpr std::array<float, 4> targets { 0.0f, -23.0f, -18.0f, -14.0f };   // index 0 = off

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            kWeighting (sr, preCoeffs, rlbCoeffs);
            fastK = onePole (0.4, sr);
            slowK = onePole (3.0, sr);
            reset();
        }

        void reset()
        {
            for (auto& s : pre) s.reset();
            for (auto& s : rlb) s.reset();
            fastMs = slowMs = 0.0;
            usualDb = -70.0f;
            gainDb = 0.0f;
            gain = 1.0f;
            learntS = firstS = 0.0f;
        }

        float getGainDb() const noexcept { return gainDb; }

        void process (float* const* ch, int numChannels, int n, int method) noexcept
        {
            const int chans = std::min (2, numChannels);
            if (chans <= 0 || n <= 0)
                return;

            const bool on = method > 0 && method < (int) targets.size();

            // Measure (always, so switching on starts from a known level)
            for (int i = 0; i < n; ++i)
            {
                double p = 0.0;
                for (int c = 0; c < chans; ++c)
                {
                    const float k = rlb[(size_t) c].process (rlbCoeffs, pre[(size_t) c].process (preCoeffs, ch[c][i]));
                    p += (double) k * k;
                }
                if (chans == 1)
                    p *= 2.0;   // a mono signal is heard on both sides
                fastMs = fastK * (fastMs - p) + p;
                slowMs = slowK * (slowMs - p) + p;
            }

            const float dt = (float) n / (float) sr;
            const float fastDb = lufs (fastMs);

            // What this material usually is when it plays: quick to learn, slow to forget
            if (fastDb > -60.0f)
                usualDb += (fastDb > usualDb ? 1.0f - std::exp (-dt / 1.0f) : 1.0f - std::exp (-dt / 8.0f)) * (fastDb - usualDb);

            // A 3 s window that starts from nothing reads low for its first seconds, by exactly
            // 1 - exp (-t / 3 s) for a steady signal. Until it has filled, divide that out: the level is
            // right from the first half second, instead of creeping up to it (and the gain with it).
            if (fastDb > -60.0f || firstS > 0.0f)
                firstS += dt;
            const bool warm = firstS > 9.0f;
            const double fill = warm ? 1.0 : 1.0 - std::exp (-(double) firstS / 3.0);
            const bool seeded = firstS > 0.5f;
            const float slowNow = lufs (slowMs / std::max (0.05, fill));

            float wanted = gainDb;

            if (! on)
            {
                wanted = gainDb * std::exp (-dt / 0.3f);
                learntS = 0.0f;
            }
            else if (seeded)
            {
                const bool gap = fastDb < -60.0f || fastDb < usualDb - 20.0f || slowNow < -50.0f;
                const bool burst = fastDb > slowNow + 6.0f;

                if (! gap && ! burst)
                {
                    const float desired = std::clamp (targets[(size_t) method] - slowNow, -12.0f, 12.0f);
                    const float tau = learntS < 2.0f ? 0.5f : 6.0f;
                    wanted = gainDb + (desired - gainDb) * (1.0f - std::exp (-dt / tau));
                    learntS += dt;
                }
            }

            // Ramp across the block: never a step
            const float from = gain;
            gainDb = wanted;
            const float to = dbToGain (gainDb);
            gain = to;

            if (std::abs (to - 1.0f) < 1.0e-6f && std::abs (from - 1.0f) < 1.0e-6f)
                return;

            const float step = (to - from) / (float) n;
            for (int c = 0; c < chans; ++c)
            {
                float g = from;
                for (int i = 0; i < n; ++i)
                {
                    g += step;
                    ch[c][i] *= g;
                }
            }
        }

    private:
        static float lufs (double ms) noexcept { return ms > 1.0e-20 ? (float) (-0.691 + 10.0 * std::log10 (ms)) : -120.0f; }

        double sr = 48000.0;
        BiquadCoeffs preCoeffs, rlbCoeffs;
        std::array<BiquadState, 2> pre {}, rlb {};
        float fastK = 0.0f, slowK = 0.0f;
        double fastMs = 0.0, slowMs = 0.0;
        float usualDb = -70.0f, gainDb = 0.0f, gain = 1.0f, learntS = 0.0f, firstS = 0.0f;
    };
}
