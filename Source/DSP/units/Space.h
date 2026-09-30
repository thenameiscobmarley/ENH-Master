#pragma once

#include "RackUnit.h"
#include "TimeFx.h"   // (Rng)

/*  The newer units for image, phase and tone: SHUFFLER, PHASE ROTATOR, ROBOVOX, SUBMAXX. */
namespace enh::dsp::units
{
    // ------------------------------------------------------------------------------------------------
    /** SHUFFLER: Alan Blumlein's stereo shuffling - the sides' low end widened more than their top, because
        below ~700 Hz our ears judge direction by phase and the image narrows there. The sides split at
        SHUFFLE: LOW WIDTH below, WIDTH above; below BASS MONO the sides are taken out (a steady low end);
        MID and SIDE: their levels. */
    class Shuffler final : public RackUnit
    {
        BiquadState lpA, lpB, hpA, hpB; BiquadCoeffs lpC, hpC; float curF = -1.0f, curM = -1.0f;
        void prepareUnit (double, int) override {}
        void resetUnit() override { lpA = lpB = hpA = hpB = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float width = std::clamp (p[1], 0.0f, 200.0f) / 100.0f, f = std::clamp (p[2], 200.0f, 3000.0f);
            const float lowW = std::clamp (p[3], 0.0f, 200.0f) / 100.0f, mono = std::clamp (p[4], 20.0f, 300.0f);
            const float mid = dbToGainF (std::clamp (p[5], -6.0f, 6.0f)), side = dbToGainF (std::clamp (p[6], -6.0f, 6.0f));
            if (f != curF) { curF = f; lpC = BiquadCoeffs::lowPass (sr, f, 0.5); }
            if (mono != curM) { curM = mono; hpC = BiquadCoeffs::highPass (sr, mono, 0.707); }
            for (int s = 0; s < n; ++s)
            {
                const float m = 0.5f * (io[0][s] + io[1][s]), sd = 0.5f * (io[0][s] - io[1][s]);
                const float low = lpB.process (lpC, lpA.process (lpC, sd)), high = sd - low;
                float s2 = low * lowW + high * width;
                s2 = hpB.process (hpC, hpA.process (hpC, s2));
                const float mo = m * mid, so = s2 * side;
                io[0][s] = mo + so; io[1][s] = mo - so;
            }
            setMeter (0.5f * (width + lowW) / 2.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** PHASE ROTATOR: speech and many instruments are lopsided - their peaks stand much higher on one side
        than the other. A chain of allpasses (4, 8 or 16) turns the phase of each frequency differently, which
        evens the waveform out: the same sound, the same spectrum, a few dB less peak (headroom, as broadcast
        processors do). AMOUNT slides the chain's turning point up out of hearing at 0 (no comb filtering: it
        is never mixed with the dry sound). The display: the crest it has taken off. */
    class PhaseRotator final : public RackUnit
    {
        std::array<std::array<BiquadState, 16>, 2> st {}; BiquadCoeffs c; float curF = -1.0f; float pkIn = 0.0f, pkOut = 0.0f;
        void prepareUnit (double, int) override {}
        void resetUnit() override { st = {}; pkIn = pkOut = 0.0f; }
        static BiquadCoeffs allpass (double sr, double f, double q)
        {
            const double w = 2.0 * 3.141592653589793 * f / sr, a = std::sin (w) / (2.0 * q), cw = std::cos (w);
            return BiquadCoeffs::normalised (1.0 - a, -2.0 * cw, 1.0 + a, 1.0 + a, -2.0 * cw, 1.0 - a);
        }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            static constexpr int stages[3] { 4, 8, 16 };
            const int ns = stages[std::clamp ((int) std::lround (p[1]), 0, 2)];
            const float amount = std::clamp (p[3], 0.0f, 100.0f) / 100.0f;
            const float f = std::min (0.45f * (float) sr, std::clamp (p[2], 60.0f, 2000.0f) * (1.0f + (1.0f - amount) * 40.0f));
            if (std::abs (f - curF) > 0.5f) { curF = f; c = allpass (sr, f, 0.7); }
            const float fall = std::exp (-(float) n / (0.5f * (float) sr));
            pkIn *= fall; pkOut *= fall;
            for (int s = 0; s < n; ++s)
                for (int ch = 0; ch < 2; ++ch)
                {
                    float v = io[ch][s];
                    pkIn = std::max (pkIn, std::abs (v));
                    for (int k = 0; k < ns; ++k) v = st[(size_t) ch][(size_t) k].process (c, v);
                    pkOut = std::max (pkOut, std::abs (v));
                    io[ch][s] = v;
                }
            setMeter (pkIn > 1.0e-4f ? std::max (0.0f, 20.0f * std::log10 (pkIn / std::max (1.0e-6f, pkOut))) / 6.0f : 0.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** ROBOVOX: a 16-band vocoder. The sound (the modulator) goes through 16 band-passes spaced evenly in
        pitch from 100 Hz to 8 kHz; each band's level (RELEASE: how fast it lets go) opens the same band of a
        synthesized carrier - a saw, a pulse, noise, or a chord (root, fifth, octave) at NOTE. FORMANT slides
        the carrier's bands up or down against the voice's: bigger or smaller "throat". */
    class Robovox final : public RackUnit
    {
        static constexpr int B = 16;
        std::array<BiquadState, B> mod {}; std::array<std::array<BiquadState, 2>, B> car {}; std::array<float, B> env {};
        std::array<BiquadCoeffs, B> modC {}, carC {}; std::array<double, 3> ph {}; Rng rng; float curFormant = 999.0f;
        void prepareUnit (double s, int) override
        {
            for (int b = 0; b < B; ++b) modC[(size_t) b] = BiquadCoeffs::bandPass (s, 100.0 * std::pow (80.0, b / (double) (B - 1)), 6.0);
            curFormant = 999.0f;
        }
        void resetUnit() override { mod = {}; car = {}; env = {}; ph = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float note = std::clamp (p[1], 24.0f, 72.0f); const int kind = std::clamp ((int) std::lround (p[2]), 0, 3);
            const float formant = std::clamp (p[3], -12.0f, 12.0f), release = std::clamp (p[4], 5.0f, 200.0f) * 0.001f, mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            if (formant != curFormant)
            {
                curFormant = formant;
                for (int b = 0; b < B; ++b) carC[(size_t) b] = BiquadCoeffs::bandPass (sr, std::min (0.45 * sr, 100.0 * std::pow (80.0, b / (double) (B - 1)) * std::pow (2.0, formant / 12.0)), 6.0);
            }
            const double f0 = 440.0 * std::pow (2.0, (note - 69.0) / 12.0);
            const float att = 1.0f - std::exp (-1.0f / (0.002f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (release * (float) sr));
            float act = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                const float m = 0.5f * (io[0][s] + io[1][s]);
                // the carrier
                float c = 0.0f;
                const double mult[3] { 1.0, 1.4983, 2.0 };
                for (int v = 0; v < (kind == 3 ? 3 : 1); ++v)
                {
                    ph[(size_t) v] += f0 * mult[v] / sr; ph[(size_t) v] -= std::floor (ph[(size_t) v]);
                    const float saw = (float) (2.0 * ph[(size_t) v] - 1.0);
                    c += kind == 1 ? (ph[(size_t) v] < 0.5 ? 0.8f : -0.8f) : kind == 2 ? 0.0f : saw * 0.8f;
                }
                if (kind == 2) c = (rng.next() - 0.5f) * 1.6f;
                float l = 0.0f, r = 0.0f;
                for (int b = 0; b < B; ++b)
                {
                    const float v = std::abs (mod[(size_t) b].process (modC[(size_t) b], m));
                    env[(size_t) b] += (v > env[(size_t) b] ? att : rel) * (v - env[(size_t) b]);
                    const float cb = car[(size_t) b][0].process (carC[(size_t) b], c);
                    const float y = cb * env[(size_t) b] * 6.0f;
                    if (b & 1) r += y; else l += y;   // (alternate bands left and right: a little width)
                    act += env[(size_t) b];
                }
                io[0][s] += mix * ((l + 0.4f * r) - io[0][s]);
                io[1][s] += mix * ((r + 0.4f * l) - io[1][s]);
            }
            setMeter (act / (float) (n * B) * 20.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SUBMAXX: bass you can hear on small speakers and earbuds that can't play it. Our ears rebuild a
        fundamental from its harmonics (the "missing fundamental"), so the lows below FREQUENCY are turned
        into their harmonics (a full-wave rectifier and a gentle curve, driven against their own level so the
        amount follows HARMONICS, not the volume), kept to the 2nd - 5th, and added; the lows themselves can
        be turned down (ORIGINAL) to save the small speaker's excursion. */
    class SubMaxx final : public RackUnit
    {
        std::array<BiquadState, 2> lpA {}, lpB {}, hpA {}, hpB {}, bpA {}; BiquadCoeffs lpC, hpC, bpC; kit::Level lvl; float curF = -1.0f;
        void prepareUnit (double s, int) override { lvl.setup (s, 0.1); }
        void resetUnit() override { lpA = lpB = hpA = hpB = bpA = {}; lvl.reset(); }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float f = std::clamp (p[1], 40.0f, 150.0f), amount = std::clamp (p[2], 0.0f, 10.0f) / 10.0f;
            const float orig = dbToGainF (std::clamp (p[3], -24.0f, 0.0f)), out = dbToGainF (std::clamp (p[4], -12.0f, 6.0f));
            if (f != curF) { curF = f; lpC = BiquadCoeffs::lowPass (sr, f * 1.2, 0.707); hpC = BiquadCoeffs::highPass (sr, f * 1.5, 0.707); bpC = BiquadCoeffs::lowPass (sr, f * 6.0, 0.707); }
            float hsum = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                const float m = 0.5f * (io[0][s] + io[1][s]);
                const float low = lpB[0].process (lpC, lpA[0].process (lpC, m));
                const float l = std::max (1.0e-5f, lvl.process (low));
                const float u = low / l;                       // the lows, at a steady level
                const float g = (std::abs (u) + 0.3f * u * u * u) * l;   // their harmonics (even from |x|, odd from x^3)
                const float h = bpA[0].process (bpC, hpB[0].process (hpC, hpA[0].process (hpC, g))) * amount * 1.6f;
                hsum += std::abs (h);
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][s];
                    io[c][s] = (x - low * (1.0f - orig) + h) * out;
                }
            }
            setMeter (hsum / (float) n * 8.0f);
        }
    };
}
