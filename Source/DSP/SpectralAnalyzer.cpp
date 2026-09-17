#include "SpectralAnalyzer.h"

namespace enh::dsp
{
    void SpectralAnalyzer::prepare (double sr)
    {
        const int order = sr > 64000.0 ? 11 : 10;          // ~21 ms window at any common rate
        size = 1 << order;
        mask = size - 1;
        hop = size / 4;

        fft = std::make_unique<juce::dsp::FFT> (order);
        ring.assign ((size_t) size, 0.0f);
        work.assign ((size_t) size * 2, 0.0f);
        window.resize ((size_t) size);
        for (int i = 0; i < size; ++i)
            window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / size));

        const int bins = size / 2 + 1;
        reference.assign ((size_t) bins, 0.0f);
        for (auto& p : power) p.assign ((size_t) bins, 0.0f);
        for (auto& p : peaks) p.assign ((size_t) bins, 0);

        const double binHz = sr / size;
        binLo = std::max (8, (int) std::lround (700.0 / binHz));
        binHi = std::min (bins - 9, (int) std::lround (std::min (8000.0, 0.45 * sr) / binHz));
        holdDecay = std::exp (-(float) hop / (float) sr / 0.35f);
        reset();
    }

    void SpectralAnalyzer::reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        for (auto& p : power) std::fill (p.begin(), p.end(), 0.0f);
        for (auto& p : peaks) std::fill (p.begin(), p.end(), (uint8_t) 0);
        writePos = histPos = 0;
        toHop = hop;
        referenceValid = false;
        tonality = tonalityHold = risingTonality = 0.0f;
        flatness = 1.0f;
        frameCount = 0;
    }

    void SpectralAnalyzer::captureReference() noexcept
    {
        std::copy (power[(size_t) histPos].begin(), power[(size_t) histPos].end(), reference.begin());
        referenceValid = true;
    }

    void SpectralAnalyzer::analyse() noexcept
    {
        for (int i = 0; i < size; ++i)
            work[(size_t) i] = ring[(size_t) ((writePos + i) & mask)] * window[(size_t) i];
        std::fill (work.begin() + size, work.end(), 0.0f);
        fft->performFrequencyOnlyForwardTransform (work.data(), true);

        histPos = (histPos + 1) % historyFrames;
        auto& P = power[(size_t) histPos];
        auto& pk = peaks[(size_t) histPos];
        const auto& pk2 = peaks[(size_t) ((histPos + historyFrames - 2) % historyFrames)];
        const auto& pk4 = peaks[(size_t) ((histPos + historyFrames - 4) % historyFrames)];

        for (int b = binLo - 8; b <= binHi + 8; ++b)
            P[(size_t) b] = work[(size_t) b] * work[(size_t) b];

        double total = 0.0, tonal = 0.0, logSum = 0.0, rising = 0.0, risingTonal = 0.0;
        const int count = binHi - binLo + 1;

        for (int b = binLo; b <= binHi; ++b)
        {
            const float p = P[(size_t) b];
            float neighbours = 0.0f;
            for (int d = 2; d <= 7; ++d)
                neighbours += P[(size_t) (b - d)] + P[(size_t) (b + d)];

            const bool isPeak = p > P[(size_t) (b - 1)] && p >= P[(size_t) (b + 1)] && p > 6.3f * (neighbours / 12.0f) && p > 1.0e-12f;
            pk[(size_t) b] = isPeak ? 1 : 0;

            total += p;
            logSum += std::log (p + 1.0e-14f);

            const bool rose = referenceValid && p > 4.0f * reference[(size_t) b] + 1.0e-12f;
            if (rose)
                rising += p;

            if (isPeak)
            {
                auto near = [b] (const std::vector<uint8_t>& h) { return h[(size_t) (b - 1)] | h[(size_t) b] | h[(size_t) (b + 1)]; };

                if (near (pk2) && near (pk4))
                {
                    const double lobe = p + 0.5 * (P[(size_t) (b - 1)] + P[(size_t) (b + 1)]);
                    tonal += lobe;
                    if (rose)
                        risingTonal += lobe;
                }
            }
        }

        tonality = total > 1.0e-10 ? (float) std::min (1.0, tonal / total) : 0.0f;
        tonalityHold = std::max (tonality, tonalityHold * holdDecay);
        risingTonality = rising > 1.0e-10 ? (float) std::min (1.0, risingTonal / rising) : 0.0f;

        const double mean = total / count;
        flatness = mean > 1.0e-12 ? (float) std::clamp (std::exp (logSum / count) / mean, 0.0, 1.0) : 1.0f;
        ++frameCount;
    }
}
