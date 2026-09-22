#pragma once

#include <array>
#include <vector>
#include "DspMath.h"

namespace enh::dsp
{
    /** Loudness to ITU-R BS.1770-4 / EBU R128: K-weighted, in LUFS.

          momentary   400 ms sliding window
          short-term  3 s sliding window
          integrated  since the last reset, gated (absolute -70 LUFS, relative -10 LU), from 400 ms
                      blocks every 100 ms
          true peak   4x oversampled (windowed-sinc interpolator), dBTP, held until reset

        Stereo, both channels weighted 1.0. The audio is only read. Real-time safe after prepare().
    */
    class LoudnessMeter
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            designKWeighting();
            blockLen = std::max (1, (int) std::lround (0.1 * sr));   // 100 ms
            designInterpolator();
            reset();
        }

        void reset()
        {
            for (auto& c : pre) c.reset();
            for (auto& c : rlb) c.reset();
            blockSum = 0.0;
            blockPos = 0;
            blocks.fill (0.0);
            blockHead = blocksSeen = 0;
            resetIntegrated();
            for (auto& h : history) h.fill (0.0f);
            historyPos = {};
        }

        /** Integrated loudness and true-peak hold start again (the meter's RESET). */
        void resetIntegrated()
        {
            histogram.fill (0);
            gatedCount = 0;
            truePeak = 0.0f;
            integratedLufs = -70.0f;
        }

        void process (const float* const* ch, int numChannels, int numSamples) noexcept
        {
            const int chans = std::min (2, numChannels);
            if (chans <= 0)
                return;

            for (int i = 0; i < numSamples; ++i)
            {
                double sum = 0.0;
                for (int c = 0; c < chans; ++c)
                {
                    const float x = ch[c][i];
                    const float k = rlb[(size_t) c].process (rlbCoeffs, pre[(size_t) c].process (preCoeffs, x));
                    sum += (double) k * (double) k;
                    truePeak = std::max (truePeak, interpolatedPeak (c, x));
                }
                blockSum += sum;

                if (++blockPos >= blockLen)
                {
                    closeBlock();
                    blockPos = 0;
                    blockSum = 0.0;
                }
            }
        }

        float getMomentaryLufs() const noexcept  { return momentaryLufs; }
        float getShortTermLufs() const noexcept  { return shortTermLufs; }
        float getIntegratedLufs() const noexcept { return integratedLufs; }
        float getTruePeakDb() const noexcept     { return 20.0f * std::log10 (std::max (1.0e-6f, truePeak)); }

    private:
        // K-weighting for any sample rate (the analogue prototypes of BS.1770, as libebur128 derives them)
        void designKWeighting()
        {
            {
                const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
                const double K = std::tan (pi * f0 / sr);
                const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
                const double a0 = 1.0 + K / Q + K * K;
                preCoeffs = { (float) ((Vh + Vb * K / Q + K * K) / a0), (float) (2.0 * (K * K - Vh) / a0),
                              (float) ((Vh - Vb * K / Q + K * K) / a0), (float) (2.0 * (K * K - 1.0) / a0),
                              (float) ((1.0 - K / Q + K * K) / a0) };
            }
            {
                const double f0 = 38.13547087602444, Q = 0.5003270373238773;
                const double K = std::tan (pi * f0 / sr);
                const double a0 = 1.0 + K / Q + K * K;
                rlbCoeffs = { 1.0f, -2.0f, 1.0f, (float) (2.0 * (K * K - 1.0) / a0), (float) ((1.0 - K / Q + K * K) / a0) };
            }
        }

        // 4x interpolator: 4 phases x 12 taps, Kaiser-windowed sinc (passband to 20 kHz at 48 kHz)
        static constexpr int phases = 4, taps = 12;

        void designInterpolator()
        {
            const double beta = 7.0;
            auto bessel0 = [] (double x)
            {
                double s = 1.0, t = 1.0;
                for (int k = 1; k < 25; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; }
                return s;
            };
            const int n = phases * taps;
            for (int p = 0; p < phases; ++p)
                for (int t = 0; t < taps; ++t)
                {
                    const int idx = t * phases + p;
                    const double m = idx - 0.5 * (n - 1);
                    const double x = m / phases;
                    const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (pi * x * 0.92) / (pi * x * 0.92) * 0.92;
                    const double w = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - std::pow (2.0 * idx / (n - 1.0) - 1.0, 2.0)))) / bessel0 (beta);
                    fir[(size_t) p][(size_t) t] = (float) (sinc * w);
                }
            // Each phase sums to 1 (DC gain exact), so a steady level reads the same on every phase
            for (auto& ph : fir)
            {
                float s = 0.0f;
                for (float v : ph) s += v;
                for (float& v : ph) v /= s;
            }
        }

        inline float interpolatedPeak (int c, float x) noexcept
        {
            // Mirrored history (each sample stored twice) so the last `taps` samples are contiguous
            auto& h = history[(size_t) c];
            auto& at = historyPos[(size_t) c];
            h[(size_t) at] = h[(size_t) (at + taps)] = x;
            if (++at == taps) at = 0;
            const float* window = h.data() + at;   // oldest .. newest
            float best = std::abs (x);
            for (int p = 0; p < phases; ++p)
            {
                const float* f = fir[(size_t) p].data();
                float acc = 0.0f;
                for (int k = 0; k < taps; ++k)
                    acc += f[taps - 1 - k] * window[k];
                best = std::max (best, std::abs (acc));
            }
            return best;
        }

        static float toLufs (double meanSquare) noexcept
        {
            return meanSquare > 1.0e-20 ? (float) (-0.691 + 10.0 * std::log10 (meanSquare)) : -120.0f;
        }

        void closeBlock() noexcept
        {
            blocks[(size_t) blockHead] = blockSum / (double) blockLen;
            blockHead = (blockHead + 1) % (int) blocks.size();
            blocksSeen = std::min (blocksSeen + 1, (int) blocks.size());

            auto meanOfLast = [this] (int count)
            {
                double s = 0.0;
                const int have = std::min (count, blocksSeen);
                for (int k = 1; k <= have; ++k)
                    s += blocks[(size_t) ((blockHead - k + (int) blocks.size()) % (int) blocks.size())];
                return have > 0 ? s / count : 0.0;   // a window not yet full counts as silence where empty
            };
            const double momentary = meanOfLast (4);
            momentaryLufs = toLufs (momentary);
            shortTermLufs = toLufs (meanOfLast (30));

            // Integrated: every 400 ms block (100 ms hop) above the absolute gate goes in the histogram
            if (blocksSeen >= 4 && momentaryLufs > -70.0f)
            {
                const int bin = std::clamp ((int) ((momentaryLufs + 70.0f) * 10.0f), 0, numBins - 1);
                ++histogram[(size_t) bin];
                ++gatedCount;
            }
            updateIntegrated();
        }

        void updateIntegrated() noexcept
        {
            if (gatedCount == 0)
            {
                integratedLufs = -70.0f;
                return;
            }
            auto binEnergy = [] (int bin) { return std::pow (10.0, ((bin + 0.5) / 10.0 - 70.0 + 0.691) / 10.0); };
            double e = 0.0;
            for (int b = 0; b < numBins; ++b)
                if (histogram[(size_t) b] > 0)
                    e += histogram[(size_t) b] * binEnergy (b);
            const float ungated = toLufs (e / gatedCount);
            const float relativeGate = ungated - 10.0f;

            double e2 = 0.0;
            int n2 = 0;
            const int from = std::clamp ((int) ((relativeGate + 70.0f) * 10.0f), 0, numBins - 1);
            for (int b = from; b < numBins; ++b)
                if (histogram[(size_t) b] > 0)
                {
                    e2 += histogram[(size_t) b] * binEnergy (b);
                    n2 += histogram[(size_t) b];
                }
            integratedLufs = n2 > 0 ? toLufs (e2 / n2) : -70.0f;
        }

        static constexpr double pi = 3.14159265358979323846;
        static constexpr int numBins = 800;   // -70 .. +10 LUFS in 0.1 LU

        double sr = 48000.0;
        BiquadCoeffs preCoeffs, rlbCoeffs;
        std::array<BiquadState, 2> pre {}, rlb {};
        int blockLen = 4800, blockPos = 0, blockHead = 0, blocksSeen = 0;
        double blockSum = 0.0;
        std::array<double, 30> blocks {};
        std::array<int, numBins> histogram {};
        int gatedCount = 0;
        float momentaryLufs = -120.0f, shortTermLufs = -120.0f, integratedLufs = -70.0f, truePeak = 0.0f;

        std::array<std::array<float, taps>, phases> fir {};
        std::array<std::array<float, 2 * taps>, 2> history {};
        std::array<int, 2> historyPos {};
    };
}
