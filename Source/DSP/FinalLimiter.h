#pragma once

#include <vector>
#include "DspMath.h"

namespace enh::dsp
{
    /** The one output limiter, at the very end of the rack.

        It replaces two zero-latency limiters (TONE & SPACE's and the engine's safety limiter) that
        pulled the gain down on each waveform peak and let it recover within the same bass cycle:
        the gain wobbled at the bass frequency, which is distortion - loud bass sounded crushed.

        Here the gain is worked out 1.5 ms ahead (lookahead), held for 25 ms - longer than a 40 Hz
        cycle, so it never moves inside a bass note - ramped down over the lookahead so the peak
        arrives at the reduced gain, and released over 150 ms. Stereo-linked. A soft clip above the
        ceiling remains only as a guard. Latency: the lookahead (reported to the host).
    */
    class FinalLimiter
    {
    public:
        static constexpr float ceiling = 0.944f;   // -0.5 dBFS

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            lookahead = std::max (1, (int) std::lround (0.0015 * sr));
            window = lookahead + std::max (1, (int) std::lround (0.025 * sr));
            release = (float) std::exp (-1.0 / (0.150 * sr));
            for (auto& d : delay)
                d.assign ((size_t) lookahead, 0.0f);
            minValue.assign ((size_t) window + 1, 1.0f);
            minIndex.assign ((size_t) window + 1, 0);
            boxcar.assign ((size_t) lookahead, 1.0f);
            reset();
        }

        void reset()
        {
            for (auto& d : delay)
                std::fill (d.begin(), d.end(), 0.0f);
            std::fill (boxcar.begin(), boxcar.end(), 1.0f);
            boxSum = (double) lookahead;
            head = tail = 0;
            pos = 0;
            counter = 0;
            gain = 1.0f;
            reductionDb = 0.0f;
        }

        int getLatencySamples() const noexcept { return lookahead; }
        float getReductionDb() const noexcept  { return reductionDb; }

        void process (float* const* ch, int numChannels, int numSamples) noexcept
        {
            const int chans = std::min (2, numChannels);
            float deepest = 1.0f;
            const int cap = (int) minValue.size();

            for (int i = 0; i < numSamples; ++i)
            {
                float peak = 0.0f;
                for (int c = 0; c < chans; ++c)
                    peak = std::max (peak, std::abs (ch[c][i]));
                const float needed = peak > ceiling ? ceiling / peak : 1.0f;

                // Sliding minimum of `needed` over the last `window` samples (monotonic queue)
                while (head != tail && minValue[(size_t) ((tail - 1 + cap) % cap)] >= needed)
                    tail = (tail - 1 + cap) % cap;
                minValue[(size_t) tail] = needed;
                minIndex[(size_t) tail] = counter;
                tail = (tail + 1) % cap;
                while (minIndex[(size_t) head] <= counter - window)
                    head = (head + 1) % cap;
                const float held = minValue[(size_t) head];

                // Ramp over the lookahead (moving average), then release slowly
                boxSum += (double) held - (double) boxcar[(size_t) pos];
                boxcar[(size_t) pos] = held;
                const float ramped = (float) (boxSum / (double) lookahead);
                gain = ramped < gain ? ramped : ramped + (gain - ramped) * release;
                deepest = std::min (deepest, gain);

                for (int c = 0; c < chans; ++c)
                {
                    auto& d = delay[(size_t) c];
                    const float out = d[(size_t) pos] * gain;
                    d[(size_t) pos] = ch[c][i];
                    const float a = std::abs (out);
                    ch[c][i] = a <= 0.985f ? out : std::copysign (0.985f + 0.015f * std::tanh ((a - 0.985f) / 0.015f), out);
                }
                pos = (pos + 1) % lookahead;
                ++counter;
            }

            reductionDb = -20.0f * std::log10 (std::max (1.0e-3f, deepest));
        }

    private:
        double sr = 48000.0;
        int lookahead = 72, window = 1272, pos = 0, head = 0, tail = 0;
        long long counter = 0;
        std::array<std::vector<float>, 2> delay;
        std::vector<float> minValue, boxcar;
        std::vector<long long> minIndex;
        double boxSum = 0.0;
        float gain = 1.0f, release = 0.9999f, reductionDb = 0.0f;
    };
}
