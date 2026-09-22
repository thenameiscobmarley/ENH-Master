#pragma once

#include <array>
#include <vector>
#include "DspMath.h"

namespace enh::dsp
{
    /** A gain that is worked out ahead of the audio: the smallest gain needed over the next
        `window` samples (sliding minimum), ramped in over the lookahead so the peak arrives at the
        reduced gain, and released slowly. The window is longer than a 40 Hz cycle, so the gain
        never moves inside a bass note (moving inside a cycle is distortion). */
    class LookaheadGain
    {
    public:
        void prepare (int lookaheadSamples, int windowSamples, float releaseCoeff)
        {
            lookahead = std::max (1, lookaheadSamples);
            window = std::max (lookahead, windowSamples);
            release = releaseCoeff;
            minValue.assign ((size_t) window + 1, 1.0f);
            minIndex.assign ((size_t) window + 1, 0);
            boxcar.assign ((size_t) lookahead, 1.0f);
            reset();
        }

        void reset()
        {
            std::fill (boxcar.begin(), boxcar.end(), 1.0f);
            boxSum = (double) lookahead;
            head = tail = pos = 0;
            counter = 0;
            gain = 1.0f;
        }

        /** Feed what the audio `lookahead` samples from now needs (<= 1); returns the gain for now. */
        inline float push (float needed) noexcept
        {
            const int cap = (int) minValue.size();
            while (head != tail && minValue[(size_t) ((tail - 1 + cap) % cap)] >= needed)
                tail = (tail - 1 + cap) % cap;
            minValue[(size_t) tail] = needed;
            minIndex[(size_t) tail] = counter;
            tail = (tail + 1) % cap;
            while (minIndex[(size_t) head] <= counter - window)
                head = (head + 1) % cap;
            const float held = minValue[(size_t) head];

            boxSum += (double) held - (double) boxcar[(size_t) pos];
            boxcar[(size_t) pos] = held;
            pos = (pos + 1) % lookahead;
            ++counter;

            const float ramped = (float) (boxSum / (double) lookahead);
            gain = ramped < gain ? ramped : ramped + (gain - ramped) * release;
            return gain;
        }

        float current() const noexcept { return gain; }

    private:
        int lookahead = 72, window = 1272, head = 0, tail = 0, pos = 0;
        long long counter = 0;
        std::vector<float> minValue, boxcar;
        std::vector<long long> minIndex;
        double boxSum = 0.0;
        float gain = 1.0f, release = 0.9999f;
    };

    /** The output limiter, at the very end of the rack. It acts only on real overs: nothing under
        0 dBFS is touched. When something would go over, it uses the same idea as the SPECTRAL
        LIMITER - cut the region that is causing it, not the whole mix - and only then limits the
        whole mix for whatever is left.

        1. Spectral (1.5 ms lookahead): the programme is split the same way it will be cut, into four
           regions (low < 150 Hz, low-mid ~400 Hz, mid ~1.6 kHz, high > 4 kHz). When a peak ahead would
           go over, the region pushing hardest in the peak's direction is cut by exactly what brings
           that peak back under (up to 12 dB), with a smooth region-shaped filter (low / high shelf,
           or a bell), held through the event and released over 150 ms. A kick or an explosion that clips takes
           its own low end down; the footsteps and voices above it stay where they were.
        2. Broadband (1.5 ms lookahead) on what the spectral stage let through: guarantees nothing
           leaves above 0 dBFS. Gain held 25 ms (longer than a bass cycle), released over 150 ms.

        Stereo-linked. Latency: both lookaheads (3 ms, reported to the host).
    */
    class FinalLimiter
    {
    public:
        /** CEILING method (MethodRegistry.h): 0 dBFS, -0.3 or -1.0 dBFS. The lookahead gains move to a new
            ceiling at their own attack / release, so a change never steps the audio. */
        void setCeilingMethod (int method) noexcept
        {
            ceiling = method == 1 ? 0.96605088f : method == 2 ? 0.89125094f : 1.0f;
        }
        static constexpr int numRegions = 4;

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            lookahead = std::max (1, (int) std::lround (0.0015 * sr));
            const int window = lookahead + std::max (1, (int) std::lround (0.025 * sr));
            const float release = (float) std::exp (-1.0 / (0.150 * sr));

            broadband.prepare (lookahead, window, release);
            for (auto& g : regionGain)
                g.prepare (lookahead, window, release);

            // Region filters: the same shapes for measuring a region and for cutting it
            regionCoeffs[0] = SvfCoeffs::make (sr, 150.0, 0.7071);    // low: low-pass (shelf)
            regionCoeffs[1] = SvfCoeffs::make (sr, 400.0, 0.9);       // low-mid: band (bell)
            regionCoeffs[2] = SvfCoeffs::make (sr, 1600.0, 0.9);      // mid: band (bell)
            regionCoeffs[3] = SvfCoeffs::make (sr, 4000.0, 0.7071);   // high: high-pass (shelf)

            for (auto& d : delayA) d.assign ((size_t) lookahead, 0.0f);
            for (auto& d : delayB) d.assign ((size_t) lookahead, 0.0f);
            reset();
        }

        void reset()
        {
            for (auto& d : delayA) std::fill (d.begin(), d.end(), 0.0f);
            for (auto& d : delayB) std::fill (d.begin(), d.end(), 0.0f);
            broadband.reset();
            for (auto& g : regionGain) g.reset();
            for (auto& c : analysis) for (auto& s : c) s.reset();
            for (auto& c : cutters) for (auto& s : c) s.reset();
            posA = posB = 0;
            reductionDb = 0.0f;
            regionCutDb = {};
        }

        int getLatencySamples() const noexcept { return 2 * lookahead; }
        float getReductionDb() const noexcept  { return reductionDb; }   // broadband, deepest in the last block
        const std::array<float, numRegions>& getRegionCutDb() const noexcept { return regionCutDb; }   // spectral, now

        void process (float* const* ch, int numChannels, int numSamples) noexcept
        {
            const int chans = std::min (2, numChannels);
            if (chans <= 0)
                return;
            float deepest = 1.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                // --- 1. spectral: when a peak is coming, how much of which region takes it back under.
                // The analysis runs the very filters the cut will use, on the same audio, so the region's
                // part of this sample is known exactly: cutting it by (1 - g) moves the sample from x to
                // x - (1 - g) v. Solve for g, per channel, on the region pushing hardest in the peak's
                // direction; the broadband stage takes whatever that cannot fix.
                std::array<float, numRegions> needed { 1.0f, 1.0f, 1.0f, 1.0f };
                for (int c = 0; c < chans; ++c)
                {
                    const float x = ch[c][i];
                    std::array<float, numRegions> v {};
                    for (int r = 0; r < numRegions; ++r)
                    {
                        const auto o = analysis[(size_t) c][(size_t) r].process (regionCoeffs[(size_t) r], x);
                        v[(size_t) r] = r == 0 ? o.low : r == 3 ? o.high : o.band;
                    }
                    const float a = std::abs (x);
                    if (a <= ceiling)
                        continue;

                    int top = -1;
                    float push = 0.0f;
                    for (int r = 0; r < numRegions; ++r)
                    {
                        const float along = v[(size_t) r] * std::copysign (1.0f, x);   // its part in the peak's direction
                        if (along > push) { push = along; top = r; }
                    }
                    if (top < 0 || push < 0.25f * (a - ceiling))
                        continue;   // no one region is carrying it: broadband

                    const float g = std::max (0.25f, 1.0f - (a - ceiling) / push);   // at most 12 dB
                    needed[(size_t) top] = std::min (needed[(size_t) top], g);
                }

                std::array<float, numRegions> gains {};
                bool anyCut = false;
                for (int r = 0; r < numRegions; ++r)
                {
                    gains[(size_t) r] = regionGain[(size_t) r].push (needed[(size_t) r]);
                    anyCut = anyCut || gains[(size_t) r] < 0.9999f;
                }

                // Delay line A: the audio the spectral gains were worked out for, 1.5 ms later
                float stageA[2] {};
                for (int c = 0; c < chans; ++c)
                {
                    auto& d = delayA[(size_t) c];
                    const float x = d[(size_t) posA];
                    d[(size_t) posA] = ch[c][i];

                    // The cutters always run (their state stays current, so a cut starts cleanly)
                    float y = x;
                    for (int r = 0; r < numRegions; ++r)
                    {
                        const auto o = cutters[(size_t) c][(size_t) r].process (regionCoeffs[(size_t) r], x);
                        if (anyCut)
                            y += (gains[(size_t) r] - 1.0f) * (r == 0 ? o.low : r == 3 ? o.high : o.band);
                    }
                    stageA[c] = y;
                }
                posA = (posA + 1) % lookahead;

                // --- 2. broadband, on what the spectral stage let through
                const float peakA = chans == 2 ? std::max (std::abs (stageA[0]), std::abs (stageA[1])) : std::abs (stageA[0]);
                const float g = broadband.push (peakA > ceiling ? ceiling / peakA : 1.0f);
                deepest = std::min (deepest, g);

                for (int c = 0; c < chans; ++c)
                {
                    auto& d = delayB[(size_t) c];
                    const float out = d[(size_t) posB] * g;
                    d[(size_t) posB] = stageA[c];
                    ch[c][i] = std::clamp (out, -ceiling, ceiling);   // guard only: the gain already holds it here
                }
                posB = (posB + 1) % lookahead;
            }

            reductionDb = -20.0f * std::log10 (std::max (1.0e-3f, deepest));
            for (int r = 0; r < numRegions; ++r)
                regionCutDb[(size_t) r] = -20.0f * std::log10 (std::max (1.0e-3f, regionGain[(size_t) r].current()));
        }

    private:
        double sr = 48000.0;
        int lookahead = 72, posA = 0, posB = 0;
        LookaheadGain broadband;
        std::array<LookaheadGain, numRegions> regionGain;
        std::array<SvfCoeffs, numRegions> regionCoeffs {};
        std::array<std::array<SvfState, numRegions>, 2> analysis {};
        std::array<std::array<SvfState, numRegions>, 2> cutters {};
        std::array<std::vector<float>, 2> delayA, delayB;
        float reductionDb = 0.0f;
        std::array<float, numRegions> regionCutDb {};
        float ceiling = 1.0f;     // 0 dBFS: it acts only on real overs (CEILING can set it lower)
    };
}
