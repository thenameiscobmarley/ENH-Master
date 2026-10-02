#pragma once

#include "Sims2.h"

/*  Twenty simulated units for MASTERING (3.8.0.1): each a mastering job done by simulating a physical thing,
    each with its own live screen (UI/Scene/SimScreens.h reads the layouts documented at each class).
      CLARITY LENS  presence the mix is hiding, brought forward   SUB DRIVER    a subwoofer cone: tight, controlled sub
      VINYL CUTTER  what a cutting lathe allows: mono bass, tamed top   CAR TEST   the master in a car
      PHONE CHECK   the master on a phone, laptop or earbuds       CLUB SYSTEM   the master on a club's PA
      PRESSURE      a pressure vessel: loud, the valve venting peaks  BALANCE   a see-saw levelling low against high
      STEREO FIELD  width as field lines, bass kept mono           SONAR         transients found and shaped, as pings
      SEISMOGRAPH   bass quakes caught and damped                  PRISM         five colours of saturation
      FURNACE       warmth that builds with heat                   DITHER        the converter's last bits, dithered
      RIDER         an engineer riding the fader to a loudness     COMPASS       left and right brought into phase
      SUSPENSION    dynamics smoothed as a car rides a road        SKYLINE       resonant peaks trimmed back
      HOURGLASS     a limiter whose release follows the sand       AURORA        air, shimmering over the top */
namespace enh::dsp::units
{
    namespace sim
    {
        inline float toDb (float g) noexcept { return 20.0f * std::log10 (std::max (1.0e-9f, g)); }
        inline float clamp01 (float v) noexcept { return std::clamp (v, 0.0f, 1.0f); }
        inline float knob10 (float v) noexcept { return std::clamp (v, 0.0f, 10.0f) / 10.0f; }
        inline float mix100 (float v) noexcept { return std::clamp (v, 0.0f, 100.0f) / 100.0f; }
        inline float fall (int n, double sr, double s) noexcept { return std::exp (-(float) n / (float) (s * sr)); }
    }

    // ------------------------------------------------------------------------------------------------
    /** CLARITY LENS: brings forward the presence the rest of the mix is hiding. It watches how loud the presence
        band (around FOCUS) is against the whole: the more it is buried, the more it is lifted (up to CLARITY's
        worth), and SAFE backs it off where the presence is already strong (no harshness made). AIR opens the top.
        State: [0..3] low, mid, presence, air levels, [4] lift 0..1, [5] focus 0..1 (1 - 6 kHz). */
    class ClarityLens final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 6) return 0; for (int i = 0; i < 4; ++i) o[i] = band[(size_t) i]; o[4] = liftNow; o[5] = focusN; return 6; }
    private:
        std::array<sim::Svf, 2> pres, airF, lowF; sim::Env ef, eb; float gainDb = 0.0f, liftNow = 0.0f, focusN = 0.5f; std::array<float, 4> band {};
        void prepareUnit (double s, int) override { ef.setup (s, 0.01, 0.3); eb.setup (s, 0.01, 0.3); }
        void resetUnit() override { for (auto* a : { &pres, &airF, &lowF }) for (auto& f : *a) f.reset(); ef.v = eb.v = 0.0f; gainDb = 0.0f; band = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float clarity = sim::knob10 (p[1]), focus = std::clamp (p[2], 1000.0f, 6000.0f), air = sim::knob10 (p[3]), safe = sim::knob10 (p[4]), mix = sim::mix100 (p[5]);
            focusN = (focus - 1000.0f) / 5000.0f;
            for (auto& f : pres) f.set (sr, focus, 0.9); for (auto& f : airF) f.set (sr, 11000.0, 0.7); for (auto& f : lowF) f.set (sr, 250.0, 0.7);
            const float gk = sim::coef (sr, 0.08);
            std::array<float, 4> pk {};
            for (int i = 0; i < n; ++i)
            {
                float lp, hp, b[2], a[2], lo[2];
                for (int c = 0; c < 2; ++c) { b[c] = pres[(size_t) c].process (io[c][i], lp, hp); airF[(size_t) c].process (io[c][i], lp, a[c]); lowF[(size_t) c].process (io[c][i], lo[c], hp); }
                const float full = ef.process (0.5f * (io[0][i] + io[1][i])), pr = eb.process (0.5f * (b[0] + b[1]));
                const float share = pr / (full + 1.0e-6f);                     // (how much of the sound the presence is)
                const float buried = sim::clamp01 (1.0f - share * 3.0f);        // (under a third: it is being hidden)
                const float target = clarity * 9.0f * buried * (1.0f - 0.85f * safe * sim::clamp01 (share * 2.5f));
                gainDb += gk * (target - gainDb);
                const float g = sim::db (gainDb) - 1.0f;
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i], y = x + g * b[c] + air * 0.9f * a[c];
                    io[c][i] = x + mix * (y - x);
                }
                pk[0] = std::max (pk[0], std::abs (lo[0])); pk[1] = std::max (pk[1], std::abs (io[0][i] - lo[0] - b[0] - a[0]));
                pk[2] = std::max (pk[2], std::abs (b[0]) * (1.0f + g)); pk[3] = std::max (pk[3], std::abs (a[0]) * (1.0f + air));
            }
            const float fl = sim::fall (n, sr, 0.2);
            for (int k = 0; k < 4; ++k) band[(size_t) k] = std::max (std::min (1.0f, pk[(size_t) k] * 3.0f), band[(size_t) k] * fl);
            liftNow = sim::clamp01 (gainDb / 9.0f);
            setMeter (liftNow);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SUB DRIVER: the lows played through a modelled subwoofer - a cone on its suspension (a mass on a spring,
        damped), tuned to TUNE: what comes out is the cone's motion, so below its tuning the sub rolls away
        cleanly, and at XMAX the suspension stiffens and holds the cone (tight, no flab). TIGHT: how damped it
        is (loose and ringing at 0, tight at 10). SUB: how much of it goes into the sound.
        State: [0] excursion -1..1, [1] xmax 0..1, [2] level, [3] tune Hz. */
    class SubDriver final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 4) return 0; o[0] = exc; o[1] = xmaxN; o[2] = level; o[3] = tune; return 4; }
    private:
        sim::Svf lpIn; float x = 0.0f, v = 0.0f, exc = 0.0f, xmaxN = 0.5f, level = 0.0f, tune = 45.0f;
        void prepareUnit (double, int) override {}
        void resetUnit() override { lpIn.reset(); x = v = exc = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float amt = sim::knob10 (p[1]); tune = std::clamp (p[2], 30.0f, 80.0f); xmaxN = sim::knob10 (p[3]);
            const float zeta = 0.15f + 0.85f * sim::knob10 (p[4]), mix = sim::mix100 (p[5]);
            lpIn.set (sr, 120.0, 0.7);
            const float w = 6.2832f * tune, dt = 1.0f / (float) sr, xm = 0.05f + 0.5f * xmaxN;
            float pk = 0.0f, ex = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float f, hp; lpIn.process (0.5f * (io[0][i] + io[1][i]), f, hp);
                const float a = w * w * (f - x) - 2.0f * zeta * w * v;   // (the cone: driven toward the signal, on its spring)
                v += a * dt; x += v * dt;
                if (std::abs (x) > xm * 0.7f) { const float lim = xm * std::tanh (x / xm); v *= std::abs (x) > 1.0e-9f ? lim / x : 1.0f; x = lim; }   // (the suspension at XMAX)
                const float out = a / (w * w);                           // (what it radiates: its acceleration)
                for (int c = 0; c < 2; ++c) io[c][i] += mix * amt * 1.2f * out;
                pk = std::max (pk, std::abs (out)); ex = std::abs (x) > std::abs (ex) ? x : ex;
            }
            exc = std::clamp (ex / xm, -1.0f, 1.0f);
            level = std::max (pk, level * sim::fall (n, sr, 0.2));
            setMeter (std::abs (exc));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** VINYL CUTTER: what a lacquer-cutting lathe allows a master: the bass made mono below MONO BASS (side-to-side
        bass would throw the stylus out of the groove), the top held back when it is too much for the cutter
        (HF LIMIT: a dynamic cut of the treble, only when it is too loud), and DEPTH: how hard it is cut (a soft
        limit on the groove's swing). State: [0] n (48), then n x (lateral, vertical) groove, [97] hf cut 0..1, [98] level. */
    class VinylCutter final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 99) return 0;
            o[0] = 48.0f;
            for (int i = 0; i < 48; ++i) { const int k = (head + i) % 48; o[1 + 2 * i] = lat[(size_t) k]; o[2 + 2 * i] = vert[(size_t) k]; }
            o[97] = cut; o[98] = level; return 99;
        }
    private:
        sim::Svf sideLp, hfM, hfS; sim::Env hfEnv; std::array<float, 48> lat {}, vert {}; int head = 0, dec = 0; float cut = 0.0f, cutG = 1.0f, level = 0.0f;
        void prepareUnit (double s, int) override { hfEnv.setup (s, 0.001, 0.08); }
        void resetUnit() override { sideLp.reset(); hfM.reset(); hfS.reset(); hfEnv.v = 0.0f; lat = vert = {}; head = dec = 0; cut = 0.0f; cutG = 1.0f; level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float mono = std::clamp (p[1], 50.0f, 300.0f), hfLim = sim::knob10 (p[2]), depth = sim::knob10 (p[3]), mix = sim::mix100 (p[4]);
            sideLp.set (sr, mono, 0.7); hfM.set (sr, 7000.0, 0.7); hfS.set (sr, 7000.0, 0.7);
            const float thr = 0.25f * (1.0f - 0.9f * hfLim) + 0.01f, g = 1.0f + 3.0f * depth * depth;
            float pk = 0.0f, ct = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float l = io[0][i], r = io[1][i];
                float m = 0.5f * (l + r), s = 0.5f * (l - r), lp, hp, hmS;
                sideLp.process (s, lp, hp); s = hp;                               // (no side below MONO BASS)
                hfM.process (m, lp, hp); const float hm = hp; hfS.process (s, lp, hmS);
                const float e = hfEnv.process (hm);
                const float want = e > thr ? thr / e : 1.0f;                      // (the cutter's limit on the treble)
                cutG += (want < cutG ? 0.2f : 0.002f) * (want - cutG);
                m += (cutG - 1.0f) * hm; s += (cutG - 1.0f) * hmS;
                m = std::tanh (m * g) / g * (1.0f + 0.3f * depth); s = std::tanh (s * g) / g * (1.0f + 0.3f * depth);
                const float yl = m + s, yr = m - s;
                io[0][i] = l + mix * (yl - l); io[1][i] = r + mix * (yr - r);
                pk = std::max (pk, std::abs (m)); ct = std::max (ct, 1.0f - cutG);
                if (++dec >= 8) { dec = 0; lat[(size_t) head] = m; vert[(size_t) head] = s; head = (head + 1) % 48; }
            }
            cut = std::max (ct, cut * sim::fall (n, sr, 0.3)); level = std::max (pk, level * sim::fall (n, sr, 0.25));
            setMeter (cut);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** CAR TEST: the master as it sounds in a car - the cabin's own bass lift and its low resonances (bigger with
        CABIN), the speakers (DOOR: bass-heavy, a hole in the low mids; DASH: thin and bright; PREMIUM: with a sub),
        the seat you're in (DRIVER: the near side louder and first; MIDDLE; BACK: the rear deck's boom), and ROAD
        noise under it. State: [0] L level, [1] R level, [2] cabin boom 0..1, [3] road 0..1, [4] speakers, [5] seat. */
    class CarTest final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 6) return 0; o[0] = lv[0]; o[1] = lv[1]; o[2] = boom; o[3] = road; o[4] = (float) spk; o[5] = (float) seat; return 6; }
    private:
        std::array<std::array<sim::Svf, 5>, 2> f; std::array<DelayLine, 2> dl; sim::Rng rng; sim::Svf roadF;
        std::array<float, 2> lv {}; float boom = 0.0f, road = 0.0f; int spk = 0, seat = 0;
        void prepareUnit (double s, int) override { for (auto& d : dl) d.setMax ((int) (0.004 * s) + 8); }
        void resetUnit() override { for (auto& c : f) for (auto& x : c) x.reset(); for (auto& d : dl) d.clear(); roadF.reset(); lv = {}; boom = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float cabin = sim::knob10 (p[1]); spk = std::clamp ((int) std::lround (p[2]), 0, 2);
            road = sim::knob10 (p[3]); seat = std::clamp ((int) std::lround (p[4]), 0, 2); const float mix = sim::mix100 (p[5]);
            static constexpr float hpHz[3] { 45.0f, 110.0f, 28.0f }, lpHz[3] { 11000.0f, 15000.0f, 16000.0f }, holeHz[3] { 400.0f, 250.0f, 350.0f }, holeG[3] { -0.5f, -0.2f, -0.2f };
            for (auto& c : f)
            {
                c[0].set (sr, hpHz[spk], 0.8); c[1].set (sr, lpHz[spk], 0.7); c[2].set (sr, holeHz[spk], 1.2);
                c[3].set (sr, 55.0f + 25.0f * (1.0f - cabin), 3.0); c[4].set (sr, 95.0f + 40.0f * (1.0f - cabin), 4.0);
            }
            roadF.set (sr, 110.0, 0.7);
            const float cabinLift = 0.6f + 1.2f * cabin + (spk == 2 ? 0.8f : 0.0f) + (seat == 2 ? 0.6f : 0.0f);
            const float gl = seat == 0 ? 1.25f : 1.0f, gr = seat == 0 ? 0.8f : 1.0f, delR = seat == 0 ? 0.0012f * (float) sr : 1.0f;
            std::array<float, 2> pk {}; float bm = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float rl, rh; roadF.process (rng.bi(), rl, rh);
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    float lp, hp, y;
                    f[(size_t) c][0].process (x, lp, hp); y = hp;
                    f[(size_t) c][1].process (y, lp, hp); y = lp;
                    y += holeG[spk] * f[(size_t) c][2].process (y, lp, hp);
                    const float m1 = f[(size_t) c][3].process (y, lp, hp), m2 = f[(size_t) c][4].process (y, lp, hp);
                    y += cabinLift * (0.9f * m1 + 0.5f * m2);   // (the cabin's modes: its boom)
                    bm = std::max (bm, std::abs (m1));
                    dl[(size_t) c].push (y);
                    y = c == 0 ? dl[0].tap (1.0f) * gl : dl[1].tap (delR) * gr;
                    y += road * 0.06f * rl;
                    io[c][i] = x + mix * (y * 0.7f - x);
                    pk[(size_t) c] = std::max (pk[(size_t) c], std::abs (y));
                }
            }
            const float fl = sim::fall (n, sr, 0.2);
            for (int c = 0; c < 2; ++c) lv[(size_t) c] = std::max (std::min (1.0f, pk[(size_t) c] * 2.0f), lv[(size_t) c] * fl);
            boom = std::max (std::min (1.0f, bm * 5.0f), boom * fl);
            setMeter (boom);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** PHONE CHECK: the master through a PHONE (a tiny speaker: nothing under 600 Hz, a peak at 2.5 kHz, mono),
        a LAPTOP (little under 250 Hz, a boxy 1.2 kHz) or EARBUDS (full, a bass bump) - at VOLUME, where the
        device's own protection limiter squashes it, as a phone's does turned up.
        State: [0] device, [1] squash 0..1, [2] level, [3] mono 0/1. */
    class PhoneCheck final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 4) return 0; o[0] = (float) dev; o[1] = squash; o[2] = level; o[3] = monoOn ? 1.0f : 0.0f; return 4; }
    private:
        std::array<std::array<sim::Svf, 4>, 2> f; sim::Env env; float gr = 1.0f, squash = 0.0f, level = 0.0f; int dev = 0; bool monoOn = true;
        void prepareUnit (double s, int) override { env.setup (s, 0.0005, 0.12); }
        void resetUnit() override { for (auto& c : f) for (auto& x : c) x.reset(); env.v = 0.0f; gr = 1.0f; squash = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            dev = std::clamp ((int) std::lround (p[1]), 0, 2);
            const float vol = sim::knob10 (p[2]); monoOn = p[3] > 0.5f; const float mix = sim::mix100 (p[4]);
            static constexpr float hp[3] { 600.0f, 250.0f, 40.0f }, pkHz[3] { 2500.0f, 1200.0f, 100.0f }, pkG[3] { 1.0f, 0.8f, 0.6f }, lp[3] { 9000.0f, 14000.0f, 16000.0f };
            for (auto& c : f) { c[0].set (sr, hp[dev], 0.54); c[1].set (sr, hp[dev], 1.31); c[2].set (sr, pkHz[dev], 1.5); c[3].set (sr, lp[dev], 0.7); }
            const float drive = sim::db (12.0f * vol), ceil = 0.5f;
            float pk = 0.0f, sq = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float y2[2];
                const float m = 0.5f * (io[0][i] + io[1][i]);
                for (int c = 0; c < 2; ++c)
                {
                    float y = monoOn ? m : io[c][i], l, h;
                    f[(size_t) c][0].process (y, l, h); f[(size_t) c][1].process (h, l, h); y = h;
                    y += pkG[dev] * f[(size_t) c][2].process (y, l, h);
                    f[(size_t) c][3].process (y, l, h); y2[c] = l * drive;
                }
                const float e = env.process (std::max (std::abs (y2[0]), std::abs (y2[1])));
                const float want = e > ceil ? ceil / e : 1.0f;
                gr += (want < gr ? 0.5f : 0.0008f) * (want - gr);
                for (int c = 0; c < 2; ++c)
                {
                    const float y = std::clamp (y2[c] * gr, -ceil, ceil) / (ceil * 1.4f) * 0.5f;
                    io[c][i] += mix * (y - io[c][i]);
                    pk = std::max (pk, std::abs (y));
                }
                sq = std::max (sq, 1.0f - gr);
            }
            squash = std::max (sq, squash * sim::fall (n, sr, 0.3)); level = std::max (pk, level * sim::fall (n, sr, 0.25));
            setMeter (squash);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** CLUB SYSTEM: the master on a club's PA - its subs (SUB), the room's reflections and tail (SIZE), the CROWD
        soaking up the top and the reverb, and where you stand (DISTANCE: further back, more room, less top).
        State: [0] bass pressure 0..1, [1] level, [2] size, [3] crowd, [4] distance. */
    class ClubSystem final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 5) return 0; o[0] = press; o[1] = level; o[2] = size; o[3] = crowd; o[4] = dist; return 5; }
    private:
        std::array<DelayLine, 2> dl; std::array<sim::Svf, 2> sub, hp30; std::array<float, 2> tailLp {}, fb {}, air {};
        float press = 0.0f, level = 0.0f, size = 0.5f, crowd = 0.5f, dist = 0.4f;
        void prepareUnit (double s, int) override { for (auto& d : dl) d.setMax ((int) (0.25 * s) + 8); }
        void resetUnit() override { for (auto& d : dl) d.clear(); for (auto& x : sub) x.reset(); for (auto& x : hp30) x.reset(); tailLp = fb = air = {}; press = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            size = sim::knob10 (p[1]); const float subA = sim::knob10 (p[2]); crowd = sim::knob10 (p[3]); dist = sim::knob10 (p[4]); const float mix = sim::mix100 (p[5]);
            for (auto& x : sub) x.set (sr, 70.0, 0.9); for (auto& x : hp30) x.set (sr, 30.0, 0.7);
            const float scale = (0.3f + 0.7f * size) * (float) sr;
            const float taps[4] { 0.019f * scale, 0.031f * scale, 0.047f * scale, 0.071f * scale }, tg[4] { 0.35f, -0.28f, 0.22f, -0.16f };
            const float roomG = (0.15f + 0.45f * dist) * (1.0f - 0.5f * crowd), direct = 1.0f - 0.35f * dist;
            const float fbG = 0.55f + 0.25f * size - 0.2f * crowd, lk = sim::lp1K (sr, 6000.0 * (1.0 - 0.6 * crowd)), ak = sim::lp1K (sr, 16000.0 * (1.0 - 0.55 * dist));
            float pk = 0.0f, bp = 0.0f;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    float l, h; hp30[(size_t) c].process (x, l, h); float y = h;
                    const float s = sub[(size_t) c].process (y, l, h);
                    y += subA * 1.3f * s;
                    dl[(size_t) c].push (y + fbG * fb[(size_t) c]);
                    float room = 0.0f; for (int k = 0; k < 4; ++k) room += tg[k] * dl[(size_t) c].tap (taps[k] * (c == 0 ? 1.0f : 1.07f));
                    tailLp[(size_t) c] += lk * (dl[(size_t) c].tap (taps[3] * 1.6f) - tailLp[(size_t) c]); fb[(size_t) c] = tailLp[(size_t) c] * 0.5f;
                    y = direct * y + roomG * (room + fb[(size_t) c]);
                    air[(size_t) c] += ak * (y - air[(size_t) c]); y = air[(size_t) c];
                    io[c][i] = x + mix * (y * 0.8f - x);
                    pk = std::max (pk, std::abs (y)); bp = std::max (bp, std::abs (s) * (1.0f + subA));
                }
            const float fl = sim::fall (n, sr, 0.15);
            press = std::max (std::min (1.0f, bp * 3.0f), press * fl); level = std::max (pk, level * fl);
            setMeter (press);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** PRESSURE: loudness as a pressure vessel - PRESSURE pumps the sound in (gain), CHARACTER how it rounds off
        as it fills (soft saturation), and the VALVE (the ceiling) vents whatever would pass it, easing shut over
        RELEASE. Nothing ever passes the valve. State: [0] fill 0..1, [1] venting 0..1, [2] level, [3] ceiling. */
    class Pressure final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 4) return 0; o[0] = fill; o[1] = vent; o[2] = level; o[3] = ceilN; return 4; }
    private:
        float gr = 1.0f, fill = 0.0f, vent = 0.0f, level = 0.0f, ceilN = 0.9f;
        void prepareUnit (double, int) override {}
        void resetUnit() override { gr = 1.0f; fill = vent = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float g = sim::db (std::clamp (p[1], 0.0f, 24.0f)), ceil = sim::db (std::clamp (p[2], -6.0f, 0.0f));
            const float rel = std::clamp (p[3], 10.0f, 500.0f) / 1000.0f, ch = sim::knob10 (p[4]), mix = sim::mix100 (p[5]);
            ceilN = ceil;
            const float rk = sim::coef (sr, rel);
            float fl = 0.0f, vt = 0.0f, pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float y[2];
                for (int c = 0; c < 2; ++c) { const float u = io[c][i] * g / ceil; y[c] = (1.0f - ch) * io[c][i] * g + ch * ceil * std::tanh (u);   // (unity when small; CHARACTER rounds it into the ceiling)
                }
                const float peak = std::max (std::abs (y[0]), std::abs (y[1]));
                const float want = peak > ceil ? ceil / peak : 1.0f;
                gr = want < gr ? want : gr + rk * (want - gr);
                for (int c = 0; c < 2; ++c)
                {
                    const float out = std::clamp (y[c] * gr, -ceil, ceil);
                    io[c][i] += mix * (out - io[c][i]);
                    pk = std::max (pk, std::abs (out));
                }
                fl = std::max (fl, peak / ceil); vt = std::max (vt, 1.0f - gr);
            }
            const float f = sim::fall (n, sr, 0.3);
            fill = std::max (std::min (1.0f, fl * 0.8f), fill * f); vent = std::max (vt, vent * f); level = std::max (pk, level * f);
            setMeter (vent * 2.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** BALANCE: the low end and the top on a see-saw. It weighs them (over a few seconds: SPEED) against where a
        balanced master sits (moved by TILT) and levels the beam with a gentle tilt EQ about 1 kHz, as far as
        AMOUNT lets it (at most 6 dB either way). State: [0] beam -1..1 (right: top-heavy), [1] correction -1..1, [2] low, [3] high. */
    class Balance final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 4) return 0; o[0] = beam; o[1] = corr / 6.0f; o[2] = loN; o[3] = hiN; return 4; }
    private:
        std::array<float, 2> split {}; sim::Svf loF, hiF; float lowMs = 1.0e-6f, highMs = 1.0e-6f, corr = 0.0f, beam = 0.0f, loN = 0.0f, hiN = 0.0f;
        void prepareUnit (double, int) override {}
        void resetUnit() override { split = {}; loF.reset(); hiF.reset(); lowMs = highMs = 1.0e-6f; corr = beam = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float tilt = std::clamp (p[1], -5.0f, 5.0f), speed = sim::knob10 (p[2]), amount = sim::knob10 (p[3]), mix = sim::mix100 (p[4]);
            loF.set (sr, 250.0, 0.7); hiF.set (sr, 4000.0, 0.7);
            const float wk = sim::coef (sr, 8.0 - 7.0 * speed), ck = sim::coef (sr, 2.0 - 1.5 * speed), sk = sim::lp1K (sr, 1000.0);
            // the weighing (once a block): the top against the bottom, dB, against where a balanced master sits
            const float ratio = 10.0f * std::log10 ((highMs + 1.0e-12f) / (lowMs + 1.0e-12f));
            const float err = std::clamp ((ratio - (-12.0f + 1.5f * tilt)) / 12.0f, -1.0f, 1.0f);
            beam = err;
            const float target = std::clamp (-err * 6.0f * amount, -6.0f, 6.0f) * (lowMs > 1.0e-7f ? 1.0f : 0.0f);
            for (int i = 0; i < n; ++i)
            {
                float l, h; const float m = 0.5f * (io[0][i] + io[1][i]);
                loF.process (m, l, h); lowMs += wk * (l * l - lowMs);
                hiF.process (m, l, h); highMs += wk * (h * h - highMs);
                corr += ck * (target - corr);
                const float gl = sim::db (-0.5f * corr), gh = sim::db (0.5f * corr);
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    split[(size_t) c] += sk * (x - split[(size_t) c]);
                    const float y = split[(size_t) c] * gl + (x - split[(size_t) c]) * gh;
                    io[c][i] = x + mix * (y - x);
                }
            }
            loN = sim::clamp01 ((10.0f * std::log10 (lowMs + 1.0e-12f) + 60.0f) / 60.0f); hiN = sim::clamp01 ((10.0f * std::log10 (highMs + 1.0e-12f) + 60.0f) / 60.0f);
            setMeter (std::abs (corr) / 6.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** STEREO FIELD: the stereo image as the field between two poles. WIDTH spreads or narrows it (the sides up or
        down); below BASS MONO the sides are taken out (the lows centred, as a master's should be); FOCUS firms up
        the centre; SAFE watches the correlation and pulls the width back whenever left and right start to cancel.
        State: [0] width 0..2, [1] correlation -1..1, [2] L level, [3] R level. */
    class StereoField final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 4) return 0; o[0] = widthNow; o[1] = corrN; o[2] = lv[0]; o[3] = lv[1]; return 4; }
    private:
        sim::Svf sLow; float lr = 0.0f, ll = 1.0e-9f, rr = 1.0e-9f, widthNow = 1.0f, corrN = 1.0f, safeG = 1.0f; std::array<float, 2> lv {};
        void prepareUnit (double, int) override {}
        void resetUnit() override { sLow.reset(); lr = 0.0f; ll = rr = 1.0e-9f; widthNow = corrN = safeG = 1.0f; lv = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float width = std::clamp (p[1], 0.0f, 200.0f) / 100.0f, mono = std::clamp (p[2], 20.0f, 300.0f), focus = sim::knob10 (p[3]), safe = sim::knob10 (p[4]), mix = sim::mix100 (p[5]);
            sLow.set (sr, mono, 0.7);
            const float ck = sim::coef (sr, 0.3);
            std::array<float, 2> pk {};
            for (int i = 0; i < n; ++i)
            {
                const float l = io[0][i], r = io[1][i];
                float m = 0.5f * (l + r), s = 0.5f * (l - r), lo, hi;
                sLow.process (s, lo, hi); s = hi;
                m *= 1.0f + 0.25f * focus;
                s *= width * safeG / (1.0f + 0.25f * focus * 0.5f);
                const float yl = m + s, yr = m - s;
                lr += ck * (yl * yr - lr); ll += ck * (yl * yl - ll); rr += ck * (yr * yr - rr);
                const float corr = lr / std::sqrt (ll * rr + 1.0e-12f);
                const float wantSafe = corr < 0.0f ? std::max (0.3f, 1.0f + corr * safe) : 1.0f;
                safeG += 0.0005f * (wantSafe - safeG);
                io[0][i] = l + mix * (yl - l); io[1][i] = r + mix * (yr - r);
                pk[0] = std::max (pk[0], std::abs (yl)); pk[1] = std::max (pk[1], std::abs (yr));
                corrN = corr;
            }
            widthNow = width * safeG;
            for (int c = 0; c < 2; ++c) lv[(size_t) c] = std::max (std::min (1.0f, pk[(size_t) c] * 2.0f), lv[(size_t) c] * sim::fall (n, sr, 0.25));
            setMeter (std::min (1.0f, widthNow * 0.5f));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SONAR: a transient shaper that finds each hit as a sonar finds an echo. ATTACK: the start of each hit up or
        down; SUSTAIN: what follows it; SENSE: how small a hit it answers to. State: [0] sweep 0..1, [1] n, then
        n x (angle 0..1, strength 0..1, age s). */
    class Sonar final : public RackUnit
    {
    public:
        static constexpr int maxPings = 20;
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 2 + 3 * maxPings) return 0;
            o[0] = sweep; o[1] = (float) np;
            for (int k = 0; k < np; ++k) { o[2 + 3 * k] = ping[(size_t) k][0]; o[3 + 3 * k] = ping[(size_t) k][1]; o[4 + 3 * k] = ping[(size_t) k][2]; }
            return 2 + 3 * maxPings;
        }
    private:
        sim::Env fast, slow; float g = 1.0f, sweep = 0.0f, lastT = 0.0f; std::array<std::array<float, 3>, maxPings> ping {}; int np = 0;
        void prepareUnit (double s, int) override { fast.setup (s, 0.0008, 0.04); slow.setup (s, 0.02, 0.25); }
        void resetUnit() override { fast.v = slow.v = 0.0f; g = 1.0f; np = 0; sweep = lastT = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float att = std::clamp (p[1], -10.0f, 10.0f), sus = std::clamp (p[2], -10.0f, 10.0f), sense = sim::knob10 (p[3]), mix = sim::mix100 (p[4]);
            const float gk = sim::coef (sr, 0.002);
            float best = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float m = std::max (std::abs (io[0][i]), std::abs (io[1][i]));
                const float f = fast.process (m), s = slow.process (m);
                const float t = sim::clamp01 ((f - s) / (s + 1.0e-4f) * (0.3f + 1.4f * sense));   // (a hit: the quick level over the slow one)
                const float su = sim::clamp01 ((s - f) / (s + 1.0e-4f) * 2.0f);                     // (its tail: the quick level falling under)
                best = std::max (best, t);
                g += gk * (sim::db (att * 0.9f * t + sus * 0.9f * su) - g);   // (at most 9 dB either way)
                for (int c = 0; c < 2; ++c) { const float x = io[c][i]; io[c][i] = x + mix * (x * g - x); }
            }
            const float dt = (float) n / (float) sr;
            sweep += dt * 0.5f; if (sweep >= 1.0f) sweep -= 1.0f;
            for (int k = 0; k < np;) { ping[(size_t) k][2] += dt; if (ping[(size_t) k][2] > 2.0f) { ping[(size_t) k] = ping[(size_t) (np - 1)]; --np; } else ++k; }
            lastT += dt;
            if (best > 0.35f && lastT > 0.08f && np < maxPings) { ping[(size_t) np++] = { sweep, best, 0.0f }; lastT = 0.0f; }
            setMeter (best);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SEISMOGRAPH: the low end's quakes, caught and damped. Below FREQ the sound is watched (a Linkwitz-Riley
        split: it sums back exactly); when it passes THRESHOLD it is held down (DAMPING: from a light touch to a
        firm hand), the rest untouched. State: [0] n (64), [1..64] the trace (oldest first), [65] damping now 0..1, [66] threshold 0..1. */
    class Seismograph final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 67) return 0;
            o[0] = 64.0f; for (int i = 0; i < 64; ++i) o[1 + i] = trace[(size_t) ((head + i) % 64)];
            o[65] = grN; o[66] = thrN; return 67;
        }
    private:
        std::array<kit::Lr4, 2> split; enh::dsp::SvfCoeffs sc; sim::Env env; float g = 1.0f, grN = 0.0f, thrN = 0.5f; std::array<float, 64> trace {}; int head = 0;
        void prepareUnit (double s, int) override { env.setup (s, 0.01, 0.15); }
        void resetUnit() override { for (auto& s : split) s.reset(); env.v = 0.0f; g = 1.0f; trace = {}; head = 0; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float thr = sim::db (std::clamp (p[1], -40.0f, 0.0f)), damp = sim::knob10 (p[2]), fq = std::clamp (p[3], 40.0f, 200.0f), mix = sim::mix100 (p[4]);
            thrN = (std::clamp (p[1], -40.0f, 0.0f) + 40.0f) / 40.0f;
            sc = enh::dsp::SvfCoeffs::make (sr, fq, 0.70710678);
            const float ratio = 1.5f + 8.5f * damp, gk = sim::coef (sr, 0.01);
            float pk = 0.0f, gm = 1.0f;
            for (int i = 0; i < n; ++i)
            {
                float lo[2], hi[2];
                for (int c = 0; c < 2; ++c) { lo[c] = split[(size_t) c].low (sc, io[c][i]); hi[c] = split[(size_t) c].high (sc, io[c][i]); }
                const float e = env.process (std::max (std::abs (lo[0]), std::abs (lo[1])));
                const float want = e > thr ? std::pow (thr / e, 1.0f - 1.0f / ratio) : 1.0f;
                g += gk * (want - g);
                for (int c = 0; c < 2; ++c) { const float x = io[c][i], y = lo[c] * g + hi[c]; io[c][i] = x + mix * (y - x); }   // (an LR4's two halves sum to an all-pass: flat)
                pk = std::max (pk, e); gm = std::min (gm, g);
            }
            trace[(size_t) head] = std::min (1.0f, pk * 3.0f); head = (head + 1) % 64;
            grN = 1.0f - gm;
            setMeter (grN);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** PRISM: the sound split into five colours (the bands sum back exactly), each saturated as far as its own
        level drives it - WARMTH the low ones, SHINE the high ones, DRIVE all of them, SPREAD how different the
        colours are. The colour is the same at any volume. State: [0..4] each band's glow 0..1, [5] level. */
    class Prism final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 6) return 0; for (int b = 0; b < 5; ++b) o[b] = glow[(size_t) b]; o[5] = level; return 6; }
    private:
        std::array<std::array<float, 4>, 2> lp {}; std::array<std::array<kit::SoftSat, 5>, 2> sat {}; std::array<std::array<kit::Level, 5>, 2> lvl {};
        std::array<float, 5> glow {}; float level = 0.0f;
        void prepareUnit (double s, int) override { for (auto& c : lvl) for (auto& l : c) l.setup (s, 0.2); }
        void resetUnit() override { lp = {}; for (auto& c : sat) for (auto& x : c) x.reset(); for (auto& c : lvl) for (auto& l : c) l.reset(); glow = {}; level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float drive = sim::knob10 (p[1]), spread = sim::knob10 (p[2]), warm = sim::knob10 (p[3]), shine = sim::knob10 (p[4]), mix = sim::mix100 (p[5]);
            static constexpr double edges[4] { 150.0, 600.0, 2500.0, 8000.0 };
            float k[4]; for (int e = 0; e < 4; ++e) k[e] = sim::lp1K (sr, edges[e]);
            float bd[5];
            for (int b = 0; b < 5; ++b)
            {
                const float colour = b < 2 ? warm : b > 2 ? shine : 0.5f;
                bd[b] = (0.3f + 2.2f * drive) * (1.0f - spread * 0.7f + spread * 1.4f * colour);
            }
            std::array<float, 5> gp {}; float pk = 0.0f;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    auto& L = lp[(size_t) c];
                    for (int e = 0; e < 4; ++e) L[(size_t) e] += k[e] * (x - L[(size_t) e]);
                    const float band[5] { L[0], L[1] - L[0], L[2] - L[1], L[3] - L[2], x - L[3] };   // (sums back to x exactly)
                    float add = 0.0f;
                    for (int b = 0; b < 5; ++b)
                    {
                        const float lv = lvl[(size_t) c][(size_t) b].process (band[b]);
                        const float h = kit::harmonicsAt (sat[(size_t) c][(size_t) b], band[b], lv, bd[b], 0.12f);
                        add += h * (0.8f + 0.8f * drive);
                        gp[(size_t) b] = std::max (gp[(size_t) b], std::abs (h) * 20.0f + lv * 0.5f);
                    }
                    const float y = x + add;
                    io[c][i] = x + mix * (y - x);
                    pk = std::max (pk, std::abs (y));
                }
            const float fl = sim::fall (n, sr, 0.2);
            for (int b = 0; b < 5; ++b) glow[(size_t) b] = std::max (std::min (1.0f, gp[(size_t) b]), glow[(size_t) b] * fl);
            level = std::max (pk, level * fl);
            setMeter (drive * std::min (1.0f, level * 2.0f));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** FURNACE: warmth that builds with heat. The sound's power heats it (HEAT), it cools on its own (COOLING);
        the hotter it runs, the harder it saturates, the darker it glows (the top rolls off) and the more it
        gives (thermal compression, as a voice coil or a valve heats). COLOUR: how much it darkens.
        State: [0] temperature 0..1, [1] level, [2] drive 0..1. */
    class Furnace final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 3) return 0; o[0] = temp; o[1] = level; o[2] = driveN; return 3; }
    private:
        std::array<float, 2> lp {}; float temp = 0.0f, level = 0.0f, driveN = 0.0f;
        void prepareUnit (double, int) override {}
        void resetUnit() override { lp = {}; temp = level = driveN = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float heat = sim::knob10 (p[1]), cool = 0.05f + sim::knob10 (p[2]), colour = sim::knob10 (p[3]), mix = sim::mix100 (p[4]);
            float pw = 0.0f; for (int i = 0; i < n; ++i) pw += 0.5f * (io[0][i] * io[0][i] + io[1][i] * io[1][i]);
            const float dt = (float) n / (float) sr;
            temp = sim::clamp01 (temp + dt * (heat * 40.0f * pw / (float) n - cool * temp * 0.8f));
            const float d = 1.0f + 5.0f * temp * (0.3f + heat), comp = 1.0f / (1.0f + 0.6f * temp), k = sim::lp1K (sr, 18000.0 * (1.0 - 0.75 * colour * temp) + 1500.0);
            driveN = (d - 1.0f) / 6.5f;
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    float y = std::tanh (x * d) / d * comp * (1.0f + 0.25f * (d - 1.0f));
                    lp[(size_t) c] += k * (y - lp[(size_t) c]); y = lp[(size_t) c];
                    io[c][i] = x + mix * (y - x);
                    pk = std::max (pk, std::abs (y));
                }
            level = std::max (pk, level * sim::fall (n, sr, 0.25));
            setMeter (temp);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** DITHER: the converter's last step - the master reduced to BITS (16, 20 or 24), with DITHER: OFF (the
        error left as distortion), TPDF (flat noise that turns it into a gentle hiss) or SHAPED (the hiss pushed
        up where the ear hears least). The last thing on a master. State: [0] bits, [1] mode, [2] n (48),
        then n x (in, out) in steps of the last bit about the first sample. */
    class Dither final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 99) return 0;
            o[0] = (float) bits; o[1] = (float) mode; o[2] = 48.0f;
            const float q = std::ldexp (1.0f, bits - 1), ref = std::round (in[(size_t) head] * q);
            for (int i = 0; i < 48; ++i) { const int k = (head + i) % 48; o[3 + 2 * i] = std::clamp (in[(size_t) k] * q - ref, -12.0f, 12.0f); o[4 + 2 * i] = std::clamp (out[(size_t) k] * q - ref, -12.0f, 12.0f); }
            return 99;
        }
    private:
        sim::Rng rng; std::array<float, 2> e1 {}; std::array<float, 48> in {}, out {}; int head = 0, bits = 16, mode = 1;
        void prepareUnit (double, int) override {}
        void resetUnit() override { e1 = {}; in = out = {}; head = 0; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            static constexpr int b[3] { 16, 20, 24 };
            bits = b[std::clamp ((int) std::lround (p[1]), 0, 2)]; mode = std::clamp ((int) std::lround (p[2]), 0, 2);
            const float q = std::ldexp (1.0f, bits - 1);
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    const float w = mode == 2 ? x - e1[(size_t) c] : x;
                    const float d = mode == 0 ? 0.0f : (rng.next() - rng.next()) / q;
                    const float y = std::clamp (std::round ((w + d) * q) / q, -1.0f, 1.0f - 1.0f / q);
                    if (mode == 2) e1[(size_t) c] = y - w;
                    io[c][i] = y;
                    if (c == 0 && i >= n - 48) { in[(size_t) head] = x; out[(size_t) head] = y; head = (head + 1) % 48; }
                }
            setMeter (bits == 16 ? 1.0f : bits == 20 ? 0.6f : 0.3f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** RIDER: an engineer's hand on the fader, riding the master toward TARGET loudness - slowly, as a person does
        (SPEED), never more than RANGE either way, and only while there is sound (it holds in silence).
        State: [0] fader -1..1 (of RANGE), [1] loudness 0..1, [2] target 0..1, [3] n (48), [4..51] loudness history. */
    class Rider final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 52) return 0;
            o[0] = faderN; o[1] = loudN; o[2] = targetN; o[3] = 48.0f;
            for (int i = 0; i < 48; ++i) o[4 + i] = hist[(size_t) ((head + i) % 48)];
            return 52;
        }
    private:
        std::array<sim::Svf, 2> kw; float ms = 0.0f, fader = 0.0f, faderN = 0.0f, loudN = 0.0f, targetN = 0.5f, histT = 0.0f; std::array<float, 48> hist {}; int head = 0;
        void prepareUnit (double, int) override {}
        void resetUnit() override { for (auto& f : kw) f.reset(); ms = fader = 0.0f; hist = {}; head = 0; histT = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float target = std::clamp (p[1], -24.0f, -6.0f), speed = 0.3f + 5.7f * sim::knob10 (p[2]), range = std::clamp (p[3], 0.0f, 12.0f), mix = sim::mix100 (p[4]);
            for (auto& f : kw) f.set (sr, 80.0, 0.7);
            const float k = sim::coef (sr, 0.4);
            for (int i = 0; i < n; ++i)
            {
                float l, h, s = 0.0f;
                for (int c = 0; c < 2; ++c) { kw[(size_t) c].process (io[c][i], l, h); s += h * h; }
                ms += k * (0.5f * s - ms);
            }
            const float loud = 10.0f * std::log10 (ms + 1.0e-12f) - 0.7f;   // (about LUFS, momentary)
            const float dt = (float) n / (float) sr;
            if (loud > -50.0f)
            {
                const float want = std::clamp (target - loud, -range, range);
                fader += std::clamp (want - fader, -speed * dt, speed * dt);
            }
            const float g = sim::db (fader);
            for (int i = 0; i < n; ++i) for (int c = 0; c < 2; ++c) { const float x = io[c][i]; io[c][i] = x + mix * (x * g - x); }
            faderN = range > 0.0f ? fader / range : 0.0f; loudN = sim::clamp01 ((loud + 40.0f) / 40.0f); targetN = (target + 40.0f) / 40.0f;
            histT += dt; if (histT > 0.1f) { histT = 0.0f; hist[(size_t) head] = sim::clamp01 ((loud + fader + 40.0f) / 40.0f); head = (head + 1) % 48; }
            setMeter (std::abs (faderN));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** COMPASS: finds how far left and right have drifted apart in time (a mic further off, a track nudged) by
        comparing them, and brings them back into line - ALIGN how far, within RANGE; on the BASS only (where it
        matters most, and phase-cancels in mono) or the FULL range. State: [0] correlation -1..1, [1] lag -1..1 (of
        RANGE), [2] applied 0..1, [3] level. */
    class Compass final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 4) return 0; o[0] = corrN; o[1] = lagN; o[2] = applied; o[3] = level; return 4; }
    private:
        static constexpr int maxLag = 32, decim = 4;
        std::array<kit::Lr4, 2> split; enh::dsp::SvfCoeffs sc; std::array<DelayLine, 2> dl; std::array<float, 2 * maxLag + 1> xc {}, rHist {}, lHist {};
        float delayNow = 0.0f, corrN = 1.0f, lagN = 0.0f, applied = 0.0f, level = 0.0f, lAcc = 0.0f, rAcc = 0.0f; int dcount = 0, rHead = 0;
        void prepareUnit (double s, int) override { for (auto& d : dl) d.setMax ((int) (0.003 * s) + 16); }
        void resetUnit() override { for (auto& s : split) s.reset(); for (auto& d : dl) d.clear(); xc = rHist = lHist = {}; delayNow = 0.0f; lAcc = rAcc = 0.0f; dcount = rHead = 0; level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float align = sim::knob10 (p[1]), range = std::clamp (p[2], 0.1f, 2.0f) / 1000.0f; const bool bassOnly = p[3] > 0.5f; const float mix = sim::mix100 (p[4]);
            sc = enh::dsp::SvfCoeffs::make (sr, 300.0, 0.70710678);
            const int lags = std::clamp ((int) (range * (float) sr / (float) decim), 1, maxLag);
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float lo[2], hi[2];
                for (int c = 0; c < 2; ++c) { lo[c] = split[(size_t) c].low (sc, io[c][i]); hi[c] = split[(size_t) c].high (sc, io[c][i]); }
                // the comparison, on the lows, every 4th sample: L now against R across the lags
                lAcc += lo[0]; rAcc += lo[1];
                if (++dcount >= decim)
                {
                    dcount = 0;
                    constexpr int n2 = 2 * maxLag + 1;
                    rHist[(size_t) rHead] = rAcc; lHist[(size_t) rHead] = lAcc; lAcc = rAcc = 0.0f;
                    // L from maxLag steps ago against R from maxLag - k steps ago: every lag either way, all in the past
                    const float l = lHist[(size_t) ((rHead - maxLag + 4 * n2) % n2)];
                    for (int k = -lags; k <= lags; ++k)
                    {
                        const float r = rHist[(size_t) ((rHead - maxLag - k + 4 * n2) % n2)];
                        auto& x = xc[(size_t) (k + maxLag)]; x += 0.002f * (l * r - x);
                    }
                    rHead = (rHead + 1) % (2 * maxLag + 1);
                }
                // the later side delayed by nothing, the earlier by the lag found (smoothly)
                for (int c = 0; c < 2; ++c) dl[(size_t) c].push (bassOnly ? lo[c] : io[c][i]);
                const float d0 = std::max (0.0f, delayNow), d1 = std::max (0.0f, -delayNow);
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    const float moved = dl[(size_t) c].tap (1.0f + (c == 0 ? d0 : d1));
                    const float y = bassOnly ? moved + hi[c] : moved;
                    io[c][i] = x + mix * (y - x);
                    pk = std::max (pk, std::abs (y));
                }
            }
            int best = 0; float bv = -1.0e30f;
            for (int k = -lags; k <= lags; ++k) if (xc[(size_t) (k + maxLag)] > bv) { bv = xc[(size_t) (k + maxLag)]; best = k; }
            const float zero = xc[(size_t) maxLag], norm = std::max (1.0e-12f, std::abs (bv));
            corrN = std::clamp (zero / norm, -1.0f, 1.0f);
            lagN = (float) best / (float) lags;
            const float want = -(float) (best * decim) * align;   // (the peak at a negative lag: R is late by that - so L is held back as much)
            delayNow += 0.05f * (want - delayNow);
            applied = std::min (1.0f, std::abs (delayNow) / std::max (1.0f, range * (float) sr));
            level = std::max (pk, level * sim::fall (n, sr, 0.25));
            setMeter (applied);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SUSPENSION: the dynamics smoothed as a car's suspension smooths a road. The road is the sound's level;
        the car's body rides it on a SPRING (how quickly it follows) and a DAMPER (how settled): the difference
        between the two is the gain - a bump (a jump in level) is soaked up, a dip is filled, never more than
        TRAVEL. State: [0] road 0..1, [1] body 0..1, [2] wheel turn 0..1, [3] n (32), [4..35] road history. */
    class Suspension final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 36) return 0;
            o[0] = roadN; o[1] = bodyN; o[2] = wheel; o[3] = 32.0f;
            for (int i = 0; i < 32; ++i) o[4 + i] = hist[(size_t) ((head + i) % 32)];
            return 36;
        }
    private:
        sim::Env env; float body = -30.0f, vel = 0.0f, gainDb = 0.0f, roadN = 0.0f, bodyN = 0.0f, wheel = 0.0f, histT = 0.0f; std::array<float, 32> hist {}; int head = 0, sub = 0; float roadDb = -60.0f;
        void prepareUnit (double s, int) override { env.setup (s, 0.003, 0.06); }
        void resetUnit() override { env.v = 0.0f; body = -30.0f; vel = 0.0f; gainDb = 0.0f; hist = {}; head = sub = 0; roadDb = -60.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float fs = 0.5f + 7.5f * sim::knob10 (p[1]), zeta = 0.2f + 1.0f * sim::knob10 (p[2]), travel = std::clamp (p[3], 0.0f, 12.0f), mix = sim::mix100 (p[4]);
            const float w = 6.2832f * fs, dt = 16.0f / (float) sr;
            for (int i = 0; i < n; ++i)
            {
                roadDb = sim::toDb (env.process (std::max (std::abs (io[0][i]), std::abs (io[1][i]))) + 1.0e-5f);
                if (++sub >= 16)   // (the body: a mass on its spring and damper, at 1/16 the rate)
                {
                    sub = 0;
                    const float a = w * w * (roadDb - body) - 2.0f * zeta * w * vel;
                    vel += a * dt; body += vel * dt;
                    gainDb = roadDb > -60.0f ? std::clamp (body - roadDb, -travel, travel) : 0.0f;
                }
                const float g = sim::db (gainDb);
                for (int c = 0; c < 2; ++c) { const float x = io[c][i]; io[c][i] = x + mix * (x * g - x); }
            }
            const float t = (float) n / (float) sr;
            wheel += t * 1.5f; if (wheel >= 1.0f) wheel -= 1.0f;
            roadN = sim::clamp01 ((roadDb + 60.0f) / 60.0f); bodyN = sim::clamp01 ((body + 60.0f) / 60.0f);
            histT += t; if (histT > 0.05f) { histT = 0.0f; hist[(size_t) head] = roadN; head = (head + 1) % 32; }
            setMeter (std::abs (gainDb) / std::max (1.0f, travel));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SKYLINE: the spectrum as a city's skyline - 16 bands - and any tower standing well above its neighbours
        (a resonance, a ringing note) trimmed back to their height, as fast as SPEED, as far as DEPTH (up to
        12 dB); SHARPNESS: how narrow a peak it looks for; FOCUS: the FULL range, the MIDS or the HIGHS only.
        State: [0..15] each band's level 0..1, [16..31] each band's trim 0..1. */
    class Skyline final : public RackUnit
    {
    public:
        static constexpr int bands = 16;
        int displayState (float* o, int max) const noexcept override { if (max < 32) return 0; for (int b = 0; b < bands; ++b) { o[b] = lvN[(size_t) b]; o[16 + b] = trimN[(size_t) b]; } return 32; }
    private:
        std::array<std::array<sim::Svf, bands>, 2> f; std::array<float, bands> env {}, g {}, lvN {}, trimN {};
        void prepareUnit (double s, int) override { for (auto& c : f) for (int b = 0; b < bands; ++b) c[(size_t) b].set (s, 90.0 * std::pow (2.0, 7.2 * b / 15.0), 4.0); g.fill (0.0f); }
        void resetUnit() override { for (auto& c : f) for (auto& x : c) x.reset(); env = g = lvN = trimN = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float depth = sim::knob10 (p[1]), sharp = sim::knob10 (p[2]), speed = sim::knob10 (p[3]); const int focus = std::clamp ((int) std::lround (p[4]), 0, 2); const float mix = sim::mix100 (p[5]);
            for (auto& c : f) for (int b = 0; b < bands; ++b) c[(size_t) b].set (sr, 90.0 * std::pow (2.0, 7.2 * b / 15.0), 2.0 + 6.0 * sharp);
            const float ek = sim::coef (sr, 0.02), gk = 1.0f - sim::fall (n, sr, 0.3 - 0.27 * speed);
            std::array<float, bands> pk {};
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    float y = x, l, h;
                    for (int b = 0; b < bands; ++b)
                    {
                        const float bp = f[(size_t) c][(size_t) b].process (x, l, h);
                        if (c == 0) env[(size_t) b] += ek * (std::abs (bp) - env[(size_t) b]);
                        y -= g[(size_t) b] * bp;
                        pk[(size_t) b] = std::max (pk[(size_t) b], std::abs (bp));
                    }
                    io[c][i] = x + mix * (y - x);
                }
            // the towers above their neighbours: trimmed back toward them
            for (int b = 0; b < bands; ++b)
            {
                const bool inFocus = focus == 0 || (focus == 1 && b >= 3 && b <= 11) || (focus == 2 && b >= 9);
                const float nb = 0.5f * (env[(size_t) std::max (0, b - 1)] + env[(size_t) std::min (bands - 1, b + 1)]) + 1.0e-6f;
                const float over = env[(size_t) b] / nb;
                const float want = inFocus && over > 1.4f - 0.3f * sharp ? std::min (1.0f - sim::db (-12.0f * depth), (over - 1.0f) * 0.5f * depth) : 0.0f;
                g[(size_t) b] += gk * (want - g[(size_t) b]);
                g[(size_t) b] = std::clamp (g[(size_t) b], 0.0f, 0.75f);
                lvN[(size_t) b] = sim::clamp01 ((sim::toDb (pk[(size_t) b] + 1.0e-6f) + 60.0f) / 60.0f);
                trimN[(size_t) b] = g[(size_t) b] / 0.75f;
            }
            float mx = 0.0f; for (float v : trimN) mx = std::max (mx, v);
            setMeter (mx);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** HOURGLASS: a clean limiter whose release is an hourglass - peaks over CEILING (after DRIVE) run the sand
        down at once, and it runs back over RELEASE; the longer it has been holding, the more sand is at the
        bottom and the slower it returns (program-dependent: no pumping on dense material, quick on sparse hits).
        State: [0] sand at the top 0..1, [1] flow 0..1, [2] level, [3] ceiling. */
    class Hourglass final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 4) return 0; o[0] = 1.0f - sand; o[1] = flow; o[2] = level; o[3] = ceilN; return 4; }
    private:
        float gr = 1.0f, sand = 0.0f, flow = 0.0f, level = 0.0f, ceilN = 0.95f;
        void prepareUnit (double, int) override {}
        void resetUnit() override { gr = 1.0f; sand = flow = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float ceil = sim::db (std::clamp (p[1], -6.0f, 0.0f)), drive = sim::db (std::clamp (p[2], 0.0f, 12.0f)), rel = std::clamp (p[3], 10.0f, 1000.0f) / 1000.0f;
            ceilN = ceil;
            const float sandUp = sim::coef (sr, 1.5), sandDown = sim::coef (sr, 3.0);
            float fl = 0.0f, pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float a = io[0][i] * drive, b = io[1][i] * drive, peak = std::max (std::abs (a), std::abs (b));
                const float want = peak > ceil ? ceil / peak : 1.0f;
                const float holding = gr < 0.97f ? 1.0f : 0.0f;
                sand += (holding > 0.5f ? sandUp : sandDown) * (holding - sand);
                const float rk = sim::coef (sr, rel * (1.0f + 3.0f * sand));
                gr = want < gr ? want : gr + rk * (want - gr);
                const float yl = std::clamp (a * gr, -ceil, ceil), yr = std::clamp (b * gr, -ceil, ceil);
                io[0][i] = yl; io[1][i] = yr;
                fl = std::max (fl, 1.0f - gr); pk = std::max (pk, std::max (std::abs (yl), std::abs (yr)));
            }
            flow = std::max (fl, flow * sim::fall (n, sr, 0.3)); level = std::max (pk, level * sim::fall (n, sr, 0.25));
            setMeter (flow * 2.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** AURORA: air over the top - the upper presence (3 - 8 kHz) drives a generator of harmonics above SHEEN, laid
        over the sound (AIR), with the top that is already there opened a touch; SMOOTH holds it back whenever the
        top is already bright, so it shimmers without ever turning harsh. State: [0..7] curtains 0..1, [8] level. */
    class Aurora final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override { if (max < 9) return 0; for (int k = 0; k < 8; ++k) o[k] = cur[(size_t) k]; o[8] = level; return 9; }
    private:
        std::array<sim::Svf, 2> src, hpG, top; sim::Env hfe; std::array<float, 8> cur {}; std::array<float, 2> dcS {}; float level = 0.0f; int turn = 0;
        void prepareUnit (double s, int) override { hfe.setup (s, 0.002, 0.1); }
        void resetUnit() override { for (auto* a : { &src, &hpG, &top }) for (auto& f : *a) f.reset(); hfe.v = 0.0f; cur = {}; dcS = {}; level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float air = sim::knob10 (p[1]), sheen = std::clamp (p[2], 5000.0f, 16000.0f), smooth = sim::knob10 (p[3]), mix = sim::mix100 (p[4]);
            for (auto& f : src) f.set (sr, 5000.0, 0.8); for (auto& f : hpG) f.set (sr, sheen, 0.7); for (auto& f : top) f.set (sr, sheen, 0.7);
            float pk = 0.0f, hp = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float l, h, topSum = 0.0f;
                const float hold = 1.0f / (1.0f + smooth * 12.0f * hfe.v);   // (how bright the top already is: the last sample's)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    const float b = src[(size_t) c].process (x, l, h);
                    float gen = std::abs (b) * 2.0f;                        // (rectified: its even harmonics, an octave up and on)
                    dcS[(size_t) c] += 0.001f * (gen - dcS[(size_t) c]); gen -= dcS[(size_t) c];
                    hpG[(size_t) c].process (gen, l, h); gen = h;
                    top[(size_t) c].process (x, l, h); topSum += 0.5f * h;
                    const float y = x + air * hold * (1.2f * gen + 0.35f * h);
                    io[c][i] = x + mix * (y - x);
                    pk = std::max (pk, std::abs (y)); hp = std::max (hp, std::abs (gen) * air * hold);
                }
                hfe.process (topSum);
            }
            const float fl = sim::fall (n, sr, 0.5);
            turn = (turn + 1) % 8;
            for (int k = 0; k < 8; ++k) cur[(size_t) k] *= fl;
            cur[(size_t) turn] = std::max (cur[(size_t) turn], std::min (1.0f, hp * 12.0f));
            level = std::max (pk, level * sim::fall (n, sr, 0.25));
            setMeter (std::min (1.0f, hp * 12.0f));
        }
    };
}
