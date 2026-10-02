#pragma once

#include <atomic>
#include <string_view>
#include "RackUnit.h"

/*  CUSTOM: a unit made in the Rack Unit Designer, loaded from its share code. The design's sound - its chain
    of blocks (designer.js DSP_BLOCKS: the same sixteen, the same parameter ranges) - runs here; its knobs are
    the slot's 16 knob parameters and 4 switches, each wired (as the design says) to one block parameter, or
    to a block's in/out. The chain comes in as a Config built on the message thread (Custom/DesignCode.h)
    and is swapped in atomically; the audio thread never allocates or waits. */
namespace enh::dsp::units
{
    struct CustomConfig
    {
        enum Block { eq, filter, drive, comp, exciter, delay, room, width, gain, chorus, pan, wander, stutter, crush, wow, shimmer, none };
        static constexpr int blockTypes = 16;
        static constexpr int maxBlocks = 8, maxParams = 7;
        struct B { Block type = none; bool on = true; std::array<float, maxParams> p {}; };
        std::array<B, maxBlocks> blocks {};
        int count = 0;
        /** A knob slot (0..15) or switch slot (16..19) wired to block `block`'s parameter `param`
            (-1: its in/out). block -1: not wired. */
        struct Wire { int block = -1, param = -1; };
        std::array<Wire, 20> wires {};
    };

    /** The blocks' parameters, in the designer's order: { min, max, log }. */
    struct ParamRange { float lo, hi; bool log; };
    inline const std::array<ParamRange, CustomConfig::maxParams>& rangesOf (CustomConfig::Block b)
    {
        static const std::array<std::array<ParamRange, CustomConfig::maxParams>, CustomConfig::blockTypes> r {{
            {{ { -15, 15, false }, { 30, 500, true }, { -15, 15, false }, { 200, 8000, true }, { 0.3f, 6, false }, { -15, 15, false }, { 2000, 16000, true } }},   // eq: low lowf mid midf q high highf
            {{ { 0, 2, false }, { 20, 20000, true }, { 0.3f, 12, false } }},                                                        // filter: mode freq q
            {{ { 0, 36, false }, { 0, 2, false }, { 1000, 20000, true }, { 0, 100, false } }},                                    // drive: drive shape tone mix
            {{ { -60, 0, false }, { 1, 20, true }, { 0.1f, 100, true }, { 10, 1500, true }, { 0, 24, false }, { 0, 100, false } }},   // comp: threshold ratio attack release makeup mix
            {{ { 1500, 12000, true }, { 0, 100, false } }},                                                                         // exciter: freq amount
            {{ { 10, 1500, true }, { 0, 90, false }, { 500, 16000, true }, { 0, 100, false } }},                                  // delay: time feedback tone mix
            {{ { 0.2f, 8, true }, { 0, 100, false }, { 0, 200, false }, { 0, 100, false } }},                                     // room: size damp predelay mix
            {{ { 0, 200, false } }},                                                                                                // width
            {{ { -24, 12, false } }},                                                                                               // gain
            {{ { 0.05f, 6, true }, { 0, 100, false }, { 0, 100, false } }},                                                          // chorus: rate depth mix
            {{ { 0.05f, 12, true }, { 0, 100, false }, { 0, 1, false } }},                                                           // pan: rate depth mode
            {{ { 150, 8000, true }, { 0, 100, false }, { 0.02f, 2, true }, { 0.3f, 10, false } }},                                   // wander: freq range speed q
            {{ { 1, 16, true }, { 0, 100, false }, { 0, 100, false } }},                                                             // stutter: rate depth smooth
            {{ { 2, 12, false }, { 0, 100, false } }},                                                                              // crush: bits mix
            {{ { 0, 100, false }, { 0, 100, false } }},                                                                             // wow: wow flutter
            {{ { 1, 8, true }, { 0, 100, false }, { 0, 100, false } }},                                                             // shimmer: size octave mix
        }};
        static const std::array<ParamRange, CustomConfig::maxParams> empty {};
        return b >= 0 && b < CustomConfig::blockTypes ? r[(size_t) b] : empty;
    }

    class CustomUnit final : public RackUnit
    {
    public:
        /** Message thread: the design's chain (nullptr: nothing loaded - the slot passes the sound). The
            caller keeps every Config it has ever given alive for as long as this unit lives. */
        void setConfig (const CustomConfig* c) noexcept { pending.store (c, std::memory_order_release); }

    private:
        struct Room   // a small four-line reverb (the designer's convolver's stand-in)
        {
            std::array<DelayLine, 4> d; std::array<OnePole, 4> damp; DelayLine pre;
            void setup (double sr) { for (auto& x : d) x.setMax ((int) (sr * 0.12)); pre.setMax ((int) (sr * 0.25)); }
            void clear() { for (auto& x : d) x.clear(); for (auto& o : damp) o.z = 0; pre.clear(); }
        };
        struct State
        {
            std::array<std::array<BiquadState, 3>, 2> eq {}; std::array<BiquadState, 2> filt {}, exHp {}, exHp2 {};
            std::array<OnePole, 2> tone {}; std::array<DelayLine, 2> dl; std::array<OnePole, 2> dlTone {}; Room room;
            float env = 0.0f, grDb = 0.0f;
            Lfo lfoA, lfoB; OnePole smooth; BiquadCoeffs wanderCo {}; int wanderAt = 0;   // (the moving blocks)
        };
        std::array<State, CustomConfig::maxBlocks> st;
        std::atomic<const CustomConfig*> pending { nullptr };
        const CustomConfig* cfg = nullptr;
        CustomConfig live;

        void prepareUnit (double s, int) override { for (auto& x : st) { x.dl[0].setMax ((int) (s * 1.6)); x.dl[1].setMax ((int) (s * 1.6)); x.room.setup (s); } }
        void resetUnit() override
        {
            for (auto& x : st) { x.eq = {}; x.filt = {}; x.exHp = {}; x.exHp2 = {}; x.tone = {}; x.dl[0].clear(); x.dl[1].clear(); x.dlTone = {}; x.room.clear(); x.env = x.grDb = 0.0f; x.lfoA = {}; x.lfoB = {}; x.smooth = {}; x.wanderAt = 0; }
        }

        void render (float* const* io, int n, const float* p) noexcept override
        {
            if (const auto* c = pending.exchange (nullptr, std::memory_order_acq_rel)) { cfg = c; resetUnit(); }
            if (cfg == nullptr) { setMeter (0.0f); return; }
            // The chain's parameters as the knobs set them now
            live = *cfg;
            for (int w = 0; w < 20; ++w)
            {
                const auto wire = cfg->wires[(size_t) w];
                if (wire.block < 0 || wire.block >= live.count) continue;
                const float v = w < 16 ? std::clamp (p[1 + w], 0.0f, 100.0f) / 100.0f : (p[1 + w] > 0.5f ? 1.0f : 0.0f);
                auto& b = live.blocks[(size_t) wire.block];
                if (wire.param < 0) { b.on = v > 0.5f; continue; }
                const auto r = rangesOf (b.type)[(size_t) wire.param];
                b.p[(size_t) wire.param] = r.log && r.lo > 0.0f ? r.lo * std::pow (r.hi / r.lo, v) : r.lo + (r.hi - r.lo) * v;
            }
            float work = 0.0f;
            for (int k = 0; k < live.count; ++k)
                if (live.blocks[(size_t) k].on)
                    work += block (live.blocks[(size_t) k], st[(size_t) k], io, n);
            setMeter (work);
        }

        /** The four-line room over the pair, fed `feed (i)` (mono), mixed in by `mix`. */
        template <typename Feed>
        void runRoom (Room& rm, float* const* io, int n, float sizeS, float damp, float preMs, float mix, Feed&& feed) noexcept
        {
            static constexpr float ms[4] { 31.7f, 37.9f, 43.1f, 49.3f };
            const float size = std::clamp (sizeS, 0.2f, 8.0f), dk = onePoleK (sr, 12000.0 * std::pow (0.1, damp / 100.0)), pre = std::max (1.0f, preMs * 0.001f * (float) sr);
            std::array<float, 4> len {}, g {};
            for (int k = 0; k < 4; ++k) { len[(size_t) k] = ms[k] * 0.001f * (float) sr * (0.7f + 0.15f * std::min (size, 4.0f)); g[(size_t) k] = std::pow (10.0f, -3.0f * len[(size_t) k] / ((float) sr * size)); }
            for (int i = 0; i < n; ++i)
            {
                rm.pre.push (feed (i));
                const float in = rm.pre.tap (pre);
                std::array<float, 4> o {};
                for (int k = 0; k < 4; ++k) o[(size_t) k] = rm.damp[(size_t) k].process (rm.d[(size_t) k].tap (len[(size_t) k]), dk) * g[(size_t) k];
                const float a = o[0] + o[1], b2 = o[0] - o[1], c2 = o[2] + o[3], d2 = o[2] - o[3];
                const std::array<float, 4> m { 0.5f * (a + c2), 0.5f * (b2 + d2), 0.5f * (a - c2), 0.5f * (b2 - d2) };
                for (int k = 0; k < 4; ++k) rm.d[(size_t) k].push (m[(size_t) k] + ((k & 1) ? -0.3f : 0.3f) * in);
                io[0][i] += mix * ((o[0] + o[2]) * 0.8f - io[0][i] * 0.35f);
                io[1][i] += mix * ((o[1] + o[3]) * 0.8f - io[1][i] * 0.35f);
            }
        }

        /** One block over the pair, in place; returns how hard it worked (0..1, for the meter). */
        float block (const CustomConfig::B& b, State& s, float* const* io, int n) noexcept
        {
            const auto& q = b.p;
            switch (b.type)
            {
                case CustomConfig::eq:
                {
                    const auto lo = BiquadCoeffs::lowShelf (sr, q[1], 0.707, q[0]), mid = BiquadCoeffs::peaking (sr, q[3], std::max (0.3f, q[4]), q[2]), hi = BiquadCoeffs::highShelf (sr, std::min (q[6], 0.45f * (float) sr), 0.707, q[5]);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] = s.eq[(size_t) c][2].process (hi, s.eq[(size_t) c][1].process (mid, s.eq[(size_t) c][0].process (lo, io[c][i])));
                    return (std::abs (q[0]) + std::abs (q[2]) + std::abs (q[5])) / 30.0f;
                }
                case CustomConfig::filter:
                {
                    const int mode = (int) std::lround (q[0]); const double f = std::min ((double) q[1], 0.45 * sr);
                    const auto co = mode == 1 ? BiquadCoeffs::highPass (sr, f, q[2]) : mode == 2 ? BiquadCoeffs::bandPass (sr, f, q[2]) : BiquadCoeffs::lowPass (sr, f, q[2]);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] = s.filt[(size_t) c].process (co, io[c][i]);
                    return 0.4f;
                }
                case CustomConfig::drive:
                {
                    const float g = dbToGainF (q[0]), post = 1.0f / std::sqrt (g), mix = q[3] / 100.0f, tk = onePoleK (sr, std::min ((double) q[2], 0.45 * sr));
                    const int shape = (int) std::lround (q[1]);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i)
                    {
                        const float x = io[c][i], u = x * g;
                        const float y = shape == 0 ? std::tanh (u + 0.2f) - std::tanh (0.2f) : shape == 1 ? std::tanh (1.5f * u) / std::tanh (1.5f) : std::clamp (u, -0.7f, 0.7f) / 0.7f;
                        io[c][i] = x + mix * (s.tone[(size_t) c].process (y * post, tk) - x);
                    }
                    return q[0] / 36.0f * mix;
                }
                case CustomConfig::comp:
                {
                    const float att = 1.0f - std::exp (-1.0f / (q[2] * 0.001f * (float) sr)), rel = 1.0f - std::exp (-1.0f / (q[3] * 0.001f * (float) sr));
                    const float makeup = dbToGainF (q[4]), mix = q[5] / 100.0f;
                    for (int i = 0; i < n; ++i)
                    {
                        const float det = std::max (std::abs (io[0][i]), std::abs (io[1][i]));
                        s.env += (det > s.env ? att : rel) * (det - s.env);
                        const float over = 20.0f * std::log10 (s.env + 1.0e-9f) - q[0];
                        s.grDb = over > 0.0f ? over * (1.0f - 1.0f / std::max (1.0f, q[1])) : 0.0f;
                        const float g = dbToGainF (-s.grDb) * makeup;
                        for (int c = 0; c < 2; ++c) io[c][i] += mix * (io[c][i] * g - io[c][i]);
                    }
                    return s.grDb / 18.0f;
                }
                case CustomConfig::exciter:
                {
                    const auto hp = BiquadCoeffs::highPass (sr, std::min ((double) q[0], 0.45 * sr), 0.707), hp2 = BiquadCoeffs::highPass (sr, std::min (q[0] * 1.5, 0.45 * sr), 0.707);
                    const float amt = q[1] / 100.0f * 0.25f;
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i)
                    { const float h = s.exHp[(size_t) c].process (hp, io[c][i]); io[c][i] += amt * s.exHp2[(size_t) c].process (hp2, std::tanh (4.0f * h)); }
                    return q[1] / 100.0f;
                }
                case CustomConfig::delay:
                {
                    const float d = std::max (1.0f, q[0] * 0.001f * (float) sr), fb = q[1] / 100.0f, tk = onePoleK (sr, std::min ((double) q[2], 0.45 * sr)), mix = q[3] / 100.0f;
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i)
                    {
                        const float y = s.dlTone[(size_t) c].process (s.dl[(size_t) c].tap (d), tk);
                        s.dl[(size_t) c].push (io[c][i] + fb * y);
                        io[c][i] += mix * (y - io[c][i] * 0.5f);
                    }
                    return mix;
                }
                case CustomConfig::room:
                {
                    const float mix = q[3] / 100.0f;
                    runRoom (s.room, io, n, q[0], q[1], q[2], mix, [&] (int i) { return 0.5f * (io[0][i] + io[1][i]); });
                    return mix;
                }
                case CustomConfig::chorus:   // two voices a few ms late, their delays swaying opposite ways
                {
                    const float base = 0.014f * (float) sr, sw = q[1] / 100.0f * 0.006f * (float) sr, mix = q[2] / 100.0f;
                    for (int i = 0; i < n; ++i)
                    {
                        const float m = s.lfoA.next (q[0], sr);
                        for (int c = 0; c < 2; ++c)
                        {
                            s.dl[(size_t) c].push (io[c][i]);
                            const float y = s.dl[(size_t) c].tap (base + (c == 0 ? sw : -sw) * m);
                            io[c][i] += mix * (y - io[c][i]);
                        }
                    }
                    return mix * q[1] / 100.0f;
                }
                case CustomConfig::pan:   // a pendulum: side to side (the browser's stereo panner, to the letter), or in and out
                {
                    const float d = q[1] / 100.0f; const bool vol = std::lround (q[2]) == 1;
                    for (int i = 0; i < n; ++i)
                    {
                        const float m = s.lfoA.next (q[0], sr);
                        if (vol) { const float g = 1.0f - 0.5f * d + 0.5f * d * m; io[0][i] *= g; io[1][i] *= g; continue; }
                        const float v = d * m, L = io[0][i], R = io[1][i];
                        if (v <= 0.0f) { const float x = (v + 1.0f) * 1.5707963f; io[0][i] = L + R * std::cos (x); io[1][i] = R * std::sin (x); }
                        else { const float x = v * 1.5707963f; io[0][i] = L * std::cos (x); io[1][i] = R + L * std::sin (x); }
                    }
                    return d;
                }
                case CustomConfig::wander:   // a band-pass drifting like wax in a lava lamp: two slow swings that never line up
                {
                    for (int i = 0; i < n; ++i)
                    {
                        const float a = s.lfoA.next (q[2], sr), b2 = s.lfoB.next (q[2] * 0.618, sr);
                        if (s.wanderAt-- <= 0)
                        {
                            s.wanderAt = 32;
                            const double cents = q[1] * (14.0 * a + 10.0 * b2);
                            s.wanderCo = BiquadCoeffs::bandPass (sr, std::clamp (q[0] * std::pow (2.0, cents / 1200.0), 20.0, 0.45 * sr), std::max (0.3f, q[3]));
                        }
                        for (int c = 0; c < 2; ++c) io[c][i] += 0.7f * (s.filt[(size_t) c].process (s.wanderCo, io[c][i]) - io[c][i]);
                    }
                    return q[1] / 100.0f;
                }
                case CustomConfig::stutter:   // chops in time: gated on and off, the edges rounded so they never click
                {
                    const float d = q[1] / 100.0f, k = onePoleK (sr, 400.0 - 3.7 * q[2]);
                    for (int i = 0; i < n; ++i)
                    {
                        const float sq = s.lfoA.next (q[0], sr) >= 0.0f ? 1.0f : -1.0f, g = 1.0f - 0.5f * d + 0.5f * d * s.smooth.process (sq, k);
                        io[0][i] *= g; io[1][i] *= g;
                    }
                    return d;
                }
                case CustomConfig::crush:   // fewer bits: the steps of an old sampler
                {
                    const float steps = std::pow (2.0f, std::round (q[0]) - 1.0f), mix = q[1] / 100.0f;
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i)
                    { const float x = io[c][i], y = 0.8f * std::round (std::clamp (x, -1.0f, 1.0f) * steps) / steps; io[c][i] = x + mix * (y - x); }
                    return mix * (1.0f - (q[0] - 2.0f) / 10.0f);
                }
                case CustomConfig::wow:   // a tape machine's slow wow and quick flutter, as pitch wobble
                {
                    const float base = 0.012f * (float) sr, w = q[0] / 100.0f * 0.004f * (float) sr, f = q[1] / 100.0f * 0.0004f * (float) sr;
                    for (int i = 0; i < n; ++i)
                    {
                        const float d = base + w * s.lfoA.next (0.55, sr) + f * s.lfoB.next (8.5, sr);
                        for (int c = 0; c < 2; ++c) { s.dl[(size_t) c].push (io[c][i]); io[c][i] = s.dl[(size_t) c].tap (d); }
                    }
                    return (q[0] + q[1]) / 200.0f;
                }
                case CustomConfig::shimmer:   // a long, bright space fed an octave up (a rectifier doubles every note)
                {
                    const auto hp = BiquadCoeffs::highPass (sr, 180.0, 0.707); const float up = q[1] / 100.0f * 1.4f, plain = 1.0f - q[1] / 200.0f, mix = q[2] / 100.0f;
                    runRoom (s.room, io, n, q[0], 10.0f, 0.0f, mix, [&] (int i)
                    { const float m = 0.5f * (io[0][i] + io[1][i]); return 0.7f * (up * s.exHp[0].process (hp, std::abs (m)) + plain * m); });
                    return mix;
                }
                case CustomConfig::width:
                {
                    const float w = q[0] / 100.0f;
                    for (int i = 0; i < n; ++i) { const float m = 0.5f * (io[0][i] + io[1][i]), sd = 0.5f * (io[0][i] - io[1][i]) * w; io[0][i] = m + sd; io[1][i] = m - sd; }
                    return std::abs (w - 1.0f);
                }
                case CustomConfig::gain:
                {
                    const float g = dbToGainF (q[0]);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] *= g;
                    return std::abs (q[0]) / 24.0f;
                }
                default: return 0.0f;
            }
        }
    };
}
