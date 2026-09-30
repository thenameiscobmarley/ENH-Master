#pragma once

#include "RackUnit.h"

/*  DE-HARSH: the dynamic harsh remover. */
namespace enh::dsp::units
{
    /** DE-HARSH. Units that keep adapting (compressors, AUTO, adaptive EQs) move the level many times a second;
        each fast move puts little sidebands around every tone, and in the 2 - 9 kHz presence region, where our
        ears are most sensitive, that flicker is heard as harshness even when the balance is right.
        Four bands over the presence region (FOCUS slides them, half an octave apart) are each watched through
        a band-pass and turned down or up by a bell of their own (the four in a row, their gains set every 32
        samples); each band's gain is set by two things:
          - SMOOTH: the band's fast level (1.5 ms) against its own average (150 ms, in dB). Where it flickers
            above the average it is taken down, below it lifted back (by at most half of DEPTH) - the flicker is
            evened out, the average left alone;
          - DEPTH and SENSE: the band's slow level against the mids (0.5 - 1.5 kHz). When the band stands out
            (SENSE: how far it may), it is taken down - a de-harsher that only acts on harsh moments.
        The harsh cut moves slowly (5 ms down, 60 ms back), so it never adds flicker of its own. It cuts by at
        most DEPTH (0 - 12 dB). LISTEN plays what it takes away. AIR softens above
        10 kHz by the same amount, a little (sibilant fizz rides along with harshness). */
    class DeHarsh final : public RackUnit
    {
        static constexpr int B = 4;
        std::array<std::array<BiquadState, 2>, B> band {};
        std::array<BiquadCoeffs, B> bc {};
        std::array<BiquadState, 2> midA {}; BiquadCoeffs midC;
        std::array<BiquadState, 2> airS {}; BiquadCoeffs airC;
        std::array<std::array<BiquadState, 2>, B> bell {}; std::array<BiquadCoeffs, B> bellC {}; std::array<float, B> bellDb {}; int tick = 0;
        std::array<float, B> fastPow {}, slowPow {}, avgDb {}, harshDb {}, flickDb {};
        float midSlow = 0.0f, airDb = 0.0f, curFocus = -1.0f;
        float aF = 0, aS = 0, gDown = 0, gUp = 0, gFl = 0;
        static constexpr double at[B] { 0.6, 0.85, 1.2, 1.7 };   // around FOCUS, half an octave apart
        static float coef (double sr, double s) { return 1.0f - (float) std::exp (-1.0 / (s * sr)); }
        void prepareUnit (double s, int) override
        {
            aF = coef (s, 0.0015); aS = coef (s, 0.150);
            gDown = coef (s, 0.005); gUp = coef (s, 0.060); gFl = coef (s, 0.001);
            midC = BiquadCoeffs::bandPass (s, 900.0, 0.9);
            airC = BiquadCoeffs::highPass (s, 10000.0, 0.707);
            curFocus = -1.0f;
        }
        void resetUnit() override
        {
            band = {}; midA = {}; airS = {}; bell = {}; bellDb.fill (99.0f); tick = 0; fastPow = {}; slowPow = {}; harshDb = {}; flickDb = {};
            avgDb.fill (-120.0f); midSlow = 0.0f; airDb = 0.0f;
        }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float smooth = 0.09f * std::clamp (p[1], 0.0f, 10.0f);   // how much of the flicker is evened out (0 .. 90%)
            const float depth = std::clamp (p[2], 0.0f, 12.0f);
            const float sense = std::clamp (p[3], 0.0f, 10.0f);
            const float focus = std::clamp (p[4], 2000.0f, 8000.0f);
            const float air = std::clamp (p[5], 0.0f, 10.0f) / 10.0f;
            const bool listen = p[6] > 0.5f;
            const float mix = std::clamp (p[7], 0.0f, 100.0f) / 100.0f;
            if (focus != curFocus)
            {
                curFocus = focus; bellDb.fill (99.0f); tick = 0;
                for (int b = 0; b < B; ++b) bc[(size_t) b] = BiquadCoeffs::bandPass (sr, std::min (0.45 * sr, focus * at[b]), 3.0);
            }
            // how far a band may stand above the mids before it counts as harsh: SENSE 0 lets +6 dB by, 10 only -12
            const float allowDb = 6.0f - 1.8f * sense;
            float took = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                const float x0 = io[0][s], x1 = io[1][s], m = 0.5f * (x0 + x1);
                const float md = midA[0].process (midC, m);
                midSlow += aS * (md * md - midSlow);
                const float midDb = 10.0f * std::log10 (midSlow + 1.0e-12f);
                float cutSum = 0.0f; const bool retune = (tick++ & 31) == 0;
                for (int b = 0; b < B; ++b)
                {
                    auto& st = band[(size_t) b]; const auto& c = bc[(size_t) b];
                    const float b0 = st[0].process (c, x0), b1 = st[1].process (c, x1);
                    const float e = 0.5f * (b0 * b0 + b1 * b1);
                    float& fp = fastPow[(size_t) b]; fp += aF * (e - fp);
                    float& sp = slowPow[(size_t) b]; sp += aS * (e - sp);
                    const float fastDb = 10.0f * std::log10 (fp + 1.0e-12f), slowDb = 10.0f * std::log10 (sp + 1.0e-12f);
                    float& av = avgDb[(size_t) b]; av += aS * (fastDb - av);
                    // flicker: toward its own average, both ways (only where the band has something in it)
                    float fl = slowDb > -70.0f ? smooth * (av - fastDb) : 0.0f;
                    fl = std::clamp (fl, -depth, 0.5f * depth);
                    float& f = flickDb[(size_t) b]; f += gFl * (fl - f);
                    // harsh: standing out over the mids (0.7: the bells overlap, a tone between two gets about DEPTH)
                    const float over = slowDb - midDb - allowDb;
                    const float hw = slowDb > -70.0f && over > 0.0f ? -0.7f * std::min (depth, 0.7f * over) : 0.0f;
                    float& h = harshDb[(size_t) b]; h += (hw < h ? gDown : gUp) * (hw - h);
                    const float g = std::clamp (f + h, -depth, 0.5f * depth);
                    if (retune && std::abs (g - bellDb[(size_t) b]) > 0.01f)
                    {
                        bellDb[(size_t) b] = g;
                        bellC[(size_t) b] = BiquadCoeffs::peaking (sr, std::min (0.45 * sr, curFocus * at[b]), 2.0, g);
                    }
                    cutSum += std::min (0.0f, g);
                }
                float y0 = x0, y1 = x1;
                for (int b = 0; b < B; ++b) { y0 = bell[(size_t) b][0].process (bellC[(size_t) b], y0); y1 = bell[(size_t) b][1].process (bellC[(size_t) b], y1); }
                // AIR: above 10 kHz follows the bands' average cut, scaled
                const float airWant = air * 0.5f * cutSum / (float) B;
                airDb += (airWant < airDb ? gDown : gUp) * (airWant - airDb);
                const float airLin = dbToGainF (airDb) - 1.0f;
                y0 += airLin * airS[0].process (airC, x0);
                y1 += airLin * airS[1].process (airC, x1);
                if (listen) { y0 = x0 - y0; y1 = x1 - y1; }
                io[0][s] = x0 + mix * (y0 - x0);
                io[1][s] = x1 + mix * (y1 - x1);
                took = std::min (took, cutSum / (float) B);
            }
            setMeter (depth > 0.0f ? std::clamp (-took / std::max (3.0f, depth), 0.0f, 1.0f) : 0.0f);
        }
    };
}
