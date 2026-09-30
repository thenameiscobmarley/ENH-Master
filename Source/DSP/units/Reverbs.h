#pragma once

#include "RackUnit.h"

/*  The newer units' reverbs: SHIMMER (an 8-line feedback delay network with a pitch shifter in its loop),
    PLATE 140 (Dattorro's plate), SPRING TANK (dispersive springs: chains of allpasses in feedback loops). */
namespace enh::dsp::units
{
    /** A pitch shifter by two read heads crossfading over a moving window (no latency beyond the window). */
    struct PitchShifter
    {
        DelayLine d; float phase = 0.0f; int window = 2400;
        void setup (double sr, double windowMs) { window = std::max (64, (int) (sr * windowMs / 1000.0)); d.setMax (window * 2 + 8); }
        void clear() { d.clear(); phase = 0.0f; }
        float process (float x, float ratio) noexcept
        {
            d.push (x);
            phase += (1.0f - ratio) / (float) window;
            phase -= std::floor (phase);
            float y = 0.0f;
            for (int k = 0; k < 2; ++k)
            {
                float ph = phase + 0.5f * (float) k; ph -= std::floor (ph);
                y += std::sin (3.14159265f * ph) * d.tap (1.0f + ph * (float) window);
            }
            return y * 0.75f;
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SHIMMER: eight delay lines mixed by a Hadamard matrix (every line feeds every other, energy kept),
        each damped (DAMP) and set to fall 60 dB in DECAY; their lengths scale with SIZE and wander (MOD) so
        it never rings metallic. The tank's output, pitched up an octave (or +7, +19, -12), is fed back in
        (SHIMMER): each pass climbs - the choir-like tail. */
    class Shimmer final : public RackUnit
    {
        static constexpr int N = 8;
        std::array<DelayLine, N> lines; std::array<OnePole, N> damp; std::array<Lfo, N> lfo;
        PitchShifter shift; DelayLine pre; float shimmerFb = 0.0f;
        static constexpr float baseMs[N] { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.7f, 73.1f };
        void prepareUnit (double s, int) override
        {
            for (auto& l : lines) l.setMax ((int) (s * 0.160));
            shift.setup (s, 70.0); pre.setMax ((int) (s * 0.05));
            for (int i = 0; i < N; ++i) lfo[(size_t) i].ph = 0.13 * i;
        }
        void resetUnit() override { for (auto& l : lines) l.clear(); for (auto& d : damp) d.z = 0; shift.clear(); pre.clear(); shimmerFb = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float size = 0.5f + 0.12f * std::clamp (p[1], 0.0f, 10.0f), decay = std::clamp (p[2], 0.3f, 20.0f);
            const float shimmer = std::clamp (p[3], 0.0f, 100.0f) / 100.0f;
            static constexpr float semis[4] { 12.0f, 7.0f, 19.0f, -12.0f };
            const float ratio = std::pow (2.0f, semis[std::clamp ((int) std::lround (p[4]), 0, 3)] / 12.0f);
            const float dk = onePoleK (sr, 16000.0 * std::pow (0.125, std::clamp (p[5], 0.0f, 10.0f) / 10.0));
            const float mod = std::clamp (p[6], 0.0f, 10.0f) / 10.0f, mix = std::clamp (p[7], 0.0f, 100.0f) / 100.0f;
            std::array<float, N> len {}, g {};
            for (int i = 0; i < N; ++i) { len[(size_t) i] = baseMs[i] * size * 0.001f * (float) sr; g[(size_t) i] = std::pow (10.0f, -3.0f * len[(size_t) i] / ((float) sr * decay)); }
            float wetPow = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                const float in = 0.5f * (io[0][s] + io[1][s]);
                std::array<float, N> o {};
                for (int i = 0; i < N; ++i)
                {
                    const float m = mod * (0.0008f * (float) sr) * lfo[(size_t) i].next (0.07 + 0.031 * i, sr);
                    o[(size_t) i] = damp[(size_t) i].process (lines[(size_t) i].tap (len[(size_t) i] + m), dk) * g[(size_t) i];
                }
                // Hadamard 8 (fast, scaled to keep energy)
                for (int h = 1; h < N; h <<= 1)
                    for (int i = 0; i < N; i += h << 1)
                        for (int j = i; j < i + h; ++j) { const float a = o[(size_t) j], b = o[(size_t) (j + h)]; o[(size_t) j] = a + b; o[(size_t) (j + h)] = a - b; }
                const float wl = 0.35355339f * (o[0] + o[2] + o[4] + o[6]), wr = 0.35355339f * (o[1] + o[3] + o[5] + o[7]);
                const float sh = shift.process (0.5f * (wl + wr), ratio);
                shimmerFb = sh * shimmer * 0.6f;
                const float drive = in + shimmerFb;
                for (int i = 0; i < N; ++i) lines[(size_t) i].push (0.35355339f * o[(size_t) i] + ((i & 1) ? -0.25f : 0.25f) * drive);
                const float l = wl * 0.7f, r = wr * 0.7f;
                wetPow += l * l + r * r;
                io[0][s] += mix * (l - io[0][s] * 0.35f);
                io[1][s] += mix * (r - io[1][s] * 0.35f);
            }
            setMeter (std::sqrt (wetPow / (float) (2 * n)) * 6.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** PLATE 140: Jon Dattorro's plate (1997): pre-delay, a band-limit, four diffusing allpasses, then the
        figure-eight tank - two halves, each a modulated allpass, a delay, damping, a second allpass and a
        delay - read out at fourteen taps. SIZE scales every length; DECAY sets the tank's gain. */
    class Plate final : public RackUnit
    {
        struct AP { DelayLine d; float len = 1; float process (float x, float g, float mod = 0.0f) noexcept { const float v = d.tap (len + mod); const float w = x + g * v; d.push (w); return v - g * w; } };
        DelayLine pre; OnePole bw; std::array<AP, 4> diff; std::array<AP, 2> modAp, ap2; std::array<DelayLine, 2> d1, d2; std::array<OnePole, 2> dampF;
        Lfo lfo; float scaleSr = 1.0f, fbL = 0.0f, fbR = 0.0f, curSize = -1.0f;
        void lengths (float size)
        {
            const float k = scaleSr * (0.6f + 0.06f * size);
            const float dl[4] { 142, 107, 379, 277 };
            for (int i = 0; i < 4; ++i) diff[(size_t) i].len = dl[i] * k;
            modAp[0].len = 672 * k; modAp[1].len = 908 * k; ap2[0].len = 1800 * k; ap2[1].len = 2656 * k;
            curSize = size;
        }
        float lenD1[2] {}, lenD2[2] {};
        void prepareUnit (double s, int) override
        {
            scaleSr = (float) (s / 29761.0);
            pre.setMax ((int) (s * 0.13));
            for (auto& a : diff) a.d.setMax ((int) (400 * scaleSr * 1.3f) + 8);
            for (auto& a : modAp) a.d.setMax ((int) (1000 * scaleSr * 1.3f) + 64);
            for (auto& a : ap2) a.d.setMax ((int) (2700 * scaleSr * 1.3f) + 8);
            for (auto& d : d1) d.setMax ((int) (4500 * scaleSr * 1.3f) + 8);
            for (auto& d : d2) d.setMax ((int) (3800 * scaleSr * 1.3f) + 8);
            lengths (6.0f);
        }
        void resetUnit() override
        {
            pre.clear(); bw.z = 0; for (auto& a : diff) a.d.clear(); for (auto& a : modAp) a.d.clear(); for (auto& a : ap2) a.d.clear();
            for (auto& d : d1) d.clear(); for (auto& d : d2) d.clear(); dampF[0].z = dampF[1].z = 0; fbL = fbR = 0;
        }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float size = std::clamp (p[4], 0.0f, 10.0f);
            if (std::abs (size - curSize) > 0.05f) lengths (size);
            const float k = scaleSr * (0.6f + 0.06f * size);
            lenD1[0] = 4453 * k; lenD1[1] = 4217 * k; lenD2[0] = 3720 * k; lenD2[1] = 3163 * k;
            const float loop = (lenD1[0] + lenD2[0] + modAp[0].len + ap2[0].len) / (float) sr;
            const float decay = std::clamp (std::pow (10.0f, -3.0f * loop / std::clamp (p[1], 0.2f, 8.0f)), 0.0f, 0.97f);
            const float preS = std::clamp (p[2], 0.0f, 120.0f) * 0.001f * (float) sr;
            const float dampK = onePoleK (sr, 14000.0 * std::pow (0.12, std::clamp (p[3], 0.0f, 10.0f) / 10.0));
            const float mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f, bwK = onePoleK (sr, 9000.0);
            float wp = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                pre.push (0.5f * (io[0][s] + io[1][s]));
                float x = bw.process (pre.tap (std::max (1.0f, preS)), bwK);
                x = diff[0].process (x, 0.75f); x = diff[1].process (x, 0.75f); x = diff[2].process (x, 0.625f); x = diff[3].process (x, 0.625f);
                const float m = 8.0f * scaleSr * lfo.next (0.9, sr);
                float a = modAp[0].process (x + fbR * decay, -0.7f, m);
                d1[0].push (a); a = dampF[0].process (d1[0].tap (lenD1[0]), dampK) * decay; a = ap2[0].process (a, 0.5f); d2[0].push (a);
                float b = modAp[1].process (x + fbL * decay, -0.7f, -m);
                d1[1].push (b); b = dampF[1].process (d1[1].tap (lenD1[1]), dampK) * decay; b = ap2[1].process (b, 0.5f); d2[1].push (b);
                fbL = d2[0].tap (lenD2[0]); fbR = d2[1].tap (lenD2[1]);
                const float l = 0.6f * (d1[1].tap (266 * k) + d1[1].tap (2974 * k) - d2[1].tap (1913 * k + 1) + d1[0].tap (1990 * k) - d2[0].tap (187 * k + 1));
                const float r = 0.6f * (d1[0].tap (353 * k) + d1[0].tap (3627 * k) - d2[0].tap (1228 * k + 1) + d1[1].tap (2673 * k) - d2[1].tap (335 * k + 1));
                wp += l * l + r * r;
                io[0][s] += mix * (l - io[0][s] * 0.35f);
                io[1][s] += mix * (r - io[1][s] * 0.35f);
            }
            setMeter (std::sqrt (wp / (float) (2 * n)) * 6.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SPRING TANK: a real spring disperses - its highs arrive before its lows, so each echo is a chirp
        ("boing"). Each spring is a feedback loop: a delay (the spring's length) and a chain of 24 allpasses
        (the dispersion), a tone filter, and a soft saturation at its input (DWELL drives it). 1 - 3 springs
        of different lengths, panned apart. */
    class Spring final : public RackUnit
    {
        static constexpr int S = 3, AP = 24;
        struct One { DelayLine d; std::array<Allpass1, AP> ap; OnePole tone; float fb = 0.0f; float len = 1.0f; };
        std::array<One, S> sp; OnePole inLp;
        void prepareUnit (double s, int) override
        {
            const float ms[S] { 33.0f, 41.0f, 47.0f };
            for (int i = 0; i < S; ++i) { sp[(size_t) i].d.setMax ((int) (s * 0.06)); sp[(size_t) i].len = ms[i] * 0.001f * (float) s; }
        }
        void resetUnit() override { for (auto& o : sp) { o.d.clear(); for (auto& a : o.ap) a = {}; o.tone.z = 0; o.fb = 0; } inLp.z = 0; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float dwell = 1.0f + 3.0f * std::clamp (p[1], 0.0f, 10.0f) / 10.0f;
            const float fb = 0.55f + 0.38f * std::clamp (p[2], 0.0f, 10.0f) / 10.0f;
            const float toneK = onePoleK (sr, 1500.0 * std::pow (5.0, std::clamp (p[3], 0.0f, 10.0f) / 10.0));
            const int springs = std::clamp ((int) std::lround (p[4]) + 1, 1, S);
            const float mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f, g = 0.62f, inK = onePoleK (sr, 4500.0);
            float wp = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                const float in = std::tanh (dwell * inLp.process (0.5f * (io[0][s] + io[1][s]), inK)) / dwell;
                float l = 0.0f, r = 0.0f;
                for (int i = 0; i < springs; ++i)
                {
                    auto& o = sp[(size_t) i];
                    float v = o.d.tap (o.len);
                    for (auto& a : o.ap) v = a.process (v, g);
                    v = o.tone.process (v, toneK);
                    o.d.push (in + fb * v);
                    const float pan = springs == 1 ? 0.5f : (float) i / (float) (springs - 1);
                    l += v * (1.0f - pan); r += v * pan;
                }
                const float k = 1.4f / (float) springs;
                l *= k; r *= k;
                wp += l * l + r * r;
                io[0][s] += mix * (l - io[0][s] * 0.3f);
                io[1][s] += mix * (r - io[1][s] * 0.3f);
            }
            setMeter (std::sqrt (wp / (float) (2 * n)) * 6.0f);
        }
    };
}
