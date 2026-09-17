#include "SpectrumScope.h"

#include <cmath>
#include <algorithm>

namespace enh::dsp
{
    ScopeAnalyser::ScopeAnalyser()
    {
        window.resize ((size_t) fftSize);
        scratch.resize ((size_t) fftSize * 2);
        inputDb.resize ((size_t) fftSize / 2);
        outputDb.resize ((size_t) fftSize / 2);
        smoothedIn.assign ((size_t) ScopeCurve::numPoints, ScopeCurve::minDb);
        smoothedOut.assign ((size_t) ScopeCurve::numPoints, ScopeCurve::minDb);
        peaks.assign ((size_t) ScopeCurve::numPoints, ScopeCurve::minDb);
        binForPoint.resize ((size_t) ScopeCurve::numPoints);

        // Hann window, normalised so a full-scale sine reads 0 dBFS
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) (fftSize - 1));
    }

    void ScopeAnalyser::prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;

        for (int i = 0; i < ScopeCurve::numPoints; ++i)
        {
            const float hz = ScopeCurve::hzForPoint (i);
            binForPoint[(size_t) i] = std::clamp ((int) std::round (hz * (float) fftSize / (float) sr), 1, fftSize / 2 - 1);
        }

        ready = true;
    }

    void ScopeAnalyser::analyse (const ScopeFifo& fifo, std::vector<float>& magsDb)
    {
        const int w = fifo.writeIndex.load (std::memory_order_acquire);

        // Newest fftSize samples, oldest first
        for (int i = 0; i < fftSize; ++i)
        {
            const int idx = (w - fftSize + i) & ScopeFifo::mask;
            scratch[(size_t) i] = fifo.samples[(size_t) idx] * window[(size_t) i];
        }

        std::fill (scratch.begin() + fftSize, scratch.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (scratch.data());

        const float norm = 2.0f / ((float) fftSize * 0.5f);   // Hann coherent gain = 0.5
        for (int i = 0; i < fftSize / 2; ++i)
            magsDb[(size_t) i] = 20.0f * std::log10 (std::max (1.0e-9f, scratch[(size_t) i] * norm));
    }

    void ScopeAnalyser::update (const ScopeFifo& in, const ScopeFifo& out, ScopeCurve& curve, float dt)
    {
        if (! ready)
            return;

        analyse (in, inputDb);
        analyse (out, outputDb);

        // Per-octave-ish smoothing: average the bins that fall into each display point, which is
        // what keeps the high end from turning into noise while the low end stays resolved.
        const float rise = 1.0f - std::exp (-dt / 0.035f);    // attack: quick
        const float fall = 1.0f - std::exp (-dt / 0.22f);     // decay: slow enough to read
        const float peakFall = 28.0f * dt;                    // dB per second

        float sumIn = 0.0f, sumOut = 0.0f;

        for (int p = 0; p < ScopeCurve::numPoints; ++p)
        {
            const int lo = p == 0 ? binForPoint[0]
                                  : (binForPoint[(size_t) (p - 1)] + binForPoint[(size_t) p]) / 2;
            const int hi = p == ScopeCurve::numPoints - 1
                             ? binForPoint[(size_t) p]
                             : (binForPoint[(size_t) p] + binForPoint[(size_t) (p + 1)]) / 2;

            float bestIn = ScopeCurve::minDb, bestOut = ScopeCurve::minDb;
            for (int b = std::min (lo, hi); b <= std::max (lo, hi); ++b)
            {
                bestIn = std::max (bestIn, inputDb[(size_t) b]);
                bestOut = std::max (bestOut, outputDb[(size_t) b]);
            }

            auto& si = smoothedIn[(size_t) p];
            auto& so = smoothedOut[(size_t) p];
            si += (bestIn > si ? rise : fall) * (bestIn - si);
            so += (bestOut > so ? rise : fall) * (bestOut - so);

            auto& pk = peaks[(size_t) p];
            pk = so > pk ? so : std::max (so, pk - peakFall);

            curve.inputDb[(size_t) p].store (si, std::memory_order_relaxed);
            curve.outputDb[(size_t) p].store (so, std::memory_order_relaxed);
            curve.peakDb[(size_t) p].store (pk, std::memory_order_relaxed);

            sumIn += std::pow (10.0f, si * 0.1f);
            sumOut += std::pow (10.0f, so * 0.1f);
        }

        curve.inputRmsDb.store (10.0f * std::log10 (std::max (1.0e-9f, sumIn)), std::memory_order_relaxed);
        curve.outputRmsDb.store (10.0f * std::log10 (std::max (1.0e-9f, sumOut)), std::memory_order_relaxed);
        curve.active.store (curve.outputRmsDb.load() > -80.0f, std::memory_order_relaxed);
    }
}
