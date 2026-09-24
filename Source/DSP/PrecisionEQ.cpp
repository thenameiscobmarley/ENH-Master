#include "PrecisionEQ.h"

#include <algorithm>
#include <cmath>

namespace enh::dsp
{
    float PrecisionEQ::gridHz (int i) noexcept
    {
        return 40.0f * std::pow (2.0f, (float) i / 24.0f);
    }

    float PrecisionEQ::bellDb (const Band& b, float hz) noexcept
    {
        if (std::abs (b.gainDb) < 1.0e-3f || hz <= 0.0f)
            return 0.0f;
        // Bandwidth (octaves) from Q, as a Gaussian in log frequency with that width at half its depth
        const float bw = 2.0f / 0.693147f * std::asinh (1.0f / (2.0f * std::max (0.1f, b.q)));
        const float x = std::log2 (hz / b.hz) / std::max (0.02f, bw / 2.3548f);
        return b.gainDb * std::exp (-0.5f * x * x);
    }

    void PrecisionEQ::prepare (double sampleRate, int maxChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        channels = std::clamp (maxChannels, 1, 2);
        fft = std::make_unique<juce::dsp::FFT> (fftOrder);
        ring.assign ((size_t) fftSize, 0.0f);
        window.assign ((size_t) fftSize, 0.0f);
        work.assign ((size_t) (2 * fftSize), 0.0f);
        longTermBins.assign ((size_t) (fftSize / 2), 0.0f);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) fftSize);

        hop = std::max (256, (int) std::lround (2048.0 * sr / 48000.0));
        hop = std::min (hop, fftSize / 2);
        frameDt = (float) hop / (float) sr;

        const double binHz = sr / (double) fftSize;
        for (int i = 0; i < gridPoints; ++i)
        {
            const double f = gridHz (i);
            const double lo = f * std::pow (2.0, -1.0 / 48.0) / binHz, hi = f * std::pow (2.0, 1.0 / 48.0) / binHz;
            const int blo = (int) std::ceil (lo), bhi = (int) std::floor (hi);
            if (bhi >= blo)
            {
                binLo[(size_t) i] = std::clamp (blo, 1, fftSize / 2 - 1);
                binHi[(size_t) i] = std::clamp (bhi, 1, fftSize / 2 - 1);
                binFrac[(size_t) i] = -1.0f;   // average the range
            }
            else
            {
                const double b = f / binHz;
                binLo[(size_t) i] = std::clamp ((int) std::floor (b), 1, fftSize / 2 - 2);
                binHi[(size_t) i] = binLo[(size_t) i] + 1;
                binFrac[(size_t) i] = (float) (b - std::floor (b));   // interpolate two bins
            }
        }
        reset();
    }

    void PrecisionEQ::reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = 0;
        toHop = hop;
        analysePending = haveSpectrum = targetsFresh = false;
        framesIn = numTargets = 0;
        longTermPow.fill (0.0f);
        std::fill (longTermBins.begin(), longTermBins.end(), 0.0f);
        lineNear.fill (false);
        longTermDb.fill (-200.0f);
        for (auto& b : bands) b = Band {};
        wantDb.fill (0.0f);
        wantHz.fill (1000.0f);
        wantQ.fill (1.0f);
        following.fill (false);
        lift.fill (0.0f);
        for (auto& ch : states) for (auto& st : ch) st.reset();
    }

    void PrecisionEQ::analyseFrame() noexcept
    {
        // The last fftSize samples, oldest first, windowed
        double energy = 0.0;
        for (int i = 0; i < fftSize; ++i)
        {
            const float x = ring[(size_t) ((writePos + i) & fftMask)];
            energy += (double) x * x;
            work[(size_t) i] = x * window[(size_t) i];
        }
        std::fill (work.begin() + fftSize, work.end(), 0.0f);

        // Silence (under about -75 dBFS RMS) teaches it nothing
        if (energy / fftSize < 3.0e-8)
            return;

        fft->performFrequencyOnlyForwardTransform (work.data(), true);

        const float alpha = std::max (1.0f - std::exp (-frameDt / std::max (0.2f, integrationS)), 1.0f / (float) (framesIn + 1));
        for (int b = 1; b < fftSize / 2; ++b)
            longTermBins[(size_t) b] += (work[(size_t) b] * work[(size_t) b] - longTermBins[(size_t) b]) * alpha;
        for (int i = 0; i < gridPoints; ++i)
        {
            float p;
            if (binFrac[(size_t) i] < 0.0f)
            {
                double sum = 0.0;
                for (int b = binLo[(size_t) i]; b <= binHi[(size_t) i]; ++b)
                    sum += (double) work[(size_t) b] * work[(size_t) b];
                p = (float) (sum / (binHi[(size_t) i] - binLo[(size_t) i] + 1));
            }
            else
            {
                const float a = work[(size_t) binLo[(size_t) i]], b = work[(size_t) binHi[(size_t) i]];
                p = a * a + (b * b - a * a) * binFrac[(size_t) i];
            }
            longTermPow[(size_t) i] += (p - longTermPow[(size_t) i]) * alpha;
            longTermDb[(size_t) i] = 10.0f * std::log10 (longTermPow[(size_t) i] + 1.0e-20f);
        }
        ++framesIn;
        haveSpectrum = true;
    }

    void PrecisionEQ::findFeatures (const Settings& s) noexcept
    {
        numTargets = 0;
        const int wanted = std::clamp (s.bands, 0, maxBands);
        if (wanted == 0 || ! haveSpectrum || framesIn < 4)
            return;

        const auto& L = longTermDb;

        // The spectrum smoothed (Gaussian, sigma = 1/4 octave) - and again three times with what sticks
        // out of the last pass clipped off, so a strong resonance does not
        // lift the very baseline it is measured against (and a hole does not sink it)
        constexpr int radius = 18;
        constexpr float sigma = 6.0f;   // 1/4 octave
        auto gaussian = [&] (const std::array<float, gridPoints>& in, std::array<float, gridPoints>& out)
        {
            for (int i = 0; i < gridPoints; ++i)
            {
                float sum = 0.0f, wsum = 0.0f;
                for (int d = -radius; d <= radius; ++d)
                {
                    const int j = std::clamp (i + d, 0, gridPoints - 1);
                    const float w = std::exp (-0.5f * (float) (d * d) / (sigma * sigma));
                    sum += w * in[(size_t) j];
                    wsum += w;
                }
                out[(size_t) i] = sum / wsum;
            }
        };
        std::array<float, gridPoints> clipped {}, smooth {};
        gaussian (L, smooth);
        for (int pass = 0; pass < 3; ++pass)
        {
            for (int i = 0; i < gridPoints; ++i)
                clipped[(size_t) i] = std::clamp (L[(size_t) i], smooth[(size_t) i] - 1.0f, smooth[(size_t) i] + 1.0f);
            gaussian (clipped, smooth);
        }

        float top = -200.0f;
        for (float v : L) top = std::max (top, v);
        const float contentFloor = top - 55.0f;

        std::array<float, gridPoints> r {};
        for (int i = 0; i < gridPoints; ++i)
            r[(size_t) i] = L[(size_t) i] - smooth[(size_t) i];

        const float scale = std::clamp (s.normalize / 0.5f, 0.0f, 2.0f) * std::clamp (s.strength, 0.0f, 2.0f);
        if (scale <= 0.01f)
            return;

        // Spectral lines - notes, hums, test tones, a regular click's comb: a bin four times stronger than the
        // bins three either side of it (a Hann line is two bins wide; noise, and a resonance's smooth bump,
        // never are). Where there are lines the spectrum is content, not a room: no feature near one - not
        // the line, and not the gap between two lines either
        {
            std::vector<char> lineBin ((size_t) (fftSize / 2), 0);
            const float floorPow = std::pow (10.0f, contentFloor / 10.0f);
            for (int b = 4; b < fftSize / 2 - 4; ++b)
            {
                const float p = longTermBins[(size_t) b];
                lineBin[(size_t) b] = p > floorPow && p > 2.0f * (longTermBins[(size_t) (b - 3)] + longTermBins[(size_t) (b + 3)]) ? 1 : 0;
            }
            const double binHz = sr / fftSize;
            for (int i = 0; i < gridPoints; ++i)
            {
                // this point's range, widened to a sixth of an octave or 3 bins, whichever is more
                const double f = gridHz (i), reach = std::max (f * (std::pow (2.0, 1.0 / 12.0) - 1.0), 3.0 * binHz);
                const int b0 = std::max (1, (int) ((f - reach) / binHz)), b1 = std::min (fftSize / 2 - 1, (int) ((f + reach) / binHz) + 1);
                bool any = false;
                for (int b = b0; b <= b1 && ! any; ++b)
                    any = lineBin[(size_t) b] != 0;
                lineNear[(size_t) i] = any;
            }
        }

        // A resonance is wider than one tone's line in the FFT: a peak about that narrow is a note, a hum or a
        // test tone - content - and is left alone, as is any "hole" right beside one (the gap between two
        // harmonics is not a hole to fill)
        const float lineHz = 4.0f * (float) (sr / fftSize);
        std::array<bool, gridPoints> toneAt {};
        for (int i = 1; i < gridPoints - 1; ++i)
        {
            if (! (r[(size_t) i] > 2.5f && r[(size_t) i] >= r[(size_t) (i - 1)] && r[(size_t) i] > r[(size_t) (i + 1)]))
                continue;
            // Its width 3 dB down (a tone's line is under 20 Hz wide there; a resonance, hundreds of Hz)
            int lo = i, hi = i;
            while (lo > 0 && r[(size_t) (lo - 1)] > r[(size_t) i] - 3.0f) --lo;
            while (hi < gridPoints - 1 && r[(size_t) (hi + 1)] > r[(size_t) i] - 3.0f) ++hi;
            const float widthHz = gridHz (hi) * std::pow (2.0f, 1.0f / 48.0f) - gridHz (lo) * std::pow (2.0f, -1.0f / 48.0f);
            if (widthHz < lineHz || hi - lo <= 1)   // (a tone between two grid points lights both)
                for (int j = std::max (0, i - 4); j <= std::min (gridPoints - 1, i + 4); ++j)
                    toneAt[(size_t) j] = true;   // within 1/6 octave of a tone
        }

        struct Cand { float hz, q, gainDb, strength; int at; };
        std::array<Cand, gridPoints> cands {};
        int n = 0;

        for (int i = 2; i < gridPoints - 2; ++i)
        {
            const float v = r[(size_t) i];
            const bool peak = v > 2.5f && v >= r[(size_t) (i - 1)] && v > r[(size_t) (i + 1)];
            const bool dip = v < -3.5f && v <= r[(size_t) (i - 1)] && v < r[(size_t) (i + 1)]
                             && smooth[(size_t) i] > contentFloor;   // only fill where there is sound
            if (! peak && ! dip)
                continue;
            if (toneAt[(size_t) i] || lineNear[(size_t) i])
                continue;
            if (L[(size_t) i] < contentFloor && peak)
                continue;

            // Where it falls to half its height either side: its width
            const float half = 0.5f * v;
            auto edge = [&] (int dir)
            {
                int j = i;
                while (j + dir > 0 && j + dir < gridPoints - 1 && (v > 0.0f ? r[(size_t) (j + dir)] > half : r[(size_t) (j + dir)] < half))
                    j += dir;
                const float a = r[(size_t) j], b = r[(size_t) std::clamp (j + dir, 0, gridPoints - 1)];
                const float t = std::abs (a - b) > 1.0e-6f ? std::clamp ((a - half) / (a - b), 0.0f, 1.0f) : 0.0f;
                return (float) j + (float) dir * t;
            };
            const float bw = std::clamp ((edge (1) - edge (-1)) / 24.0f, 1.0f / 24.0f, 1.2f);   // octaves
            const float pw = std::pow (2.0f, bw);
            const float q = std::clamp (std::sqrt (pw) / (pw - 1.0f), 0.8f, 16.0f);

            // Its centre, between grid points (parabola through the three around it)
            const float a = r[(size_t) (i - 1)], b = v, c = r[(size_t) (i + 1)];
            const float denom = a - 2.0f * b + c;
            const float off = std::abs (denom) > 1.0e-6f ? std::clamp (0.5f * (a - c) / denom, -0.5f, 0.5f) : 0.0f;
            const float hz = 40.0f * std::pow (2.0f, ((float) i + off) / 24.0f);

            float gain = peak ? -std::min (0.85f * (v - 1.0f), 8.0f) : std::min (0.5f * (-v - 2.0f), 3.0f);
            gain *= scale;
            // Footsteps and voices live in 250 Hz - 5 kHz: no broad scoop there (-4 dB at most for a wide
            // cut), but a narrow notch on a ringing resonance takes nothing they need (-8 dB from Q 6 up)
            if (peak && hz > 250.0f && hz < 5000.0f)
                gain = std::max (gain, -4.0f - 4.0f * std::clamp ((q - 2.0f) / 4.0f, 0.0f, 1.0f));
            gain = std::clamp (gain, -8.0f, 3.0f);
            if (std::abs (gain) < 0.5f)
                continue;

            cands[(size_t) n++] = { hz, q, gain, std::abs (gain), i };
        }

        std::sort (cands.begin(), cands.begin() + n, [] (const Cand& x, const Cand& y) { return x.strength > y.strength; });
        for (int k = 0; k < n && numTargets < wanted; ++k)
        {
            bool clear = true;
            for (int t = 0; t < numTargets; ++t)
                clear = clear && std::abs (std::log2 (cands[(size_t) k].hz / targets[(size_t) t].hz)) >= 1.0f / 6.0f;
            if (clear)
                targets[(size_t) numTargets++] = { cands[(size_t) k].hz, cands[(size_t) k].q, cands[(size_t) k].gainDb, cands[(size_t) k].strength };
        }
        targetsFresh = true;
    }

    void PrecisionEQ::update (const Settings& s, float dt) noexcept
    {
        integrationS = s.integrationS;
        if (analysePending)
        {
            analysePending = false;
            analyseFrame();
            findFeatures (s);
        }

        const int wanted = std::clamp (s.bands, 0, maxBands);

        // New features: each band keeps following the one nearest it; a free band takes a new one
        if (targetsFresh)
        {
            targetsFresh = false;
            following.fill (false);
            std::array<bool, maxBands> taken {};
            for (int t = 0; t < numTargets; ++t)
            {
                const auto& tg = targets[(size_t) t];
                int best = -1;
                float bestDist = 1.0f / 3.0f;
                for (int b = 0; b < wanted; ++b)
                {
                    if (taken[(size_t) b] || std::abs (bands[(size_t) b].gainDb) < 0.05f)
                        continue;
                    const float d = std::abs (std::log2 (tg.hz / bands[(size_t) b].hz));
                    if (d < bestDist && (bands[(size_t) b].gainDb < 0.0f) == (tg.gainDb < 0.0f))
                    {
                        bestDist = d;
                        best = b;
                    }
                }
                if (best < 0)
                    for (int b = 0; b < wanted && best < 0; ++b)
                        if (! taken[(size_t) b] && std::abs (bands[(size_t) b].gainDb) < 0.05f)
                        {
                            best = b;
                            bands[(size_t) b].hz = tg.hz;   // silent, so it can jump there
                            bands[(size_t) b].q = tg.q;
                        }
                if (best < 0)
                    continue;
                taken[(size_t) best] = following[(size_t) best] = true;
                wantHz[(size_t) best] = tg.hz;
                wantQ[(size_t) best] = tg.q;
                wantDb[(size_t) best] = tg.gainDb;
            }
            for (int b = 0; b < maxBands; ++b)
                if (! following[(size_t) b] || b >= wanted)
                    wantDb[(size_t) b] = 0.0f;
        }
        if (wanted == 0)
            wantDb.fill (0.0f);

        // Glide: frequency and width in the log domain, depth in (quicker) and out (slower)
        const float kMove = 1.0f - std::exp (-dt / 0.15f);
        for (int b = 0; b < maxBands; ++b)
        {
            auto& band = bands[(size_t) b];
            if (following[(size_t) b])
            {
                band.hz *= std::pow (wantHz[(size_t) b] / band.hz, kMove);
                band.q *= std::pow (wantQ[(size_t) b] / band.q, kMove);
            }
            const bool growing = std::abs (wantDb[(size_t) b]) > std::abs (band.gainDb);
            const float k = 1.0f - std::exp (-dt / (growing ? 0.12f : 0.40f));
            band.gainDb += (wantDb[(size_t) b] - band.gainDb) * k;
            if (std::abs (band.gainDb) < 0.01f && std::abs (wantDb[(size_t) b]) < 1.0e-6f)
                band.gainDb = 0.0f;

            const float A = std::pow (10.0f, band.gainDb / 40.0f);
            coeffs[(size_t) b] = SvfCoeffs::make (sr, band.hz, band.q * A);
            lift[(size_t) b] = A * A - 1.0f;
        }
    }

    void PrecisionEQ::process (float* const* ch, int numChannels, int startSample, int numSamples) noexcept
    {
        const int nc = std::min (numChannels, channels);
        for (int b = 0; b < maxBands; ++b)
        {
            if (std::abs (bands[(size_t) b].gainDb) < 1.0e-6f)
            {
                for (int c = 0; c < 2; ++c)
                    states[(size_t) c][(size_t) b].reset();   // idle: it starts clean next time
                continue;
            }
            const auto& co = coeffs[(size_t) b];
            const float l = lift[(size_t) b];
            for (int c = 0; c < nc; ++c)
            {
                auto& st = states[(size_t) c][(size_t) b];
                float* x = ch[c] + startSample;
                for (int i = 0; i < numSamples; ++i)
                    x[i] += l * st.process (co, x[i]).band;
            }
        }
    }
}
