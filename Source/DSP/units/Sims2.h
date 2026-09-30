#pragma once

#include "Sims.h"

/*  Ten more simulated units (see Sims.h): each a small model of a physical thing, its screen drawn live from
    what it publishes (UI/Scene/SimScreens.h reads the layouts documented at each class).
      VALVE AMP     a valve preamp and power stage, the supply sagging    SPEAKER CAB  a speaker and the mic on it
      RADIO         a radio tuned on or off a station                     PENDULUM     a swinging pendulum moving the sound
      BOUNCE DELAY  a ball bouncing: echoes closer and quieter            SYMPATHY     sympathetic strings ringing along
      FLYBY         the sound moving past you (Doppler, distance, air)    TESLA COIL   the sound as a singing arc
      TALK BOX      the sound through a mouth shaping vowels              LAVA LAMP    wax blobs rising and falling, each a resonance */
namespace enh::dsp::units
{
    namespace sim
    {
        /** A state-variable filter (topology-preserving): low, band (unity peak x k) and high at once. */
        struct Svf
        {
            float ic1 = 0.0f, ic2 = 0.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f;
            void set (double sr, double hz, double q) noexcept
            {
                const double g = std::tan (3.141592653589793 * std::clamp (hz, 10.0, 0.45 * sr) / sr), kk = 1.0 / std::max (0.05, q);
                a1 = (float) (1.0 / (1.0 + g * (g + kk))); a2 = (float) g * a1; a3 = (float) g * a2; k = (float) kk;
            }
            void reset() noexcept { ic1 = ic2 = 0.0f; }
            /** Returns the band-pass (unity at the centre); lp, hp out. */
            float process (float x, float& lp, float& hp) noexcept
            {
                const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
                lp = v2; hp = x - k * v1 - v2;
                return k * v1;
            }
        };
        /** An envelope: quick up, slower down. */
        struct Env { float v = 0.0f, up = 0.0f, down = 0.0f;
            void setup (double sr, double a, double r) noexcept { up = coef (sr, a); down = coef (sr, r); }
            float process (float x) noexcept { const float a = std::abs (x); v += (a > v ? up : down) * (a - v); return v; } };
        inline float db (float d) noexcept { return std::pow (10.0f, d / 20.0f); }
    }

    // ------------------------------------------------------------------------------------------------
    /** VALVE AMP: a triode preamp (asymmetric: even harmonics, BIAS moves where it works) into a push-pull
        power stage fed from a supply that sags as it is worked (SAG: the harder it plays, the less headroom
        - the bloom and squash of a valve amp), then the output transformer (no deep lows, no extreme highs).
        TONE tilts it between the preamp and the power stage. Level-matched at the input's own level.
        State: [0] preamp glow 0..1, [1] power glow, [2] sag 0..1, [3] drive 0..1, [4] level. */
    class ValveAmp final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 5) return 0; o[0] = glowPre; o[1] = glowPow; o[2] = sagNow; o[3] = driveNow; o[4] = level; return 5; }
    private:
        std::array<float, 2> tilt {}, hpS {}, lpS {}; sim::Env supply; float glowPre = 0.0f, glowPow = 0.0f, sagNow = 0.0f, driveNow = 0.0f, level = 0.0f;
        void prepareUnit (double s, int) override { supply.setup (s, 0.02, 0.25); }
        void resetUnit() override { tilt = hpS = lpS = {}; supply.v = 0.0f; glowPre = glowPow = sagNow = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float drive = std::clamp (p[1], 0.0f, 10.0f) / 10.0f, bias = (std::clamp (p[2], 0.0f, 10.0f) - 5.0f) / 5.0f;
            const float sag = std::clamp (p[3], 0.0f, 10.0f) / 10.0f, tone = std::clamp (p[4], -5.0f, 5.0f) / 5.0f;
            const float out = sim::db (std::clamp (p[5], -12.0f, 12.0f)), mix = std::clamp (p[6], 0.0f, 100.0f) / 100.0f;
            driveNow = drive;
            const float g1 = 1.0f + 14.0f * drive * drive, b = 0.35f * bias;
            const float make = 0.3f / std::tanh (g1 * 0.3f);   // (a signal at 0.3 comes out at about 0.3)
            const float tk = sim::lp1K (sr, 700.0), hk = sim::lp1K (sr, 55.0), lk = sim::lp1K (sr, 9000.0);
            float pk = 0.0f, pre = 0.0f, pow = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (io[0][i] + io[1][i]);
                const float e = supply.process (m);
                const float head = 1.0f - 0.55f * sag * std::min (1.0f, e * 2.5f);   // the supply, sagging
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    float y = (std::tanh (g1 * x + b) - std::tanh (b)) * make;             // the triode
                    tilt[(size_t) c] += tk * (y - tilt[(size_t) c]);
                    y = y + tone * 0.6f * (tone > 0.0f ? (y - tilt[(size_t) c]) : tilt[(size_t) c] - y) * (tone > 0.0f ? 1.0f : -1.0f);
                    y = head * std::tanh (y * (1.0f + 1.5f * drive) / head) / (1.0f + 1.5f * drive);   // push-pull, in what the supply leaves
                    hpS[(size_t) c] += hk * (y - hpS[(size_t) c]); y -= hpS[(size_t) c];              // the transformer
                    lpS[(size_t) c] += lk * (y - lpS[(size_t) c]); y = lpS[(size_t) c] * out;
                    io[c][i] = x + mix * (y - x);
                    pk = std::max (pk, std::abs (y));
                }
                pre = std::max (pre, std::abs (m) * g1 * 0.25f); pow = std::max (pow, 1.0f - head + std::abs (m));
            }
            const float fall = std::exp (-(float) n / (0.3f * (float) sr));
            glowPre = std::max (std::min (1.0f, pre), glowPre * fall); glowPow = std::max (std::min (1.0f, pow), glowPow * fall);
            sagNow = std::max (1.0f - (1.0f - 0.55f * sag * std::min (1.0f, supply.v * 2.5f)), sagNow * fall) ;
            level = std::max (pk, level * fall);
            setMeter (std::min (1.0f, glowPre * 0.5f + sagNow));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SPEAKER CAB: a guitar-style speaker in its cabinet and the mic in front of it. SIZE (10, 12 or 15 inch)
        sets where it stops in the lows (its resonance, with a bump) and where the cone breaks up at the top;
        MIC POS moves the mic from the dust cap (bright, the breakup peak) to the edge (darker, rounder);
        DISTANCE backs it off (the floor's reflection combs in, a little room); ROOM adds the room around it.
        State: [0] cone excursion -1..1, [1] mic pos 0..1, [2] distance 0..1, [3] size 0/1/2, [4] level. */
    class SpeakerCab final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 5) return 0; o[0] = cone; o[1] = micPos; o[2] = dist; o[3] = (float) size; o[4] = level; return 5; }
    private:
        std::array<sim::Svf, 2> res, brk, lp1, lp2; std::array<float, 2> hpS {}; std::array<DelayLine, 2> refl;
        float cone = 0.0f, coneS = 0.0f, micPos = 0.0f, dist = 0.0f, level = 0.0f; int size = 1;
        void prepareUnit (double s, int) override { for (auto& d : refl) d.setMax ((int) (0.05 * s) + 8); }
        void resetUnit() override { for (auto* a : { &res, &brk, &lp1, &lp2 }) for (auto& f : *a) f.reset(); hpS = {}; for (auto& d : refl) d.clear(); cone = coneS = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            size = std::clamp ((int) std::lround (p[1]), 0, 2);
            micPos = std::clamp (p[2], 0.0f, 10.0f) / 10.0f; dist = std::clamp (p[3], 0.0f, 10.0f) / 10.0f;
            const float room = std::clamp (p[4], 0.0f, 10.0f) / 10.0f, mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            static constexpr float fr[3] { 110.0f, 85.0f, 65.0f }, fb[3] { 3400.0f, 2800.0f, 2000.0f }, top[3] { 6500.0f, 5200.0f, 3800.0f };
            const float edge = 1.0f - 0.55f * micPos;
            for (int c = 0; c < 2; ++c)
            {
                res[(size_t) c].set (sr, fr[size], 1.3); brk[(size_t) c].set (sr, fb[size] * (1.0f - 0.25f * micPos), 2.2);
                lp1[(size_t) c].set (sr, top[size] * edge, 0.7); lp2[(size_t) c].set (sr, top[size] * edge * 1.3f, 0.6);
            }
            const float hk = sim::lp1K (sr, fr[size] * 0.5f), breakG = 0.9f * (1.0f - micPos);
            const float reflD = (0.0008f + 0.0035f * dist) * (float) sr, reflG = 0.2f + 0.35f * dist;
            const float roomG = 0.25f * room + 0.1f * dist, levelG = 1.0f / (1.0f + 0.8f * dist);
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    float lp, hp;
                    res[(size_t) c].process (x, lp, hp);
                    hpS[(size_t) c] += hk * (hp - hpS[(size_t) c]);
                    float y = hp - hpS[(size_t) c];   // below its resonance: 18 dB an octave down (a closed box), its bump at it
                    y += breakG * brk[(size_t) c].process (y, lp, hp);                   // the cone's breakup
                    lp1[(size_t) c].process (y, lp, hp); lp2[(size_t) c].process (lp, y, hp);   // the top falling away
                    refl[(size_t) c].push (y);
                    y = (y + reflG * refl[(size_t) c].tap (reflD) + roomG * (refl[(size_t) c].tap (0.011f * (float) sr) * 0.6f - refl[(size_t) c].tap (0.023f * (float) sr) * 0.45f
                                                                              + refl[(size_t) c].tap (0.037f * (float) sr) * 0.3f)) * levelG;
                    io[c][i] = x + mix * (y - x);
                    pk = std::max (pk, std::abs (y));
                }
                coneS += sim::lp1K (sr, 30.0) * (0.5f * (io[0][i] + io[1][i]) - coneS);   // (the cone: its slow excursion)
            }
            cone = std::clamp (coneS * 4.0f, -1.0f, 1.0f);
            level = std::max (pk, level * std::exp (-(float) n / (0.25f * (float) sr)));
            setMeter (std::min (1.0f, level * 2.0f));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** RADIO: the sound as a radio receives it. BAND: AM (narrow, 150 Hz - 4.5 kHz), SW (narrower, and it fades)
        or FM (wide, a little hiss). TUNE: off the station either way - the signal weakens, the static rises and
        a heterodyne whistles at the difference. STATIC: the crackle and hiss; FADE: the signal coming and going
        (short wave most of all); SPEAKER: the set's small speaker (a honk, and it breaks up). Mono, as a radio is.
        State: [0] tune -1..1, [1] signal 0..1, [2] static 0..1, [3] band, [4] level, [5] fade gain. */
    class Radio final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 6) return 0; o[0] = tuneNow; o[1] = signal; o[2] = staticNow; o[3] = (float) band; o[4] = level; o[5] = fadeNow; return 6; }
    private:
        sim::Svf lo, lo2, hi, hi2, spk, nz; sim::Rng rng; float ph = 0.0f, fadeT = 1.0f, fadeNow = 1.0f, fadePh = 0.0f, crack = 0.0f;
        float tuneNow = 0.0f, signal = 1.0f, staticNow = 0.0f, level = 0.0f; int band = 0;
        void prepareUnit (double, int) override {}
        void resetUnit() override { lo.reset(); lo2.reset(); hi.reset(); hi2.reset(); spk.reset(); nz.reset(); ph = 0.0f; fadeT = fadeNow = 1.0f; fadePh = crack = 0.0f; level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            band = std::clamp ((int) std::lround (p[1]), 0, 2);
            tuneNow = std::clamp (p[2], -10.0f, 10.0f) / 10.0f;
            const float stat = std::clamp (p[3], 0.0f, 10.0f) / 10.0f, fade = std::clamp (p[4], 0.0f, 10.0f) / 10.0f;
            const float speaker = std::clamp (p[5], 0.0f, 10.0f) / 10.0f, mix = std::clamp (p[6], 0.0f, 100.0f) / 100.0f;
            static constexpr float lowHz[3] { 150.0f, 300.0f, 50.0f }, highHz[3] { 4500.0f, 3000.0f, 12000.0f };
            lo.set (sr, lowHz[band], 0.54); lo2.set (sr, lowHz[band], 1.31); hi.set (sr, highHz[band], 0.54); hi2.set (sr, highHz[band], 1.31); spk.set (sr, 1100.0, 1.2); nz.set (sr, highHz[band] * 0.6f, 0.9);
            const float off = std::abs (tuneNow);
            signal = std::clamp (1.0f - 1.6f * off * off, 0.0f, 1.0f) * (0.9f + 0.1f * fadeNow);
            const float whistleHz = 150.0f + 2500.0f * off, whistle = off > 0.02f ? 0.06f * std::min (1.0f, off * 6.0f) * (1.0f - off) : 0.0f;
            staticNow = std::min (1.0f, stat * (0.35f + 0.65f * off) + (band == 1 ? 0.15f : 0.0f) * stat + off * 0.5f);
            const float noiseG = 0.05f * staticNow * (band == 2 ? 0.5f : 1.0f);
            const float fadeDepth = fade * (band == 1 ? 0.85f : band == 0 ? 0.4f : 0.15f);
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                fadePh += 0.3f / (float) sr; if (fadePh >= 1.0f) { fadePh -= 1.0f; fadeT = 1.0f - fadeDepth * rng.next(); }
                fadeNow += sim::lp1K (sr, 0.6) * (fadeT - fadeNow);
                ph += whistleHz / (float) sr; if (ph >= 1.0f) ph -= 1.0f;
                if (rng.next() < stat * stat * 30.0f / (float) sr) crack = rng.bi() * 0.5f;
                crack *= 0.93f;
                const float x = 0.5f * (io[0][i] + io[1][i]);
                float lp, hp;
                lo.process (x, lp, hp); lo2.process (hp, lp, hp); float y = hp;   // (4th-order Butterworth edges: a radio's IF is steep)
                hi.process (y, lp, hp); hi2.process (lp, lp, hp); y = lp * signal * fadeNow;
                float nlp, nhp; const float noise = nz.process (rng.bi(), nlp, nhp) * 2.0f + crack;
                y += noiseG * noise + whistle * std::sin (6.2832f * ph) * (0.3f + std::min (1.0f, std::abs (x) * 3.0f));
                const float honk = spk.process (y, lp, hp);
                y += speaker * 0.8f * honk;
                y = std::tanh (y * (1.0f + 2.0f * speaker)) / (1.0f + 2.0f * speaker) * (1.0f + speaker);
                for (int c = 0; c < 2; ++c) io[c][i] = io[c][i] + mix * (y - io[c][i]);
                pk = std::max (pk, std::abs (y));
            }
            level = std::max (pk, level * std::exp (-(float) n / (0.25f * (float) sr)));
            setMeter (signal * std::min (1.0f, level * 2.0f));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** PENDULUM: a pendulum, swinging for real (gravity, its LENGTH - longer swings slower, as a real one -
        and FRICTION), and it moves the sound: VOLUME (a dip at each end of the swing), PAN (left to right with
        it) or FILTER (a low-pass swept by it). SWING: how far it is kept swinging (a clock's escapement tops it
        up); KICK: the track's hits push it, so it swings with the music.
        State: [0] angle rad, [1] swing rad, [2] length m, [3] mode, [4] level, [5] kick 0..1. */
    class Pendulum final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 6) return 0; o[0] = theta; o[1] = amp; o[2] = len; o[3] = (float) mode; o[4] = level; o[5] = kickNow; return 6; }
    private:
        float theta = 0.3f, omega = 0.0f, amp = 0.3f, len = 1.0f, level = 0.0f, kickNow = 0.0f; int mode = 1;
        sim::Env fast, slow; std::array<float, 2> lp {}; float peakTheta = 0.3f, prevOmega = 0.0f;
        void prepareUnit (double s, int) override { fast.setup (s, 0.001, 0.03); slow.setup (s, 0.05, 0.3); }
        void resetUnit() override { theta = 0.3f; omega = 0.0f; peakTheta = 0.3f; fast.v = slow.v = 0.0f; lp = {}; level = kickNow = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            len = std::clamp (p[1], 10.0f, 300.0f) / 100.0f;
            amp = 0.05f + 0.95f * std::clamp (p[2], 0.0f, 10.0f) / 10.0f;   // radians it is kept swinging to
            const float fric = 0.02f + 0.3f * std::clamp (p[3], 0.0f, 10.0f) / 10.0f;
            mode = std::clamp ((int) std::lround (p[4]), 0, 2);
            const float kick = std::clamp (p[5], 0.0f, 10.0f) / 10.0f, mix = std::clamp (p[6], 0.0f, 100.0f) / 100.0f;
            const float dt = 1.0f / (float) sr, gl = 9.81f / len;
            float pk = 0.0f, kk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (io[0][i] + io[1][i]);
                const float hit = std::max (0.0f, fast.process (m) - 1.6f * slow.process (m));
                kk = std::max (kk, hit);
                // the physics (semi-implicit Euler: stable, and exact enough at audio rate)
                omega += (-gl * std::sin (theta) - fric * omega) * dt;
                // the escapement: as it passes the bottom, a nudge toward the swing it is kept at (its speed there
                // is its swing x sqrt (g / L)); a hit pushes it along too
                omega += (omega >= 0.0f ? 1.0f : -1.0f) * hit * kick * 30.0f * dt;
                const float before = theta;
                theta += omega * dt;
                theta = std::clamp (theta, -1.4f, 1.4f);
                if ((before < 0.0f) != (theta < 0.0f)) omega += (omega >= 0.0f ? 1.0f : -1.0f) * (amp - peakTheta) * std::sqrt (gl) * 0.3f;
                if ((omega >= 0.0f) != (prevOmega >= 0.0f)) peakTheta = std::abs (theta);   // (a turning point: how far it got)
                prevOmega = omega;
                const float mm = std::clamp (theta / std::max (0.05f, amp), -1.0f, 1.0f);
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    float y;
                    if (mode == 0) y = x * (1.0f - 0.7f * mm * mm);
                    else if (mode == 1) { const float a = 0.7854f * (1.0f + mm); y = x * (c == 0 ? std::cos (a) : std::sin (a)) * 1.41421356f; }
                    else { const float hz = 400.0f * std::pow (2.0f, 2.5f * (1.0f - mm * mm) + 1.5f); lp[(size_t) c] += sim::lp1K (sr, hz) * (x - lp[(size_t) c]); y = lp[(size_t) c]; }
                    io[c][i] = x + mix * (y - x);
                    pk = std::max (pk, std::abs (y));
                }
            }
            const float fall = std::exp (-(float) n / (0.25f * (float) sr));
            level = std::max (pk, level * fall); kickNow = std::max (std::min (1.0f, kk * 8.0f), kickNow * fall);
            setMeter (std::abs (theta) / 1.4f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** BOUNCE DELAY: echoes as a dropped ball bounces - the first after HEIGHT, each next one sooner and quieter
        by BOUNCE (its restitution: 0.3 dead, 0.95 lively), until they run together and stop. TONE darkens or
        brightens them, SPREAD throws them left and right, MIX.
        State: [0] seconds since the last drop, [1] first bounce s, [2] restitution, [3] drop height 0..1, [4] level. */
    class BounceDelay final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 5) return 0; o[0] = age; o[1] = t0; o[2] = e; o[3] = dropH; o[4] = level; return 5; }
    private:
        static constexpr int maxTaps = 14;
        std::array<DelayLine, 2> d; std::array<float, 2> tone {}; sim::Env fast, slow;
        float age = 10.0f, t0 = 0.4f, e = 0.7f, dropH = 0.0f, level = 0.0f, tSm = -1.0f;
        void prepareUnit (double s, int) override { for (auto& x : d) x.setMax ((int) (3.2 * s) + 8); fast.setup (s, 0.001, 0.03); slow.setup (s, 0.05, 0.3); }
        void resetUnit() override { for (auto& x : d) x.clear(); tone = {}; age = 10.0f; dropH = level = 0.0f; tSm = -1.0f; fast.v = slow.v = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float t = std::clamp (p[1], 50.0f, 1000.0f) / 1000.0f;
            e = std::clamp (p[2], 0.3f, 0.95f);
            const float tn = std::clamp (p[3], -5.0f, 5.0f) / 5.0f, spread = std::clamp (p[4], 0.0f, 10.0f) / 10.0f, mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            if (tSm < 0.0f) tSm = t;
            tSm += (1.0f - std::exp (-(float) n / (0.2f * (float) sr))) * (t - tSm);
            t0 = tSm;
            // the taps: each bounce e times the one before, in time and in height
            std::array<float, maxTaps> at {}, g {}; int taps = 0; float when = 0.0f, gap = tSm, amp = 0.8f;
            while (taps < maxTaps && when + gap < 3.0f && gap > 0.012f) { when += gap; at[(size_t) taps] = when * (float) sr; g[(size_t) taps] = amp; ++taps; gap *= e; amp *= e; }
            const float tk = sim::lp1K (sr, 5000.0 * std::pow (2.0, 1.5 * tn));
            float pk = 0.0f, hit = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (io[0][i] + io[1][i]);
                hit = std::max (hit, std::max (0.0f, fast.process (m) - 1.6f * slow.process (m)));
                for (int c = 0; c < 2; ++c) d[(size_t) c].push (io[c][i]);
                for (int c = 0; c < 2; ++c)
                {
                    float y = 0.0f;
                    for (int k = 0; k < taps; ++k)
                    {
                        const float side = (k & 1) ? (c == 0 ? 1.0f - spread : 1.0f) : (c == 0 ? 1.0f : 1.0f - spread);
                        y += g[(size_t) k] * side * d[(size_t) c].tap (at[(size_t) k]);
                    }
                    tone[(size_t) c] += tk * (y - tone[(size_t) c]);
                    y = tn >= 0.0f ? y + tn * 0.5f * (y - tone[(size_t) c]) : tone[(size_t) c] + (1.0f + tn) * (y - tone[(size_t) c]);
                    io[c][i] += mix * y;
                    pk = std::max (pk, std::abs (y));
                }
            }
            age += (float) n / (float) sr;
            if (hit > 0.03f && age > 0.25f * tSm) { age = 0.0f; dropH = std::min (1.0f, hit * 6.0f); }
            level = std::max (pk, level * std::exp (-(float) n / (0.25f * (float) sr)));
            setMeter (std::min (1.0f, level * 2.0f));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SYMPATHY: six strings tuned to a chord on NOTE, left open by the sound, so they ring along with it as a
        sitar's or a piano's with the pedal down do. Each is a plucked-string model (Karplus-Strong, fractional
        delay): DECAY how long they ring, BRIGHT how much top they keep. CHORD: OPEN (root, fifth, octave...),
        MAJOR, MINOR, FIFTHS, OCTAVES. MIX: how much of them (the sound itself stays).
        State: [0..5] each string's level, [6] level, [7..12] each string's Hz. */
    class Sympathy final : public RackUnit
    {
    public:
        static constexpr int strings = 6;
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 13) return 0;
            for (int s = 0; s < strings; ++s) { o[s] = amp[(size_t) s]; o[7 + s] = hz[(size_t) s]; }
            o[6] = level; return 13;
        }
    private:
        std::array<DelayLine, strings> line; std::array<float, strings> damp {}, amp {}, hz {}, dcS {};
        float level = 0.0f;
        void prepareUnit (double s, int) override { for (auto& l : line) l.setMax ((int) (s / 25.0) + 8); }
        void resetUnit() override { for (auto& l : line) l.clear(); damp = amp = dcS = {}; level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float note = std::clamp (p[1], 28.0f, 64.0f);
            const int chord = std::clamp ((int) std::lround (p[2]), 0, 4);
            const float decay = 0.4f + 7.6f * std::clamp (p[3], 0.0f, 10.0f) / 10.0f, bright = std::clamp (p[4], 0.0f, 10.0f) / 10.0f;
            const float mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            static constexpr float iv[5][strings] { { 0, 7, 12, 19, 24, 31 }, { 0, 4, 7, 12, 16, 19 }, { 0, 3, 7, 12, 15, 19 }, { 0, 7, 14, 21, 28, 35 }, { 0, 12, 24, 0.07f, 12.07f, 36 } };
            const float dk = 0.25f + 0.7f * bright;
            std::array<float, strings> period {}, fb {};
            for (int s = 0; s < strings; ++s)
            {
                hz[(size_t) s] = 440.0f * std::pow (2.0f, (note + iv[chord][s] - 69.0f) / 12.0f);
                period[(size_t) s] = (float) sr / hz[(size_t) s];
                fb[(size_t) s] = std::pow (10.0f, -3.0f / (decay * hz[(size_t) s])) ;   // (-60 dB after DECAY seconds)
            }
            float pk = 0.0f; std::array<float, strings> sp {};
            for (int i = 0; i < n; ++i)
            {
                const float x = 0.5f * (io[0][i] + io[1][i]);
                float l = 0.0f, r = 0.0f;
                for (int s = 0; s < strings; ++s)
                {
                    auto& ln = line[(size_t) s];
                    const float back = ln.tap (period[(size_t) s] - (1.0f - dk) / dk);   // (the loss filter's own delay comes off, so it stays in tune)
                    damp[(size_t) s] += dk * (back - damp[(size_t) s]);
                    const float y = damp[(size_t) s] * fb[(size_t) s];
                    ln.push (x * 0.08f * std::sqrt (1.0f - fb[(size_t) s]) * 10.0f + y);
                    dcS[(size_t) s] += 0.001f * (y - dcS[(size_t) s]);
                    const float v = y - dcS[(size_t) s];
                    sp[(size_t) s] = std::max (sp[(size_t) s], std::abs (v));
                    const float pan = (float) s / (float) (strings - 1);
                    l += v * (1.0f - 0.6f * pan); r += v * (0.4f + 0.6f * pan);
                }
                l = std::tanh (l * 2.0f) * 0.5f; r = std::tanh (r * 2.0f) * 0.5f;
                io[0][i] += mix * l; io[1][i] += mix * r;
                pk = std::max (pk, std::max (std::abs (l), std::abs (r)));
            }
            const float fall = std::exp (-(float) n / (0.2f * (float) sr));
            for (int s = 0; s < strings; ++s) amp[(size_t) s] = std::max (std::min (1.0f, sp[(size_t) s] * 10.0f), amp[(size_t) s] * fall);
            level = std::max (pk, level * fall);
            setMeter (std::min (1.0f, level * 4.0f));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** FLYBY: the sound as a source moving past you: along a LINE (a car going by, again and again), round a
        CIRCLE in front of you, or a figure EIGHT. Everything follows from where it is: the delay of the sound's
        travel (its pitch rises coming and falls going - Doppler), the level (closer, louder), the air (further,
        duller: AIR), and the side it is on. SPEED in m/s, DISTANCE: how close it passes.
        State: [0] x m, [1] y m, [2] path, [3] distance m, [4] level, [5] Doppler ratio, [6] phase 0..1, [7] speed m/s. */
    class Flyby final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 8) return 0; o[0] = px; o[1] = py; o[2] = (float) path; o[3] = dist; o[4] = level; o[5] = dop; o[6] = u; o[7] = speed; return 8; }

        /** Where the source is at phase u (0..1) of path `path` passing at `d` metres; its path's length. */
        static void at (int path, float d, float u, float& x, float& y) noexcept
        {
            const float a = 6.2832f * u;
            if (path == 0) { x = -60.0f + 120.0f * u; y = d; }
            else if (path == 1) { x = 1.8f * d * std::sin (a); y = 2.0f * d - 1.8f * d * std::cos (a); y = std::max (y, 0.2f * d); }
            else { const float s = 2.2f * d; x = s * std::sin (a); y = d * 1.6f + s * 0.5f * std::sin (2.0f * a); }
        }
        static float lengthOf (int path, float d) noexcept { return path == 0 ? 120.0f : path == 1 ? 6.2832f * 1.8f * d : 6.2832f * 2.6f * d; }
    private:
        std::array<DelayLine, 2> dl; std::array<float, 2> air {};
        float px = 0.0f, py = 5.0f, dist = 8.0f, level = 0.0f, dop = 1.0f, u = 0.0f, speed = 30.0f, prevDelay = -1.0f; int path = 1;
        void prepareUnit (double s, int) override { for (auto& d : dl) d.setMax ((int) (0.6 * s) + 16); }
        void resetUnit() override { for (auto& d : dl) d.clear(); air = {}; u = 0.0f; level = 0.0f; prevDelay = -1.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            speed = std::clamp (p[1], 5.0f, 100.0f); dist = std::clamp (p[2], 1.0f, 50.0f);
            path = std::clamp ((int) std::lround (p[3]), 0, 2);
            const float airA = std::clamp (p[4], 0.0f, 10.0f) / 10.0f, mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            const float du = speed / lengthOf (path, dist) / (float) sr;
            // the block's two ends: delay, gain, air and side glide across it
            auto stateAt = [&] (float uu, float& delay, float& gain, float& side, float& airHz)
            {
                float x, y; at (path, dist, uu, x, y);
                const float r = std::max (0.5f, std::hypot (x, y));
                delay = r / 343.0f * (float) sr;
                gain = std::min (1.0f, dist / r) * (path == 0 ? std::sin (3.14159f * uu) : 1.0f);   // (a line fades in and out at its far ends)
                side = x / r;
                airHz = 20000.0f / (1.0f + airA * r / 12.0f);
            };
            float d0, g0, s0, a0, d1, g1, s1, a1;
            stateAt (u, d0, g0, s0, a0);
            float u1 = u + du * (float) n; const bool wrap = u1 >= 1.0f; if (wrap) u1 -= 1.0f;
            stateAt (u1, d1, g1, s1, a1);
            if (wrap && path == 0) { d1 = d0; }   // (the line starts again far away and silent: no sweep across)
            const float k0 = sim::lp1K (sr, a0), k1 = sim::lp1K (sr, a1);
            float pk = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) i / (float) n;
                const float delay = d0 + (d1 - d0) * t, gain = g0 + (g1 - g0) * t, side = s0 + (s1 - s0) * t, k = k0 + (k1 - k0) * t;
                const float m = 0.5f * (io[0][i] + io[1][i]);
                dl[0].push (m);
                float y = dl[0].tap (delay) * gain;
                air[0] += k * (y - air[0]); y = air[0];
                const float a = 0.7854f * (1.0f + side);
                const float l = y * std::cos (a) * 1.41421356f, r = y * std::sin (a) * 1.41421356f;
                io[0][i] += mix * (l - io[0][i]); io[1][i] += mix * (r - io[1][i]);
                pk = std::max (pk, std::abs (y));
            }
            dop = prevDelay < 0.0f ? 1.0f : std::clamp (1.0f - (d1 - d0) / (float) n, 0.5f, 1.5f);
            prevDelay = d1;
            u = u1; at (path, dist, u, px, py);
            level = std::max (pk, level * std::exp (-(float) n / (0.25f * (float) sr)));
            setMeter (std::min (1.0f, level * 2.0f));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** TESLA COIL: the sound played as a singing Tesla coil plays it - the arc switched on and off at the
        music's own pitch (a buzzing square that follows it), only while the sound is loud enough to strike
        (VOLTAGE: how easily), with the spark's crackle (ARC) and the mains' hum in it (BUZZ). TONE: darker or
        brighter; MIX (the arc is loud: it is level-matched to the input, and kept under it).
        State: [0] arc 0..1, [1] level, [2] seed, [3] voltage 0..1. */
    class TeslaCoil final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 4) return 0; o[0] = arc; o[1] = level; o[2] = (float) (seed % 1000u); o[3] = volt; return 4; }
    private:
        sim::Env env; sim::Rng rng; std::array<float, 2> lp {}, hp {}; float hum = 0.0f, crack = 0.0f, arc = 0.0f, level = 0.0f, volt = 0.0f; unsigned seed = 1u;
        void prepareUnit (double s, int) override { env.setup (s, 0.002, 0.08); }
        void resetUnit() override { env.v = 0.0f; lp = hp = {}; hum = crack = arc = level = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            volt = std::clamp (p[1], 0.0f, 10.0f) / 10.0f;
            const float buzz = std::clamp (p[2], 0.0f, 10.0f) / 10.0f, arcA = std::clamp (p[3], 0.0f, 10.0f) / 10.0f;
            const float tone = std::clamp (p[4], -5.0f, 5.0f) / 5.0f, mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            const float thr = 0.3f * (1.0f - 0.95f * volt) + 0.002f;
            const float lk = sim::lp1K (sr, 2500.0 * std::pow (2.0, 1.8 * tone)), hk = sim::lp1K (sr, 90.0);
            float pk = 0.0f, a = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (io[0][i] + io[1][i]);
                const float e = env.process (m);
                const float on = std::clamp ((e - thr) / (thr + 0.02f), 0.0f, 1.0f);   // (strikes above the threshold, fully a little over it)
                hum += 120.0f / (float) sr; if (hum >= 1.0f) hum -= 1.0f;
                const float s = m >= 0.0f ? 1.0f : -1.0f;
                if (on > 0.0f && rng.next() < arcA * 400.0f / (float) sr) crack = rng.bi();
                crack *= 0.6f;
                const float mains = 1.0f - buzz * 0.7f * (0.5f + 0.5f * std::sin (6.2832f * hum));
                const float y0 = on * e * (0.9f * s * mains + 0.8f * arcA * crack);
                a = std::max (a, on);
                for (int c = 0; c < 2; ++c)
                {
                    lp[(size_t) c] += lk * (y0 - lp[(size_t) c]);
                    hp[(size_t) c] += hk * (lp[(size_t) c] - hp[(size_t) c]);
                    const float y = (lp[(size_t) c] - hp[(size_t) c]) * 0.9f;
                    io[c][i] += mix * (y - io[c][i]);
                    pk = std::max (pk, std::abs (y));
                }
            }
            seed = seed * 1103515245u + 12345u;
            const float fall = std::exp (-(float) n / (0.12f * (float) sr));
            arc = std::max (a, arc * fall); level = std::max (pk, level * fall);
            setMeter (arc);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** TALK BOX: the sound through a mouth. Three formants (the resonances that make a vowel) move between
        A, E, I, O and U: VOWEL sets where it sits, MOVE lets the sound's own level open it through the vowels
        (it talks with the playing), RATE sweeps it on its own, SIZE a smaller or bigger mouth.
        State: [0] vowel 0..4, [1..3] F1..F3 Hz, [4] level, [5] open 0..1. */
    class TalkBox final : public RackUnit
    {
    public:
        int displayState (float* o, int max) const noexcept override
        { if (max < 6) return 0; o[0] = vowel; o[1] = f[0]; o[2] = f[1]; o[3] = f[2]; o[4] = level; o[5] = open; return 6; }
    private:
        std::array<std::array<sim::Svf, 3>, 2> fm; sim::Env env; float ph = 0.0f, vowel = 0.0f, level = 0.0f, open = 0.0f; std::array<float, 3> f { 700, 1200, 2600 };
        void prepareUnit (double s, int) override { env.setup (s, 0.01, 0.15); }
        void resetUnit() override { for (auto& c : fm) for (auto& x : c) x.reset(); env.v = 0.0f; ph = 0.0f; level = open = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float base = std::clamp (p[1], 0.0f, 4.0f), move = std::clamp (p[2], 0.0f, 10.0f) / 10.0f;
            const float rate = std::clamp (p[3], 0.0f, 10.0f) / 10.0f, size = std::clamp (p[4], -5.0f, 5.0f) / 5.0f;
            const float mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            static constexpr float tab[5][3] { { 750, 1200, 2600 }, { 450, 1900, 2600 }, { 300, 2300, 3000 }, { 480, 850, 2500 }, { 330, 700, 2400 } };
            static constexpr float gains[3] { 1.0f, 0.7f, 0.35f };
            float pk = 0.0f, e = 0.0f;
            for (int i0 = 0; i0 < n; i0 += 32)
            {
                const int m = std::min (32, n - i0);
                ph += rate * rate * 2.0f * (float) m / (float) sr; if (ph >= 1.0f) ph -= 1.0f;
                vowel = std::clamp (base + move * std::min (1.0f, env.v * 6.0f) * 3.0f + (rate > 0.0f ? 1.5f * (0.5f - 0.5f * std::cos (6.2832f * ph)) : 0.0f), 0.0f, 4.0f);
                const int a = std::min (3, (int) vowel); const float t = vowel - (float) a;
                const float scale = std::pow (2.0f, -0.4f * size);
                for (int k = 0; k < 3; ++k) { f[(size_t) k] = (tab[a][k] + (tab[a + 1][k] - tab[a][k]) * t) * scale; for (auto& c : fm) c[(size_t) k].set (sr, f[(size_t) k], 6.0 + 3.0 * k); }
                for (int i = i0; i < i0 + m; ++i)
                {
                    e = env.process (0.5f * (io[0][i] + io[1][i]));
                    for (int c = 0; c < 2; ++c)
                    {
                        const float x = io[c][i];
                        float y = 0.0f, lp, hp;
                        for (int k = 0; k < 3; ++k) y += gains[k] * fm[(size_t) c][(size_t) k].process (x, lp, hp);
                        y *= 1.6f;
                        io[c][i] = x + mix * (y - x);
                        pk = std::max (pk, std::abs (y));
                    }
                }
            }
            open = std::clamp ((f[0] - 250.0f) / 550.0f, 0.0f, 1.0f);
            (void) e;
            level = std::max (pk, level * std::exp (-(float) n / (0.25f * (float) sr)));
            setMeter (open);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** LAVA LAMP: BLOBS of wax in a lamp - heated at the bottom they rise, cooled at the top they sink, drifting,
        as a real one does (HEAT: how fast) - and each blob is a resonance in the sound, as high as the blob is
        (around CENTRE), heard on the side of the lamp it is on: slow, organic, never-repeating filter movement.
        DEPTH: how strong. MIX.
        State: [0] n, then n x (x -1..1, y 0..1, radius 0..1, heat 0..1). */
    class LavaLamp final : public RackUnit
    {
    public:
        static constexpr int maxBlobs = 6;
        int displayState (float* o, int max) const noexcept override
        {
            if (max < 1 + 4 * maxBlobs) return 0;
            o[0] = (float) count;
            for (int b = 0; b < maxBlobs; ++b) { o[1 + 4 * b] = blob[(size_t) b].x; o[2 + 4 * b] = blob[(size_t) b].y; o[3 + 4 * b] = blob[(size_t) b].r; o[4 + 4 * b] = blob[(size_t) b].heat; }
            return 1 + 4 * maxBlobs;
        }
    private:
        struct Blob { float x, y, vy, r, heat; };
        std::array<Blob, maxBlobs> blob {}; std::array<std::array<sim::Svf, maxBlobs>, 2> band; int count = 4; bool seeded = false; sim::Rng rng;
        void prepareUnit (double, int) override {}
        void resetUnit() override { seeded = false; for (auto& c : band) for (auto& b : c) b.reset(); }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float heat = 0.1f + std::clamp (p[1], 0.0f, 10.0f) / 10.0f;
            count = std::clamp ((int) std::lround (p[2]), 1, maxBlobs);
            const float depth = std::clamp (p[3], 0.0f, 10.0f) / 10.0f, centre = std::clamp (p[4], 200.0f, 4000.0f), mix = std::clamp (p[5], 0.0f, 100.0f) / 100.0f;
            if (! seeded)
            {
                for (int b = 0; b < maxBlobs; ++b) blob[(size_t) b] = { rng.bi() * 0.6f, rng.next(), 0.0f, 0.35f + 0.5f * rng.next(), rng.next() };
                seeded = true;
            }
            // the wax: warmed near the bottom, cooled near the top; warm rises, cool sinks; the water drags
            const float dt = (float) n / (float) sr;
            for (int b = 0; b < count; ++b)
            {
                auto& w = blob[(size_t) b];
                w.heat += dt * heat * 0.6f * ((w.y < 0.15f ? 1.0f : 0.0f) - (w.y > 0.85f ? 1.0f : 0.0f) - 0.15f * (w.heat - 0.5f));
                w.heat = std::clamp (w.heat, 0.0f, 1.0f);
                w.vy += dt * (heat * 0.5f * (w.heat - 0.5f) - 2.0f * w.vy);
                w.y = std::clamp (w.y + w.vy * dt, 0.0f, 1.0f);
                if (w.y <= 0.0f || w.y >= 1.0f) w.vy *= -0.2f;
                w.x = std::clamp (w.x + dt * 0.05f * rng.bi(), -0.7f, 0.7f);
            }
            for (int b = 0; b < count; ++b)
            {
                const float hz = centre * std::pow (2.0f, (blob[(size_t) b].y - 0.5f) * 4.0f);
                for (auto& c : band) c[(size_t) b].set (sr, hz, 2.0 + 4.0 * blob[(size_t) b].r);
            }
            const float g = 1.6f * depth / std::sqrt ((float) count);
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    float y = x, lp, hp;
                    for (int b = 0; b < count; ++b)
                    {
                        const float side = c == 0 ? 0.5f - 0.5f * blob[(size_t) b].x : 0.5f + 0.5f * blob[(size_t) b].x;
                        y += g * (0.4f + 0.6f * side) * band[(size_t) c][(size_t) b].process (x, lp, hp);
                    }
                    y /= 1.0f + 0.5f * depth;
                    io[c][i] = x + mix * (y - x);
                }
            float avg = 0.0f; for (int b = 0; b < count; ++b) avg += blob[(size_t) b].y;
            setMeter (avg / (float) count);
        }
    };
}
