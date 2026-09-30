#pragma once

#include "RackUnit.h"
#include "LbList.h"
#include "../UnitKit.h"

/*  The LUNCHBOX's 500-series modules beyond its first four (Tools/units/gen_units.py lists them). Each is a
    RackUnit: IN (p[0]) fades it in and out over 30 ms and it is bit-for-bit out when OUT; a guard and a
    +6 dBFS soft ceiling; no latency. Built to the standard of the CLASS-A EQ: every filter's coefficients
    follow gliding values (no zipper noise), every level detector is linked across the two sides (the image
    never moves), and nothing is added that wasn't asked for. */
namespace enh::dsp::units::lb
{
    /** A knob's value gliding over ~30 ms, at the block rate. */
    struct Smooth
    {
        float v = 0.0f; bool set = false;
        float step (float target, int n, double sr) noexcept
        {
            if (! set) { v = target; set = true; return v; }
            const float k = 1.0f - (float) std::exp (-(double) n / (0.030 * sr));
            v += k * (target - v);
            if (std::abs (target - v) < 1.0e-4f * std::max (1.0f, std::abs (target))) v = target;
            return v;
        }
    };
    /** Stereo biquad, redesigned only when its value moved. */
    struct Bq2
    {
        BiquadCoeffs c; std::array<BiquadState, 2> s {};
        void reset() noexcept { s = {}; }
        float run (int ch, float x) noexcept { return s[(size_t) ch].process (c, x); }
    };
    inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
    inline float coefFor (double sr, double seconds) noexcept { return 1.0f - (float) std::exp (-1.0 / (std::max (1.0e-5, seconds) * sr)); }
    inline int choice (float v, int n) noexcept { return std::clamp ((int) std::lround (v), 0, n - 1); }

    // ------------------------------------------------------------------------------------------------
    /** PREAMP: a clean line/mic amplifier that warms up as it is driven. GAIN in, a low cut (12 dB/oct),
        DRIVE: a biased soft curve driven against the signal's own level (the same colour at any volume;
        even harmonics first, like a single-ended stage), then OUTPUT and PHASE. At DRIVE 0 it is a wire with
        gain. */
    class Preamp final : public RackUnit
    {
        Smooth gain, drive, out; Bq2 hp; int cutNow = -1; std::array<kit::SoftSat, 2> sat {}; std::array<kit::Level, 2> lvl {};
        std::array<float, 2> dcX {}, dcY {}; float dcR = 0.999f;
        void prepareUnit (double s, int) override { for (auto& l : lvl) l.setup (s, 0.3); dcR = (float) std::exp (-2.0 * 3.141592653589793 * 8.0 / s); cutNow = -1; }
        void resetUnit() override { hp.reset(); for (auto& x : sat) x.reset(); for (auto& l : lvl) l.reset(); dcX = {}; dcY = {}; gain.set = drive.set = out.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float g = dbToGainF (gain.step (std::clamp (p[1], -12.0f, 24.0f), n, sr));
            const float d = drive.step (std::clamp (p[2], 0.0f, 10.0f), n, sr) / 10.0f;
            const int cut = choice (p[3], 4);
            const float o = dbToGainF (out.step (std::clamp (p[4], -24.0f, 12.0f), n, sr)) * (p[5] > 0.5f ? -1.0f : 1.0f);
            if (cut != cutNow) { cutNow = cut; static constexpr float hz[4] { 0.0f, 30.0f, 60.0f, 120.0f }; if (cut > 0) hp.c = BiquadCoeffs::highPass (sr, hz[cut], 0.7071); }
            const float amount = 1.6f * d * d + 0.25f * d, bias = 0.12f + 0.2f * d;
            float work = 0.0f;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float x = io[c][i] * g;
                    if (cutNow > 0) x = hp.run (c, x);
                    if (amount > 1.0e-4f)
                    {
                        const float l = lvl[(size_t) c].process (x);
                        float h = kit::harmonicsAt (sat[(size_t) c], x, l, 0.35f + amount, bias) * std::min (1.0f, amount * 2.0f);
                        // (the even harmonics' offset leaves through a DC blocker)
                        const float y = h - dcX[(size_t) c] + dcR * dcY[(size_t) c]; dcX[(size_t) c] = h; dcY[(size_t) c] = y;
                        x += y;
                        work += std::abs (y);
                    }
                    io[c][i] = x * o;
                }
            setMeter (work / (float) (2 * n) * 40.0f + 0.1f * d);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** FILTER: a high-pass and a low-pass, Butterworth, 12 or 24 dB/oct. HIGH-PASS at 20 Hz and LOW-PASS
        at 20 kHz are off. */
    class Filter final : public RackUnit
    {
        Smooth hpF, lpF; std::array<Bq2, 2> hp {}, lp {}; float hpNow = -1, lpNow = -1; int slopeNow = -1;
        void prepareUnit (double, int) override { hpNow = lpNow = -1.0f; slopeNow = -1; }
        void resetUnit() override { for (auto& b : hp) b.reset(); for (auto& b : lp) b.reset(); hpF.set = lpF.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float fh = hpF.step (std::clamp (p[1], 20.0f, 1000.0f), n, sr), fl = lpF.step (std::clamp (p[2], 1000.0f, 20000.0f), n, sr);
            const int slope = choice (p[3], 2);
            static constexpr double q4[2] { 0.5411961, 1.3065630 };
            if (std::abs (fh - hpNow) > 0.05f || slope != slopeNow)
                for (int k = 0; k < 2; ++k) hp[(size_t) k].c = BiquadCoeffs::highPass (sr, fh, slope ? q4[k] : 0.7071);
            if (std::abs (fl - lpNow) > 0.5f || slope != slopeNow)
                for (int k = 0; k < 2; ++k) lp[(size_t) k].c = BiquadCoeffs::lowPass (sr, std::min ((double) fl, 0.45 * sr), slope ? q4[k] : 0.7071);
            hpNow = fh; lpNow = fl; slopeNow = slope;
            const bool hpOn = fh > 20.5f, lpOn = fl < 19900.0f;
            const int stages = slope ? 2 : 1;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float x = io[c][i];
                    if (hpOn) for (int k = 0; k < stages; ++k) x = hp[(size_t) k].run (c, x);
                    if (lpOn) for (int k = 0; k < stages; ++k) x = lp[(size_t) k].run (c, x);
                    io[c][i] = x;
                }
            setMeter ((hpOn ? 0.5f : 0.0f) + (lpOn ? 0.5f : 0.0f));
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** 550 EQ: three bands with proportional Q - the more a band is boosted or cut, the narrower it gets
        (as on the classic 500-series three-band): gentle, musical moves stay broad, big ones get precise.
        SHELF turns the LOW and HIGH bands into shelves. */
    class Eq550 final : public RackUnit
    {
        std::array<Smooth, 3> g {}; std::array<Bq2, 3> b {}; std::array<float, 3> gNow { 99, 99, 99 }; std::array<int, 3> fNow { -1, -1, -1 }; int shelfNow = -1;
        void prepareUnit (double, int) override { gNow = { 99, 99, 99 }; fNow = { -1, -1, -1 }; shelfNow = -1; }
        void resetUnit() override { for (auto& x : b) x.reset(); for (auto& x : g) x.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            static constexpr float lowHz[4] { 50, 100, 200, 400 }, midHz[5] { 400, 800, 1500, 3000, 5000 }, highHz[5] { 5000, 7500, 10000, 12500, 15000 };
            const int shelf = p[7] > 0.5f ? 1 : 0;
            const int f[3] { choice (p[2], 4), choice (p[4], 5), choice (p[6], 5) };
            float busy = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                const float gv = g[(size_t) k].step (std::clamp (p[1 + 2 * k], -12.0f, 12.0f), n, sr);
                busy += std::abs (gv) / 36.0f;
                if (std::abs (gv - gNow[(size_t) k]) < 0.005f && f[k] == fNow[(size_t) k] && shelf == shelfNow) continue;
                gNow[(size_t) k] = gv; fNow[(size_t) k] = f[k];
                const double q = 0.5 + 0.12 * std::abs (gv);   // proportional Q: 0.5 flat .. ~1.9 at 12 dB
                const double hz = k == 0 ? lowHz[f[k]] : k == 1 ? midHz[f[k]] : std::min (0.42 * sr, (double) highHz[f[k]]);
                b[(size_t) k].c = k == 0 && shelf ? BiquadCoeffs::lowShelf (sr, hz, 0.7071, gv)
                                : k == 2 && shelf ? BiquadCoeffs::highShelf (sr, hz, 0.7071, gv)
                                                  : BiquadCoeffs::peaking (sr, hz, q, gv);
            }
            shelfNow = shelf;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float x = io[c][i];
                    for (int k = 0; k < 3; ++k) if (gNow[(size_t) k] != 0.0f) x = b[(size_t) k].run (c, x);
                    io[c][i] = x;
                }
            setMeter (busy);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** TUBE EQ: the passive program equalizer of the 1950s. LOW BOOST and LOW ATTEN work at the same
        frequency (CPS) but are not opposites: the boost is a broad resonant shelf, the cut a shelf a little
        higher - both at once give the famous deep low end with a dip above it. HIGH BOOST is a bell at KCS,
        its width set by BANDWIDTH; HIGH ATTEN a shelf at ATTEN SEL. After the network, the make-up valve
        stage: a touch of second harmonic, driven against the signal's level. */
    class TubeEq final : public RackUnit
    {
        std::array<Smooth, 5> sm {}; std::array<Bq2, 4> b {}; std::array<float, 5> now { 99, 99, 99, 99, 99 }; std::array<int, 3> fNow { -1, -1, -1 };
        std::array<kit::SoftSat, 2> sat {}; std::array<kit::Level, 2> lvl {};
        void prepareUnit (double s, int) override { for (auto& l : lvl) l.setup (s, 0.3); now = { 99, 99, 99, 99, 99 }; fNow = { -1, -1, -1 }; }
        void resetUnit() override { for (auto& x : b) x.reset(); for (auto& x : sm) x.set = false; for (auto& x : sat) x.reset(); for (auto& l : lvl) l.reset(); }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            static constexpr float lowHz[4] { 20, 30, 60, 100 }, highK[7] { 3, 4, 5, 8, 10, 12, 16 }, attK[3] { 5, 10, 20 };
            const int lf = choice (p[1], 4), hf = choice (p[4], 7), af = choice (p[8], 3);
            const float boost = sm[0].step (std::clamp (p[2], 0.0f, 10.0f), n, sr), atten = sm[1].step (std::clamp (p[3], 0.0f, 10.0f), n, sr);
            const float hBoost = sm[2].step (std::clamp (p[5], 0.0f, 10.0f), n, sr), bw = sm[3].step (std::clamp (p[6], 0.0f, 10.0f), n, sr);
            const float hAtten = sm[4].step (std::clamp (p[7], 0.0f, 10.0f), n, sr);
            const float vals[5] { boost, atten, hBoost, bw, hAtten };
            bool changed = lf != fNow[0] || hf != fNow[1] || af != fNow[2];
            for (int k = 0; k < 5; ++k) changed = changed || std::abs (vals[k] - now[(size_t) k]) > 0.002f;
            if (changed)
            {
                for (int k = 0; k < 5; ++k) now[(size_t) k] = vals[k];
                fNow = { lf, hf, af };
                const double f0 = lowHz[lf];
                b[0].c = BiquadCoeffs::lowShelf (sr, f0 * 1.15, 0.95, 1.35 * boost);               // the boost: up to ~13.5 dB, a slight bump
                b[1].c = BiquadCoeffs::lowShelf (sr, f0 * 1.9, 0.55, -1.75 * atten);               // the cut: a little higher, gentler
                const double q = 3.0 - 2.6 * bw / 10.0;                                            // BANDWIDTH: sharp .. broad
                b[2].c = BiquadCoeffs::peaking (sr, std::min (0.42 * sr, highK[hf] * 1000.0), q, 1.8 * hBoost);
                b[3].c = BiquadCoeffs::highShelf (sr, std::min (0.42 * sr, attK[af] * 1000.0), 0.55, -2.0 * hAtten);
            }
            const bool on[4] { now[0] > 0.0f, now[1] > 0.0f, now[2] > 0.0f, now[4] > 0.0f };
            float work = 0.0f;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    float x = io[c][i];
                    for (int k = 0; k < 4; ++k) if (on[k]) x = b[(size_t) k].run (c, x);
                    // the valve: second harmonic about -50 dB at a normal level (a colour, not a distortion)
                    const float l = lvl[(size_t) c].process (x);
                    const float h = kit::harmonicsAt (sat[(size_t) c], x, l, 0.22f, 0.35f) * 0.6f;
                    x += h;
                    work += std::abs (h);
                    io[c][i] = x;
                }
            setMeter ((boost + atten + hBoost + hAtten) / 20.0f);
            (void) work;
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** TILT EQ: one knob that leans the whole spectrum about PIVOT - brighter and leaner one way, warmer and
        darker the other - with two broad first-order-like shelves in opposite directions (half the TILT
        each), so the level at the pivot never moves. */
    class Tilt final : public RackUnit
    {
        Smooth t; std::array<Bq2, 2> b {}; float tNow = 99; int pNow = -1;
        void prepareUnit (double, int) override { tNow = 99; pNow = -1; }
        void resetUnit() override { for (auto& x : b) x.reset(); t.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            static constexpr float pivot[4] { 300, 650, 1200, 2500 };
            const float tv = t.step (std::clamp (p[1], -6.0f, 6.0f), n, sr);
            const int pv = choice (p[2], 4);
            if (std::abs (tv - tNow) > 0.003f || pv != pNow)
            {
                tNow = tv; pNow = pv;
                b[0].c = BiquadCoeffs::lowShelf (sr, pivot[pv], 0.5, -0.5 * tv);
                b[1].c = BiquadCoeffs::highShelf (sr, pivot[pv], 0.5, 0.5 * tv);
            }
            if (tNow == 0.0f) { setMeter (0.0f); return; }
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                    io[c][i] = b[1].run (c, b[0].run (c, io[c][i]));
            setMeter (std::abs (tv) / 6.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** AIR BAND: the very top opened with a shelf so wide it has no edge - it starts gently an octave or two
        below FREQ, so it adds sheen and space, never hiss or edge. 20k and 40k: the part of a shelf that high
        that still reaches the ear (a little of it, a long way down). */
    class Air final : public RackUnit
    {
        Smooth a; Bq2 b; float aNow = 99; int fNow = -1;
        void prepareUnit (double, int) override { aNow = 99; fNow = -1; }
        void resetUnit() override { b.reset(); a.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            static constexpr float hz[5] { 2500, 5000, 10000, 20000, 40000 };
            const float av = a.step (std::clamp (p[1], 0.0f, 12.0f), n, sr);
            const int f = choice (p[2], 5);
            if (std::abs (av - aNow) > 0.003f || f != fNow)
            {
                aNow = av; fNow = f;
                const double fc = std::min ((double) hz[f], 0.40 * sr);
                const double scale = hz[f] <= fc ? 1.0 : std::pow (fc / hz[f], 0.6);   // (a shelf above what can be drawn: its audible tail)
                b.c = BiquadCoeffs::highShelf (sr, f >= 3 ? std::min (fc, 16000.0) : fc * 0.8, 0.38, av * scale);
            }
            if (aNow == 0.0f) { setMeter (0.0f); return; }
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                    io[c][i] = b.run (c, io[c][i]);
            setMeter (av / 12.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** LOUDNESS: our ears lose the bass (and a little of the top) as sound gets quieter - the equal-loudness
        contours of ISO 226. Mixes are made at around 83 dB; heard quieter they sound thin. LOUDNESS puts back
        what the ear loses at LISTEN dB (about 0.28 dB of bass and 0.08 dB of treble per dB below 83, up to
        15 dB), by AMOUNT. AUTO follows the programme instead: quiet passages get more, loud ones less (its
        level taken over ~3 s, -14 LUFS-ish read as 83 dB). */
    class Loudness final : public RackUnit
    {
        std::array<Bq2, 2> b {}; Smooth lvlSm; float lowNow = 99, highNow = 99; float ms = 0.0f;
        void prepareUnit (double, int) override { lowNow = highNow = 99; }
        void resetUnit() override { for (auto& x : b) x.reset(); lvlSm.set = false; ms = 0.0f; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            float level = std::clamp (p[1], 40.0f, 90.0f);
            if (p[3] > 0.5f)
            {
                double e = 0.0;
                for (int i = 0; i < n; ++i) e += 0.5 * ((double) io[0][i] * io[0][i] + (double) io[1][i] * io[1][i]);
                const float k = 1.0f - (float) std::exp (-(double) n / (3.0 * sr));
                ms += k * ((float) (e / n) - ms);
                const float dbfs = 10.0f * std::log10 (ms + 1.0e-12f);
                level = std::clamp (83.0f + (dbfs + 17.0f), 40.0f, 90.0f);   // (RMS of -17 dBFS ~ -14 LUFS)
            }
            const float l = lvlSm.step (level, n, sr) , amt = std::clamp (p[2], 0.0f, 100.0f) / 100.0f;
            const float below = 83.0f - l;
            const float lowDb = std::clamp (0.28f * below, -6.0f, 15.0f) * amt, highDb = std::clamp (0.08f * below, -3.0f, 5.0f) * amt;
            if (std::abs (lowDb - lowNow) > 0.01f || std::abs (highDb - highNow) > 0.01f)
            {
                lowNow = lowDb; highNow = highDb;
                b[0].c = BiquadCoeffs::lowShelf (sr, 110.0, 0.55, lowDb);
                b[1].c = BiquadCoeffs::highShelf (sr, 7500.0, 0.55, highDb);
            }
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                    io[c][i] = b[1].run (c, b[0].run (c, io[c][i]));
            setMeter (std::abs (lowDb) / 15.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** DE-ESSER: listens above FREQ; when that band crosses THRESH it is turned down - only that band
        (SPLIT: a dynamic high shelf, exactly as deep as the cut) or the whole sound (WIDE) - by up to
        RANGE, at about 3:1, 0.5 ms in and 60 ms out. LISTEN plays what it listens to. */
    class DeEsser final : public RackUnit
    {
        Smooth f; Bq2 side, shelf; float fNow = -1; float env = 0.0f, gr = 0.0f, shelfDb = 99.0f; int tick = 0;
        void prepareUnit (double, int) override { fNow = -1; shelfDb = 99.0f; }
        void resetUnit() override { side.reset(); shelf.reset(); f.set = false; env = gr = 0.0f; shelfDb = 99.0f; tick = 0; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float fv = f.step (std::clamp (p[1], 2000.0f, 12000.0f), n, sr);
            if (std::abs (fv - fNow) > 1.0f) { fNow = fv; side.c = BiquadCoeffs::highPass (sr, std::min (0.42 * sr, (double) fv), 0.7071); shelfDb = 99.0f; }
            const float thr = std::clamp (p[2], -50.0f, 0.0f), range = std::clamp (p[3], 0.0f, 16.0f);
            const bool wide = p[4] > 0.5f, listen = p[5] > 0.5f;
            const float att = coefFor (sr, 0.0005), rel = coefFor (sr, 0.060), gAtt = coefFor (sr, 0.0005), gRel = coefFor (sr, 0.060);
            float most = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float s0 = side.run (0, io[0][i]), s1 = side.run (1, io[1][i]);
                const float e = std::max (std::abs (s0), std::abs (s1));
                env += (e > env ? att : rel) * (e - env);
                const float over = gainToDb (env) - thr;
                const float want = over > 0.0f ? std::min (range, over * (2.0f / 3.0f)) : 0.0f;
                gr += (want > gr ? gAtt : gRel) * (want - gr);
                most = std::max (most, gr);
                if (listen) { io[0][i] = s0; io[1][i] = s1; continue; }
                if (wide) { const float g = dbToGainF (-gr); io[0][i] *= g; io[1][i] *= g; continue; }
                // SPLIT: a high shelf just under FREQ, as deep as the cut, redesigned every 16 samples
                if ((tick++ & 15) == 0 && std::abs (gr - shelfDb) > 0.02f)
                {
                    shelfDb = gr;
                    shelf.c = BiquadCoeffs::highShelf (sr, std::min (0.42 * sr, fNow * 0.85), 0.7071, -gr);
                }
                if (shelfDb > 0.001f) for (int c = 0; c < 2; ++c) io[c][i] = shelf.run (c, io[c][i]);
            }
            setMeter (range > 0.0f ? most / std::max (4.0f, range) : 0.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** TRANSIENT: the attack and the sustain of every hit, up or down, whatever its level - two envelopes of
        the same sound that differ only in how fast they rise (ATTACK) or fall (SUSTAIN); their ratio is the
        hit's shape, so a quiet hit is shaped as much as a loud one. Up to +-15 dB, linked. */
    class Transient final : public RackUnit
    {
        float fast = 0, slow = 0, susFast = 0, susSlow = 0, gSm = 0; Smooth out;
        void prepareUnit (double, int) override {}
        void resetUnit() override { fast = slow = susFast = susSlow = gSm = 0.0f; out.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float a = std::clamp (p[1], -100.0f, 100.0f) / 100.0f, su = std::clamp (p[2], -100.0f, 100.0f) / 100.0f;
            const float o = dbToGainF (out.step (std::clamp (p[3], -12.0f, 12.0f), n, sr));
            const float fA = coefFor (sr, 0.0005), sA = coefFor (sr, 0.012), rel = coefFor (sr, 0.040), rSlow = coefFor (sr, 0.300), gk = coefFor (sr, 0.0004);
            float most = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float e = std::max (std::abs (io[0][i]), std::abs (io[1][i])) + 1.0e-7f;
                fast += (e > fast ? fA : rel) * (e - fast);
                slow += (e > slow ? sA : rel) * (e - slow);
                susFast += (e > susFast ? fA : rel) * (e - susFast);
                susSlow += (e > susSlow ? fA : rSlow) * (e - susSlow);
                const float atkDb = std::clamp (gainToDb (fast / slow), 0.0f, 24.0f);
                const float susDb = std::clamp (gainToDb (susSlow / susFast), 0.0f, 24.0f);
                const float g = std::clamp (a * 0.9f * atkDb + su * 0.9f * susDb, -15.0f, 15.0f);
                gSm += gk * (g - gSm);
                most = std::max (most, std::abs (gSm));
                const float lin = dbToGainF (gSm) * o;
                io[0][i] *= lin; io[1][i] *= lin;
            }
            setMeter (most / 12.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** GATE: a clean downward expander. Below THRESH the sound is turned down by RATIO (1:2, 1:4, or a gate:
        straight to RANGE), never more than RANGE; opens in 0.3 ms, closes over RELEASE, with 3 dB of
        hysteresis on the gate and a 20 ms hold so a decaying note doesn't chatter. Linked. */
    class Gate final : public RackUnit
    {
        float env = 0, gr = 0; int hold = 0; bool open = false;
        void prepareUnit (double, int) override {}
        void resetUnit() override { env = gr = 0.0f; hold = 0; open = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float thr = std::clamp (p[1], -80.0f, -10.0f), range = std::clamp (p[2], 0.0f, 60.0f);
            const int ratio = choice (p[3], 3);
            const float rel = coefFor (sr, std::clamp (p[4], 10.0f, 1000.0f) * 0.001), att = coefFor (sr, 0.0003), eRel = coefFor (sr, 0.010);
            const int holdN = (int) (0.020 * sr);
            float most = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float e = std::max (std::abs (io[0][i]), std::abs (io[1][i]));
                env += (e > env ? 1.0f : eRel) * (e - env);
                const float db = gainToDb (env);
                float want;
                if (ratio == 2)
                {
                    if (db > thr) { open = true; hold = holdN; }
                    else if (db < thr - 3.0f && hold <= 0) open = false;
                    want = open ? 0.0f : range;
                }
                else
                {
                    want = db < thr ? std::min (range, (thr - db) * (ratio == 0 ? 1.0f : 3.0f)) : 0.0f;
                    // (the expander holds 20 ms after the level falls under THRESH before it starts to close)
                    if (db > thr) hold = holdN;
                    else if (hold > 0) { --hold; want = std::min (want, gr); }
                }
                if (ratio == 2 && hold > 0) --hold;
                gr += (want < gr ? att : rel) * (want - gr);
                most = std::max (most, gr);
                const float g = dbToGainF (-gr);
                io[0][i] *= g; io[1][i] *= g;
            }
            setMeter (range > 0.0f ? most / range : 0.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** BUS COMP: a clean VCA compressor for gluing a mix: feed-forward, both sides linked, a 6 dB soft
        knee, its detector high-passed at 60 Hz (the kick doesn't pump the rest). ATTACK and RELEASE in the
        classic steps; AUTO release: a fast release that lets go of short peaks and a slow one that only
        builds up under sustained compression. MAKE-UP after, then MIX (parallel compression). */
    class Comp final : public RackUnit
    {
        Bq2 scHp; float gr = 0.0f, slowGr = 0.0f; Smooth mk, mix; bool scSet = false;
        void prepareUnit (double, int) override { scSet = false; }
        void resetUnit() override { scHp.reset(); gr = slowGr = 0.0f; mk.set = mix.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            if (! scSet) { scSet = true; scHp.c = BiquadCoeffs::highPass (sr, 60.0, 0.7071); }
            static constexpr float ratios[4] { 1.5f, 2.0f, 4.0f, 10.0f }, atks[6] { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f }, rels[4] { 0.1f, 0.3f, 0.6f, 1.2f };
            const float thr = std::clamp (p[1], -30.0f, 0.0f), ratio = ratios[choice (p[2], 4)];
            const float att = coefFor (sr, atks[choice (p[3], 6)] * 0.001);
            const int ri = choice (p[4], 5);
            const bool autoRel = ri == 4;
            const float rel = coefFor (sr, autoRel ? 0.1 : rels[ri]), relSlow = coefFor (sr, 1.2), slowAtt = coefFor (sr, 0.4);
            const float makeup = dbToGainF (mk.step (std::clamp (p[5], 0.0f, 15.0f), n, sr)), wet = mix.step (std::clamp (p[6], 0.0f, 100.0f) / 100.0f, n, sr);
            const float knee = 6.0f, slope = 1.0f - 1.0f / ratio;
            float most = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float s0 = scHp.run (0, io[0][i]), s1 = scHp.run (1, io[1][i]);
                const float db = gainToDb (std::max (std::abs (s0), std::abs (s1)));
                const float over = db - thr;
                const float want = over <= -0.5f * knee ? 0.0f : over >= 0.5f * knee ? slope * over : slope * (over + 0.5f * knee) * (over + 0.5f * knee) / (2.0f * knee);
                if (autoRel)
                {
                    gr += (want > gr ? att : rel) * (want - gr);
                    const float sw = 0.6f * gr;
                    slowGr += (sw > slowGr ? slowAtt : relSlow) * (sw - slowGr);
                }
                else
                    gr += (want > gr ? att : rel) * (want - gr);
                const float total = std::max (gr, autoRel ? slowGr : 0.0f);
                most = std::max (most, total);
                const float g = dbToGainF (-total) * makeup;
                for (int c = 0; c < 2; ++c) { const float x = io[c][i]; io[c][i] = x + wet * (x * g - x); }
            }
            setMeter (most / 12.0f);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** SATURATOR: warmth, driven against the signal's own level so it colours the same at any volume, the
        loudness it adds taken back (only the colour stays). TAPE: soft and symmetric, a little top softened;
        TUBE: second harmonic first (a biased curve); CONSOLE: a faint mix of both, tighter. TONE tilts what
        comes out; MIX blends it with the dry sound. Anti-aliased (the curve's antiderivative). */
    class Saturator final : public RackUnit
    {
        std::array<kit::SoftSat, 2> sat {}; std::array<kit::Level, 2> lvl {}; std::array<Bq2, 2> tone {}; Bq2 tapeLp; Smooth drv, tn, mix;
        float toneNow = 99; bool lpSet = false; std::array<float, 2> dcX {}, dcY {}; float dcR = 0.999f;
        void prepareUnit (double s, int) override { for (auto& l : lvl) l.setup (s, 0.3); toneNow = 99; lpSet = false; dcR = (float) std::exp (-2.0 * 3.141592653589793 * 8.0 / s); }
        void resetUnit() override { for (auto& x : sat) x.reset(); for (auto& l : lvl) l.reset(); for (auto& t : tone) t.reset(); tapeLp.reset(); drv.set = tn.set = mix.set = false; dcX = {}; dcY = {}; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float d = drv.step (std::clamp (p[1], 0.0f, 10.0f), n, sr) / 10.0f;
            const int type = choice (p[2], 3);
            const float t = tn.step (std::clamp (p[3], -5.0f, 5.0f), n, sr), wet = mix.step (std::clamp (p[4], 0.0f, 100.0f) / 100.0f, n, sr);
            if (! lpSet) { lpSet = true; tapeLp.c = BiquadCoeffs::highShelf (sr, 9000.0, 0.6, -1.5); }
            if (std::abs (t - toneNow) > 0.005f)
            {
                toneNow = t;
                tone[0].c = BiquadCoeffs::lowShelf (sr, 900.0, 0.5, -0.6 * t);
                tone[1].c = BiquadCoeffs::highShelf (sr, 900.0, 0.5, 0.6 * t);
            }
            const float drive = 0.3f + 2.2f * d * d;
            const float bias = type == 1 ? 0.35f : type == 2 ? 0.12f : 0.0f;
            const float amount = type == 2 ? 0.7f : 1.0f;
            float work = 0.0f;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < 2; ++c)
                {
                    const float x = io[c][i];
                    const float l = lvl[(size_t) c].process (x);
                    float h = kit::harmonicsAt (sat[(size_t) c], x, l, drive, bias) * amount * std::min (1.0f, 4.0f * d);
                    const float hy = h - dcX[(size_t) c] + dcR * dcY[(size_t) c]; dcX[(size_t) c] = h; dcY[(size_t) c] = hy;
                    float y = x + hy;
                    if (type == 0) y = tapeLp.run (c, y);
                    if (toneNow != 0.0f) y = tone[1].run (c, tone[0].run (c, y));
                    work += std::abs (hy);
                    io[c][i] = x + wet * (y - x);
                }
            setMeter (work / (float) (2 * n) * 30.0f + 0.15f * d);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** M/S WIDTH: the sides turned up or down (0 - 200 %), the middle's level, and BASS MONO: below it the
        sides are high-passed out (12 dB/oct), so the low end stays centred and solid on any system. */
    class Width final : public RackUnit
    {
        Smooth w, mid; Bq2 hp; int monoNow = -1;
        void prepareUnit (double, int) override { monoNow = -1; }
        void resetUnit() override { hp.reset(); w.set = mid.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            static constexpr float hz[4] { 0, 80, 120, 200 };
            const float wv = w.step (std::clamp (p[1], 0.0f, 200.0f) / 100.0f, n, sr);
            const int mono = choice (p[2], 4);
            const float mg = dbToGainF (mid.step (std::clamp (p[3], -6.0f, 6.0f), n, sr));
            if (mono != monoNow) { monoNow = mono; if (mono > 0) hp.c = BiquadCoeffs::highPass (sr, hz[mono], 0.7071); }
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (io[0][i] + io[1][i]) * mg;
                float s = 0.5f * (io[0][i] - io[1][i]) * wv;
                if (monoNow > 0) s = hp.run (0, s);
                io[0][i] = m + s; io[1][i] = m - s;
            }
            setMeter (0.5f * wv);
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** PEAK LIMITER: holds the sample peaks at CEILING with no look-ahead (no latency): the gain drops at once
        to what the peak needs and recovers over RELEASE (faster after a lone peak, slower under a dense
        passage). DRIVE pushes into it. A soft 2 dB knee under the ceiling keeps light limiting transparent. */
    class Limiter final : public RackUnit
    {
        float g = 1.0f, busy = 0.0f; Smooth drv, ceil;
        void prepareUnit (double, int) override {}
        void resetUnit() override { g = 1.0f; busy = 0.0f; drv.set = ceil.set = false; }
        void render (float* const* io, int n, const float* p) noexcept override
        {
            const float c = dbToGainF (ceil.step (std::clamp (p[1], -12.0f, 0.0f), n, sr)), dr = dbToGainF (drv.step (std::clamp (p[2], 0.0f, 12.0f), n, sr));
            const float relS = std::clamp (p[3], 10.0f, 500.0f) * 0.001f;
            const float kneeLo = c * dbToGainF (-2.0f);
            const float busyK = coefFor (sr, 1.0);
            float most = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                const float x0 = io[0][i] * dr, x1 = io[1][i] * dr;
                const float pk = std::max (std::abs (x0), std::abs (x1));
                // what this sample needs: through the knee softly, never above the ceiling
                float need = 1.0f;
                if (pk > kneeLo)
                {
                    const float over = gainToDb (pk / kneeLo);                    // 0 .. : dB into the knee
                    const float outDb = over < 4.0f ? over - over * over / 16.0f : 2.0f;   // (a 2 dB soft knee to the ceiling)
                    need = dbToGainF (outDb - over);
                    need = std::min (need, c / pk);
                }
                busy += busyK * ((need < 0.999f ? 1.0f : 0.0f) - busy);
                const float rel = coefFor (sr, relS * (1.0f + 2.0f * busy));
                g = need < g ? need : g + rel * (need - g);
                most = std::max (most, -gainToDb (g));
                io[0][i] = x0 * g; io[1][i] = x1 * g;
            }
            setMeter (most / 12.0f);
        }
    };

    /** The modules, by their place in LbList.h (keep the order of gen_units.py's LB_MODULES). */
    inline std::unique_ptr<RackUnit> make (int k)
    {
        switch (k)
        {
            case 0:  return std::make_unique<Preamp>();
            case 1:  return std::make_unique<Filter>();
            case 2:  return std::make_unique<Eq550>();
            case 3:  return std::make_unique<TubeEq>();
            case 4:  return std::make_unique<Tilt>();
            case 5:  return std::make_unique<Air>();
            case 6:  return std::make_unique<Loudness>();
            case 7:  return std::make_unique<DeEsser>();
            case 8:  return std::make_unique<Transient>();
            case 9:  return std::make_unique<Gate>();
            case 10: return std::make_unique<Comp>();
            case 11: return std::make_unique<Saturator>();
            case 12: return std::make_unique<Width>();
            case 13: return std::make_unique<Limiter>();
            default: return nullptr;
        }
    }
    static_assert (lbmods::count == 14, "Lb500.h: one case per module in LbList.h");
}
