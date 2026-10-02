#pragma once

#include "RackUnit.h"
#include "Reverbs.h"   // (PitchShifter)

/*  The newer units that play with time and pitch: GRAIN CLOUD, HARMONIZER, BBD ENSEMBLE, BODE SHIFTER. */
namespace enh::dsp::units
{
    /** A tiny deterministic random (the same every run: tests can compare). */
    struct Rng { unsigned s = 0x9e3779b9u; float next() noexcept { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 16777216.0f; } };

    // ------------------------------------------------------------------------------------------------
    /** GRAIN CLOUD: the sound written into a 2.5 s memory; grains (SIZE long, DENSITY a second) read back
        from a little in the past, each windowed (Hann), pitched (PITCH, plus SPRAY's random detune and
        position scatter), sometimes backwards (REVERSE), panned at random. FEEDBACK writes the cloud back
        into the memory: it thickens into a texture. */
    class GrainCloud final : public RackUnit
    {
        static constexpr int maxGrains = 32;
        struct Grain { bool on = false; float pos = 0, step = 1, len = 1, age = 0, panL = 1, panR = 1; };
        std::array<DelayLine, 2> mem; std::array<Grain, maxGrains> grains; Rng rng; float untilNext = 0.0f; std::array<float, 2> fb {};
        void prepareUnit (double s, int) override { for (auto& m : mem) m.setMax ((int) (s * 2.6)); }
        void resetUnit() override { for (auto& m : mem) m.clear(); for (auto& g : grains) g.on = false; untilNext = 0; fb = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float size = std::clamp (p[1], 10.0f, 500.0f) * 0.001f * (float) sr, density = std::clamp (p[2], 1.0f, 40.0f);
            const float pitch = std::clamp (p[3], -12.0f, 12.0f), spray = std::clamp (p[4], 0.0f, 100.0f) / 100.0f;
            const float reverse = std::clamp (p[5], 0.0f, 100.0f) / 100.0f, feedback = std::clamp (p[6], 0.0f, 90.0f) / 100.0f;
            const float mix = std::clamp (p[7], 0.0f, 100.0f) / 100.0f;
            int active = 0;
            for (int s = 0; s < n; ++s)
            {
                mem[0].push (io[0][s] + feedback * fb[0]);
                mem[1].push (io[1][s] + feedback * fb[1]);
                if ((untilNext -= 1.0f) <= 0.0f)
                {
                    untilNext = (float) sr / density * (0.5f + rng.next());
                    for (auto& g : grains) if (! g.on)
                    {
                        const float semis = pitch + spray * (rng.next() - 0.5f) * 1.0f;
                        const float ratio = std::pow (2.0f, semis / 12.0f), back = rng.next() < reverse ? -1.0f : 1.0f;
                        g.on = true; g.len = size * (0.7f + 0.6f * rng.next()); g.age = 0;
                        g.step = ratio * back;
                        // Start far enough back that the grain never reads ahead of what was written
                        g.pos = g.len * std::max (1.0f, ratio) + 16.0f + spray * rng.next () * 0.8f * (float) sr;
                        if (back < 0.0f) g.pos = 16.0f + spray * rng.next() * 0.8f * (float) sr;
                        const float pan = 0.5f + (rng.next() - 0.5f) * (0.3f + 0.7f * spray);
                        g.panL = std::cos (1.5707963f * pan) * 1.41f; g.panR = std::sin (1.5707963f * pan) * 1.41f;
                        break;
                    }
                }
                float l = 0.0f, r = 0.0f;
                for (auto& g : grains)
                {
                    if (! g.on) continue;
                    ++active;
                    const float w = 0.5f - 0.5f * std::cos (6.2831853f * g.age / g.len);
                    const float d = std::clamp (g.pos, 2.0f, 2.5f * (float) sr);
                    l += w * g.panL * mem[0].tap (d);
                    r += w * g.panR * mem[1].tap (d);
                    g.pos += 1.0f - g.step;   // (the read point moves by its pitch; the memory moves on by one)
                    if ((g.age += 1.0f) >= g.len) g.on = false;
                }
                const float k = 1.0f / std::sqrt (1.0f + density * size / (float) sr);
                l *= k; r *= k;
                fb = { l, r };
                io[0][s] += mix * (l - io[0][s]);
                io[1][s] += mix * (r - io[1][s]);
            }
            setMeter ((float) active / (float) (n * 8));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** HARMONIZER: two voices, each a pitch shifter (VOICE 1 / 2 in semitones, DETUNE pulling them apart in
        cents), after a DELAY, with FEEDBACK round the pair - the classic studio harmonizer, voice 1 left,
        voice 2 right. */
    class Harmonizer final : public RackUnit
    {
        float delayNow = -1.0f;   // (DELAY slewed: it never moves faster than 0.4 sample a sample - a bend, never a click)
        std::array<PitchShifter, 2> v; std::array<DelayLine, 2> pre; std::array<float, 2> fb {};
        void prepareUnit (double s, int) override { for (auto& x : v) x.setup (s, 45.0); for (auto& d : pre) d.setMax ((int) (s * 0.12)); }
        void resetUnit() override { for (auto& x : v) x.clear(); for (auto& d : pre) d.clear(); fb = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float det = std::clamp (p[3], 0.0f, 50.0f) / 100.0f;
            const float r1 = std::pow (2.0f, (std::round (std::clamp (p[1], -12.0f, 12.0f)) - 0.5f * det) / 12.0f);
            const float r2 = std::pow (2.0f, (std::round (std::clamp (p[2], -12.0f, 12.0f)) + 0.5f * det) / 12.0f);
            const float delayTarget = std::max (1.0f, std::clamp (p[4], 0.0f, 100.0f) * 0.001f * (float) sr);
            if (delayNow < 0.0f) delayNow = delayTarget;
            const float feedback = std::clamp (p[5], 0.0f, 80.0f) / 100.0f, mix = std::clamp (p[6], 0.0f, 100.0f) / 100.0f;
            float act = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                const float in = 0.5f * (io[0][s] + io[1][s]);
                delayNow += std::clamp (delayTarget - delayNow, -0.4f, 0.4f);
                const float delay = delayNow;
                pre[0].push (in + feedback * fb[0]); pre[1].push (in + feedback * fb[1]);
                const float a = v[0].process (pre[0].tap (delay), r1), b = v[1].process (pre[1].tap (delay), r2);
                fb = { a, b };
                const float l = 0.8f * a + 0.25f * b, r = 0.25f * a + 0.8f * b;
                act += std::abs (a) + std::abs (b);
                io[0][s] += mix * (l - 0.5f * io[0][s]);
                io[1][s] += mix * (r - 0.5f * io[1][s]);
            }
            setMeter (act / (float) n * 2.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** BBD ENSEMBLE: the bucket-brigade chorus - delays of a few milliseconds swept by LFOs, through the
        chips' own sound: a band-limit before and after (the anti-alias and reconstruction filters), a soft
        compander, and a little clock hiss (NOISE). CHORUS I: one slow sweep; II: faster, deeper; ENSEMBLE:
        three delays 120 degrees apart, with a fast shimmer on top (the string-machine sound). */
    class BbdEnsemble final : public RackUnit
    {
        std::array<DelayLine, 2> d; std::array<Lfo, 3> slow, fast; std::array<OnePole, 4> lp; Rng rng; OnePole hiss;
        void prepareUnit (double s, int) override { for (auto& x : d) x.setMax ((int) (s * 0.04)); for (int i = 0; i < 3; ++i) { slow[(size_t) i].ph = i / 3.0; fast[(size_t) i].ph = i / 3.0; } }
        void resetUnit() override { for (auto& x : d) x.clear(); for (auto& l : lp) l.z = 0; hiss.z = 0; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float rate = std::clamp (p[1], 0.1f, 10.0f), depth = std::clamp (p[2], 0.0f, 10.0f) / 10.0f;
            const int mode = std::clamp ((int) std::lround (p[3]), 0, 2);
            const float noise = std::clamp (p[4], 0.0f, 10.0f) / 10.0f, mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            const float k = onePoleK (sr, 9000.0), base = 0.007f * (float) sr, sweep = (mode == 1 ? 0.004f : 0.0025f) * (float) sr * depth;
            const int taps = mode == 2 ? 3 : 1;
            for (int s = 0; s < n; ++s)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const float in = lp[(size_t) c].process (io[c][s], k);
                    d[(size_t) c].push (std::tanh (in * 1.4f) / 1.4f);   // (the compander's gentle squash)
                }
                float out[2] {};
                for (int t = 0; t < taps; ++t)
                {
                    const float ls = slow[(size_t) t].next (rate * (mode == 1 ? 1.6 : 1.0), sr);
                    const float lf = mode == 2 ? 0.15f * fast[(size_t) t].next (6.0, sr) : 0.0f;
                    for (int c = 0; c < 2; ++c)
                    {
                        const float sgn = c == 0 ? 1.0f : -1.0f;   // (the two sides sweep opposite ways: width)
                        out[c] += d[(size_t) c].tap (base + sweep * (sgn * ls + lf));
                    }
                }
                const float h = hiss.process (rng.next() - 0.5f, 0.6f) * noise * 0.004f;
                for (int c = 0; c < 2; ++c)
                {
                    const float wet = lp[(size_t) (2 + c)].process (out[c] / (float) taps, k) + h;
                    io[c][s] += mix * (wet - 0.5f * io[c][s]);
                }
            }
            setMeter (depth * mix);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** BODE SHIFTER: every frequency moved by the same number of Hertz (not a pitch shift: harmonics stop
        being harmonic - bells, robots, gentle "rotating speaker" at a few Hz). A Hilbert transform (two
        chains of allpasses 90 degrees apart, Olli Niemitalo's coefficients) makes the signal's analytic
        pair, turned by a sine and cosine. FEEDBACK shifts it again each pass (barber-pole spirals);
        SPREAD shifts the right side a little further than the left. */
    class BodeShifter final : public RackUnit
    {
        struct HalfAp { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; float process (float x, float a2) noexcept { const float y = a2 * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
        struct Hilbert
        {
            std::array<HalfAp, 4> a, b; float delayed = 0.0f;
            static constexpr float ca[4] { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
            static constexpr float cb[4] { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
            std::pair<float, float> process (float x) noexcept
            {
                float i = x, q = x;
                for (int k = 0; k < 4; ++k) { i = a[(size_t) k].process (i, ca[k] * ca[k]); q = b[(size_t) k].process (q, cb[k] * cb[k]); }
                const float re = delayed; delayed = i;
                return { re, q };
            }
        };
        std::array<Hilbert, 2> h; std::array<double, 2> ph {}; std::array<float, 2> fb {};
        void prepareUnit (double, int) override {}
        void resetUnit() override { h = {}; ph = {}; fb = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float shift = std::clamp (p[1], -500.0f, 500.0f), feedback = std::clamp (p[2], 0.0f, 90.0f) / 100.0f;
            const float spread = std::clamp (p[3], 0.0f, 50.0f), mix = std::clamp (p[4], 0.0f, 100.0f) / 100.0f;
            for (int s = 0; s < n; ++s)
                for (int c = 0; c < 2; ++c)
                {
                    const auto [re, im] = h[(size_t) c].process (io[c][s] + feedback * fb[(size_t) c]);
                    const double f = shift + (c == 1 ? spread : 0.0f);
                    ph[(size_t) c] += f / sr; ph[(size_t) c] -= std::floor (ph[(size_t) c]);
                    const float w = (float) (6.283185307179586 * ph[(size_t) c]);
                    const float y = re * std::cos (w) - im * std::sin (w);
                    fb[(size_t) c] = y;
                    io[c][s] += mix * (y - io[c][s]);
                }
            setMeter (std::min (1.0f, std::abs (shift) / 100.0f + feedback) * mix);
        }
    };
}
