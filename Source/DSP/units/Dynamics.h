#pragma once

#include "RackUnit.h"
#include "TimeFx.h"   // (Rng)

/*  The newer units that shape level and colour: ATR TAPE, T4 OPTO, VARI-MU, DYNAMIC EQ 4, OVERCLIP. */
namespace enh::dsp::units
{
    // ------------------------------------------------------------------------------------------------
    /** ATR TAPE: tape's magnetisation lags the field that makes it - a hysteresis loop. Here the saturator
        remembers where it has been (the curve is driven by the input plus a share of its last output: the
        loop opens with SATURATION), so it compresses and adds harmonics that depend on the direction the
        signal moves, as tape does. Then what the heads do at each SPEED: the head bump (a low lift) and the
        top roll-off; WOW (slow) and FLUTTER (fast) pitch wander from a modulated delay; HISS, if wanted. */
    class AtrTape final : public RackUnit
    {
        std::array<float, 2> m {}; std::array<DelayLine, 2> wf; Lfo wow, flutter; Rng rng; std::array<BiquadState, 2> bump; std::array<OnePole, 2> top; OnePole hissLp;
        BiquadCoeffs bumpC; int curSpeed = -1; float topK = 0.5f; OnePole flutterNoise;
        void prepareUnit (double s, int) override { for (auto& d : wf) d.setMax ((int) (s * 0.02)); }
        void resetUnit() override { m = {}; for (auto& d : wf) d.clear(); bump = {}; for (auto& t : top) t.z = 0; hissLp.z = 0; flutterNoise.z = 0; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float in = dbToGainF (std::clamp (p[1], -12.0f, 18.0f)), sat = std::clamp (p[2], 0.0f, 10.0f) / 10.0f;
            const int speed = std::clamp ((int) std::lround (p[3]), 0, 2);
            if (speed != curSpeed)
            {
                curSpeed = speed;
                static constexpr double bumpHz[3] { 45.0, 60.0, 95.0 }, topHz[3] { 11000.0, 16000.0, 20000.0 };
                bumpC = BiquadCoeffs::peaking (sr, bumpHz[speed], 0.9, 2.5);
                topK = onePoleK (sr, std::min (topHz[speed], 0.45 * sr));
            }
            const float wowD = std::clamp (p[4], 0.0f, 10.0f) * 0.00012f * (float) sr, flD = std::clamp (p[5], 0.0f, 10.0f) * 0.000012f * (float) sr;
            const float hiss = std::clamp (p[6], 0.0f, 10.0f) > 0.0f ? dbToGainF (-78.0f + 3.0f * p[6]) : 0.0f;
            const float out = dbToGainF (std::clamp (p[7], -12.0f, 12.0f));
            const float drive = 0.6f + 2.4f * sat, loop = 0.35f * sat;
            float satAmt = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                const float mod = wowD * (1.0f + wow.next (0.55, sr)) + flD * (1.0f + flutter.next (6.8, sr) + 0.6f * flutterNoise.process (rng.next() - 0.5f, 0.01f));
                const float h = hiss * hissLp.process (rng.next() - 0.5f, 0.7f);
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][s] * in;
                    // hysteresis: the curve pulled toward where it just was
                    const float y = std::tanh (drive * x + loop * m[(size_t) c]) / drive;
                    m[(size_t) c] = y * drive;
                    satAmt += std::abs (y - x);
                    float v = bump[(size_t) c].process (bumpC, y);
                    v = top[(size_t) c].process (v, topK);
                    wf[(size_t) c].push (v);
                    io[c][s] = (wf[(size_t) c].tap (2.0f + mod) + h) * out / std::max (0.25f, in);
                }
            }
            setMeter (satAmt / (float) n * 4.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** T4 OPTO: an electroluminescent panel lights a photocell; the cell's resistance sets the gain. The
        light answers in ~10 ms; the cell lets go in two stages - fast at first (~60 ms), then slowly, and the
        longer it has been lit the slower (its memory): the smooth, program-dependent release that leveling
        amplifiers are loved for. PEAK REDUCTION: how hard it is lit; GAIN: makeup; LIMIT: a higher ratio;
        EMPHASIS: the detector hears more treble (it reacts to sibilance and cymbals first). */
    class T4Opto final : public RackUnit
    {
        float light = 0.0f, fast = 0.0f, slow = 0.0f, memory = 0.0f; BiquadState emphasisSt; BiquadCoeffs emphasisC; float curEmph = -1.0f;
        void prepareUnit (double, int) override {}
        void resetUnit() override { light = fast = slow = memory = 0.0f; emphasisSt = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float peak = std::clamp (p[1], 0.0f, 100.0f) / 100.0f, gain = dbToGainF (std::clamp (p[2], 0.0f, 30.0f));
            const bool limit = p[3] > 0.5f;
            const float emph = std::clamp (p[4], 0.0f, 10.0f), mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            if (emph != curEmph) { curEmph = emph; emphasisC = BiquadCoeffs::highShelf (sr, 3000.0, 0.7, 1.2 * emph); }
            const float thresh = dbToGainF (-6.0f - 34.0f * peak), ratio = limit ? 12.0f : 3.0f;
            const float att = 1.0f - std::exp (-1.0f / (0.010f * (float) sr)), relFast = 1.0f - std::exp (-1.0f / (0.060f * (float) sr));
            float grMax = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                const float det = std::abs (emphasisSt.process (emphasisC, 0.5f * (io[0][s] + io[1][s])));
                const float over = det > thresh ? std::pow (det / thresh, 1.0f - 1.0f / ratio) - 1.0f : 0.0f;   // how much light
                light += (over > light ? att : relFast * 0.5f) * (over - light);
                // the cell: a fast part and a slow part; the slow part's share grows with how long it has been lit
                memory += (light > 0.05f ? 1.0f : -0.3f) / (float) sr; memory = std::clamp (memory, 0.0f, 3.0f);
                const float relSlow = 1.0f - std::exp (-1.0f / ((0.5f + memory) * (float) sr));
                fast += (light > fast ? att : relFast) * (light - fast);
                slow += (light > slow ? att * 0.3f : relSlow) * (light - slow);
                const float r = 0.6f * fast + 0.4f * slow;
                const float g = 1.0f / (1.0f + r);
                grMax = std::max (grMax, -20.0f * std::log10 (g));
                for (int c = 0; c < 2; ++c) { const float d = io[c][s]; io[c][s] = d + mix * (d * g * gain - d); }
            }
            setMeter (grMax / 18.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** VARI-MU: a remote-cutoff valve's gain falls as its grid is biased - so the ratio rises with the gain
        reduction (gentle at first, firmer the harder it works), with a soft knee and a valve's even harmonics.
        RECOVERY: 0.1 - 2 s, or AUTO (two time constants: quick after a transient, slow after a sustained
        passage). M/S: the middle and the sides compressed apart. INPUT drives the valve stage. */
    class VariMu final : public RackUnit
    {
        std::array<float, 2> env {}, grDb {}, fastRel {}; std::array<kit::SoftSat, 2> tube; std::array<kit::Level, 2> lvl;
        void prepareUnit (double s, int) override { for (auto& l : lvl) l.setup (s, 0.15); }
        void resetUnit() override { env = {}; grDb = {}; fastRel = {}; for (auto& t : tube) t.reset(); for (auto& l : lvl) l.reset(); }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float drive = 0.3f + 0.17f * std::clamp (p[1], 0.0f, 10.0f), thresh = std::clamp (p[2], -40.0f, 0.0f);
            const int rec = std::clamp ((int) std::lround (p[3]), 0, 5);
            static constexpr float recS[5] { 0.1f, 0.3f, 0.6f, 1.2f, 2.0f };
            const float gain = dbToGainF (std::clamp (p[4], 0.0f, 20.0f));
            const bool ms = p[5] > 0.5f; const float mix = std::clamp (p[6], 0.0f, 100.0f) / 100.0f;
            const float att = 1.0f - std::exp (-1.0f / (0.008f * (float) sr)), envK = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));
            float meterDb = 0.0f;
            for (int s = 0; s < n; ++s)
            {
                float a = io[0][s], b = io[1][s];
                if (ms) { const float m = 0.5f * (a + b), sd = 0.5f * (a - b); a = m; b = sd; }
                float ch[2] { a, b };
                const float linked = std::max (std::abs (a), std::abs (b));
                for (int c = 0; c < 2; ++c)
                {
                    const float det = ms ? std::abs (ch[c]) : linked;
                    env[(size_t) c] += envK * (det * det - env[(size_t) c]);
                    const float levelDb = 10.0f * std::log10 (env[(size_t) c] * 2.0f + 1.0e-12f), over = levelDb - thresh;
                    // soft knee (10 dB), ratio 1.5 rising 0.12 per dB of reduction
                    const float o = over < -5.0f ? 0.0f : over < 5.0f ? (over + 5.0f) * (over + 5.0f) / 20.0f : over;
                    const float ratio = 1.5f + 0.12f * grDb[(size_t) c];
                    const float want = o * (1.0f - 1.0f / ratio);
                    float rel;
                    if (rec < 5) rel = 1.0f - std::exp (-1.0f / (recS[rec] * (float) sr));
                    else { fastRel[(size_t) c] += 0.0005f * ((want > 3.0f ? 1.0f : 0.0f) - fastRel[(size_t) c]);
                           rel = 1.0f - std::exp (-1.0f / ((0.15f + 1.5f * fastRel[(size_t) c]) * (float) sr)); }
                    grDb[(size_t) c] += (want > grDb[(size_t) c] ? att : rel) * (want - grDb[(size_t) c]);
                    meterDb = std::max (meterDb, grDb[(size_t) c]);
                    const float g = dbToGainF (-grDb[(size_t) c]);
                    const float x = ch[c] * g, l = lvl[(size_t) c].process (x);
                    ch[c] = (x + 0.35f * drive * kit::harmonicsAt (tube[(size_t) c], x, l, drive, 0.3f)) * gain;
                }
                if (ms) { const float m = ch[0], sd = ch[1]; ch[0] = m + sd; ch[1] = m - sd; }
                for (int c = 0; c < 2; ++c) io[c][s] += mix * (ch[c] - io[c][s]);
            }
            setMeter (meterDb / 18.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** DYNAMIC EQ 4: four bells that move with the music. Each band listens to its own frequency (a band-pass
        at FREQ) and, above THRESH, turns its bell by up to RANGE (negative: it dips a resonance only when it
        rings out - a harsh 3 kHz only when it's harsh; positive: it lifts a band only when it's there). The
        bells glide (every 32 samples) so nothing zips. */
    class DynamicEq final : public RackUnit
    {
        static constexpr int B = 4;
        std::array<std::array<BiquadState, 2>, B> bell {}; std::array<BiquadState, B> side {}; std::array<float, B> env {}, gainDb {};
        std::array<BiquadCoeffs, B> bellC {}, sideC {}; std::array<float, B> curF {};
        void prepareUnit (double, int) override { curF = {}; }
        void resetUnit() override { bell = {}; side = {}; env = {}; gainDb = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float att = 1.0f - std::exp (-1.0f / (0.005f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (0.080f * (float) sr));
            float most = 0.0f;
            for (int s0 = 0; s0 < n; s0 += 32)
            {
                const int m = std::min (32, n - s0);
                for (int b = 0; b < B; ++b)
                {
                    const float f = std::clamp (p[1 + 3 * b], 20.0f, 0.45f * (float) sr), thresh = p[2 + 3 * b], range = std::clamp (p[3 + 3 * b], -12.0f, 12.0f);
                    if (f != curF[(size_t) b]) { curF[(size_t) b] = f; sideC[(size_t) b] = BiquadCoeffs::bandPass (sr, f, 1.4); }
                    float e = env[(size_t) b];
                    for (int s = s0; s < s0 + m; ++s)
                    {
                        const float v = std::abs (side[(size_t) b].process (sideC[(size_t) b], 0.5f * (io[0][s] + io[1][s])));
                        e += (v > e ? att : rel) * (v - e);
                    }
                    env[(size_t) b] = e;
                    const float over = std::clamp ((20.0f * std::log10 (e + 1.0e-9f) - thresh) / 12.0f, 0.0f, 1.0f);
                    gainDb[(size_t) b] += 0.3f * (range * over - gainDb[(size_t) b]);
                    most = std::max (most, std::abs (gainDb[(size_t) b]));
                    bellC[(size_t) b] = BiquadCoeffs::peaking (sr, f, 1.0, gainDb[(size_t) b]);
                }
                for (int s = s0; s < s0 + m; ++s)
                    for (int c = 0; c < 2; ++c)
                    {
                        float v = io[c][s];
                        for (int b = 0; b < B; ++b) v = bell[(size_t) b][(size_t) c].process (bellC[(size_t) b], v);
                        io[c][s] = v;
                    }
            }
            setMeter (most / 12.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** OVERCLIP: a clipper run four times faster than the audio (two half-band stages up, the same down),
        so the edges it cuts don't fold back as aliases. DRIVE into it, CEILING out; KNEE rounds the corner;
        SOFT (a smooth curve), HARD (a flat top), TUBE (asymmetric, even harmonics). */
    class Overclip final : public RackUnit
    {
        /** A half-band low-pass (31 taps, windowed sinc) for x2 up / down sampling. */
        struct HalfBand
        {
            static constexpr int T = 31; std::array<float, T> h {}; std::array<float, T> z {}; int w = 0;
            HalfBand() { for (int i = 0; i < T; ++i) { const int k = i - T / 2; const double x = k * 0.5; const double sinc = k == 0 ? 1.0 : std::sin (3.141592653589793 * x) / (3.141592653589793 * x);
                                                        h[(size_t) i] = (float) (0.5 * sinc * (0.42 - 0.5 * std::cos (6.283185 * i / (T - 1)) + 0.08 * std::cos (12.56637 * i / (T - 1)))); } }
            void clear() { z = {}; w = 0; }
            float process (float x) noexcept { z[(size_t) w] = x; float y = 0.0f; int k = w; for (int i = 0; i < T; ++i) { y += h[(size_t) i] * z[(size_t) k]; k = k == 0 ? T - 1 : k - 1; } w = (w + 1) % T; return y; }
        };
        std::array<std::array<HalfBand, 4>, 2> up, down;
        void prepareUnit (double, int) override {}
        void resetUnit() override { for (auto& a : up) for (auto& b : a) b.clear(); for (auto& a : down) for (auto& b : a) b.clear(); }
        static float clip (float x, int mode, float knee) noexcept
        {
            if (mode == 1) { const float a = std::abs (x); const float k = std::max (0.001f, knee); return a < 1.0f - k ? x : std::copysign (a < 1.0f + k ? 1.0f - k + (a - 1.0f + k) - (a - 1.0f + k) * (a - 1.0f + k) / (4.0f * k) : 1.0f, x); }
            if (mode == 2) { const float y = std::tanh (x + 0.15f * x * x); return y - 0.15f * std::tanh (x * x) * 0.5f; }   // (tube: asymmetric; its DC mostly taken back)
            // soft: linear to (1 - knee), then a tanh shoulder that never passes 1
            const float a = std::abs (x), k = 0.15f + knee;
            return a < 1.0f - k ? x : std::copysign ((1.0f - k) + k * std::tanh ((a - (1.0f - k)) / k), x);
        }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float drive = dbToGainF (std::clamp (p[1], 0.0f, 24.0f)), ceiling = dbToGainF (std::clamp (p[2], -12.0f, 0.0f));
            const float knee = std::clamp (p[3], 0.0f, 10.0f) / 10.0f * 0.5f; const int mode = std::clamp ((int) std::lround (p[4]), 0, 2);
            const float mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            float cut = 0.0f;
            for (int s = 0; s < n; ++s)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][s] * drive / ceiling;
                    float acc = 0.0f;
                    // x4: each input sample becomes four (zeros between, filtered twice), clipped, filtered back down
                    for (int k1 = 0; k1 < 2; ++k1)
                    {
                        const float u1 = up[(size_t) c][0].process (k1 == 0 ? 2.0f * x : 0.0f);
                        for (int k2 = 0; k2 < 2; ++k2)
                        {
                            const float u2 = up[(size_t) c][1].process (k2 == 0 ? 2.0f * u1 : 0.0f);
                            const float y = clip (u2, mode, knee);
                            const float d1 = down[(size_t) c][0].process (y);
                            if (k2 == 1) { const float d2 = down[(size_t) c][1].process (d1); if (k1 == 1) acc = d2; }
                        }
                    }
                    const float y = acc * ceiling;
                    cut += std::max (0.0f, std::abs (x * ceiling) - std::abs (y));
                    io[c][s] += mix * (y - io[c][s]);
                }
            setMeter (cut / (float) n * 3.0f);
        }
    };
}
