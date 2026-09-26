// LUNCHBOX unit tests: CLASS-A EQ (+ IRON), DE-HARSH, CROSSFEED. Included into EnhDspTests.cpp after
// check(); run with   EnhDspTests --lunchbox   (every check runs at 44.1, 48 and 96 kHz)
// Self-contained: needs only check (bool, message) and the Lunchbox header.

#include <chrono>
#include <complex>
#include <cstdio>
#include <cstring>
#include <functional>
#include <vector>
#include "DSP/Lunchbox.h"

namespace lunchtest
{
    using enh::dsp::Lunchbox;
    using Buf = std::vector<float>;
    constexpr double twoPi = 6.283185307179586;

    struct Rng
    {
        unsigned long long s = 88172645463325252ull;
        explicit Rng (unsigned long long seed) : s (seed * 2654435761ull + 1ull) {}
        float next() noexcept   // -1 .. 1
        {
            s ^= s << 13; s ^= s >> 7; s ^= s << 17;
            return (float) ((double) (s >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0);
        }
    };

    /** Pink noise (Paul Kellett's filter), about `rmsDb` dBFS RMS. */
    inline Buf pink (int n, unsigned long long seed, float rmsDb)
    {
        Rng r (seed);
        Buf x ((size_t) n);
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        double e = 0;
        for (int i = 0; i < n; ++i)
        {
            const float w = r.next();
            b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
            x[(size_t) i] = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            e += (double) x[(size_t) i] * x[(size_t) i];
        }
        const float g = (float) (std::pow (10.0, rmsDb / 20.0) / std::sqrt (e / n + 1e-30));
        for (auto& v : x) v *= g;
        return x;
    }

    inline double db (double x) { return 20.0 * std::log10 (std::max (1e-15, x)); }

    inline double rms (const Buf& x, int from, int to)
    {
        double s = 0;
        for (int i = from; i < to; ++i) s += (double) x[(size_t) i] * x[(size_t) i];
        return std::sqrt (s / std::max (1, to - from));
    }

    /** Runs the unit over a stereo signal in blocks; `during` may change the settings before each block,
        `after` sees the unit after each block. */
    inline void render (Lunchbox& lb, Buf& l, Buf& r, Lunchbox::Settings s, int block = 256,
                        std::function<void (int, Lunchbox::Settings&)> during = {},
                        std::function<void (int, const Lunchbox&)> after = {})
    {
        const int n = (int) l.size();
        for (int pos = 0; pos < n; pos += block)
        {
            const int m = std::min (block, n - pos);
            if (during) during (pos, s);
            float* chans[2] = { l.data() + pos, r.data() + pos };
            lb.process (chans, 2, m, s);
            if (after) after (pos, lb);
        }
    }

    /** Impulse response (left in -> left, right out; or both in), settings fixed from the start. */
    inline void impulse (double sr, const Lunchbox::Settings& s, Buf& l, Buf& r, bool both, int len = 1 << 16, float height = 1.0f)
    {
        Lunchbox lb;
        lb.prepare (sr, 512);
        l.assign ((size_t) len, 0.0f);
        r.assign ((size_t) len, 0.0f);
        l[0] = height;
        if (both) r[0] = height;
        render (lb, l, r, s, 512);
    }

    /** A response at hz (DFT of the impulse response at that frequency). */
    inline std::complex<double> respC (const Buf& h, double hz, double sr)
    {
        double re = 0, im = 0;
        for (size_t i = 0; i < h.size(); ++i)
        {
            re += h[i] * std::cos (twoPi * hz * (double) i / sr);
            im -= h[i] * std::sin (twoPi * hz * (double) i / sr);
        }
        return { re, im };
    }

    inline double respDb (const Buf& h, double hz, double sr) { return 10.0 * std::log10 (std::norm (respC (h, hz, sr)) + 1e-30); }

    /** A tone's amplitude in x[from, to) (Hann-windowed correlation). */
    inline double tone (const Buf& x, int from, int to, double hz, double sr)
    {
        double re = 0, im = 0, ws = 0;
        const int n = to - from;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (twoPi * (i + 0.5) / n);
            const double ph = twoPi * hz * (double) (from + i) / sr;
            re += w * x[(size_t) (from + i)] * std::cos (ph);
            im += w * x[(size_t) (from + i)] * std::sin (ph);
            ws += w;
        }
        return 2.0 * std::sqrt (re * re + im * im) / ws;
    }

    /** A band's level (dB) around hz: a narrow band-pass (Q 4) and RMS. */
    inline double bandDb (const Buf& x, int from, int to, double hz, double sr)
    {
        const auto c = enh::dsp::BiquadCoeffs::bandPass (sr, hz, 4.0);
        enh::dsp::BiquadState st;
        double e = 0;
        for (int i = 0; i < to; ++i)
        {
            const float y = st.process (c, x[(size_t) i]);
            if (i >= from) e += (double) y * y;
        }
        return 10.0 * std::log10 (e / std::max (1, to - from) + 1e-30);
    }

    /** RMS (dB) of x[from, to) through a 20 Hz .. 20 kHz band limit (4th order each side). */
    inline double audibleDb (const Buf& x, int from, int to, double sr)
    {
        using enh::dsp::BiquadCoeffs;
        const BiquadCoeffs hp1 = BiquadCoeffs::highPass (sr, 20.0, 0.5412), hp2 = BiquadCoeffs::highPass (sr, 20.0, 1.3066);
        const BiquadCoeffs lp1 = BiquadCoeffs::lowPass (sr, std::min (20000.0, 0.45 * sr), 0.5412), lp2 = BiquadCoeffs::lowPass (sr, std::min (20000.0, 0.45 * sr), 1.3066);
        enh::dsp::BiquadState a, b, c, d;
        double e = 0;
        for (int i = 0; i < to; ++i)
        {
            const float y = d.process (lp2, c.process (lp1, b.process (hp2, a.process (hp1, x[(size_t) i]))));
            if (i >= from) e += (double) y * y;
        }
        return 10.0 * std::log10 (e / std::max (1, to - from) + 1e-30);
    }

    inline Buf sine (int n, double hz, double sr, float amp, double phase = 0.0)
    {
        Buf x ((size_t) n);
        for (int i = 0; i < n; ++i)
            x[(size_t) i] = amp * (float) std::sin (twoPi * hz * i / sr + phase);
        return x;
    }

    /** A sine through the unit (stereo, same on both sides): its harmonics 2 .. 9 and THD (%). */
    struct Thd { double percent = 0, fundamental = 0, h2 = 0, h3 = 0; };
    inline Thd thd (double sr, const Lunchbox::Settings& s, double hz, float levelDb)
    {
        const int n = (int) (sr * 1.5), from = (int) (sr * 0.5);
        Buf l = sine (n, hz, sr, (float) std::pow (10.0, levelDb / 20.0)), r = l;
        Lunchbox lb;
        lb.prepare (sr, 256);
        render (lb, l, r, s);
        Thd t;
        t.fundamental = tone (l, from, n, hz, sr);
        double h = 0;
        for (int k = 2; k <= 9 && k * hz < sr * 0.45; ++k)
        {
            const double a = tone (l, from, n, k * hz, sr);
            h += a * a;
            if (k == 2) t.h2 = a;
            if (k == 3) t.h3 = a;
        }
        t.percent = 100.0 * std::sqrt (h) / std::max (1e-15, t.fundamental);
        return t;
    }

    inline bool allFinite (const Buf& x)
    {
        for (float v : x) if (! std::isfinite (v)) return false;
        return true;
    }

    /** Largest sample-to-sample step, and largest second difference, in x[from, to). */
    struct Slope { double first = 0, second = 0; };
    inline Slope slope (const Buf& x, int from, int to)
    {
        Slope s;
        for (int i = std::max (2, from); i < to; ++i)
        {
            s.first = std::max (s.first, (double) std::abs (x[(size_t) i] - x[(size_t) i - 1]));
            s.second = std::max (s.second, (double) std::abs (x[(size_t) i] - 2.0f * x[(size_t) i - 1] + x[(size_t) i - 2]));
        }
        return s;
    }

    /** bs2b's own direct (D) and cross (X) filters, written out independently of the unit, at hz. */
    inline void bs2b (double sr, double fcLo, double level, double hz, std::complex<double>& d, std::complex<double>& x)
    {
        const double gbLo = level * -5.0 / 6.0 - 3.0, gbHi = level / 6.0 - 3.0;
        const double gLo = std::pow (10.0, gbLo / 20.0), gHi = 1.0 - std::pow (10.0, gbHi / 20.0);
        const double fcHi = fcLo * std::pow (2.0, (gbLo - 20.0 * std::log10 (gHi)) / 12.0);
        const double xl = std::exp (-twoPi * fcLo / sr), xh = std::exp (-twoPi * fcHi / sr);
        const std::complex<double> z1 = std::polar (1.0, -twoPi * hz / sr);
        x = gLo * (1.0 - xl) / (1.0 - xl * z1);
        d = (1.0 - gHi * (1.0 - xh) - xh * z1) / (1.0 - xh * z1);
    }
}

static void runLunchboxTestsAt (double sr)
{
    namespace lt = lunchtest;
    using lt::Lunchbox;
    using lt::Buf;
    char msg[640];
    std::printf ("\n== LUNCHBOX at %.1f kHz ==\n", sr / 1000.0);

    //==========================================================================
    // Bypass
    {
        const int n = (int) sr * 2;
        const Buf inL = lt::pink (n, 1, -18.0f), inR = lt::pink (n, 2, -18.0f);
        Buf l = inL, r = inR;
        Lunchbox lb;
        lb.prepare (sr, 256);
        Lunchbox::Settings s;   // everything OUT (but knobs away from flat: they must not matter)
        s.lowGainDb = 9.0f; s.midGainDb = -7.0f; s.highGainDb = 5.0f; s.hpf = 3; s.iron = true;
        lt::render (lb, l, r, s);
        const bool same = std::memcmp (l.data(), inL.data(), sizeof (float) * (size_t) n) == 0
                       && std::memcmp (r.data(), inR.data(), sizeof (float) * (size_t) n) == 0;
        check (same, "all sections OUT: bit-identical passthrough");

        // Everything IN for a second, then OUT: once the fades are over it is bit-identical again
        Buf l2 = inL, r2 = inR;
        Lunchbox lb2;
        lb2.prepare (sr, 256);
        lt::render (lb2, l2, r2, s, 256, [&] (int pos, Lunchbox::Settings& q)
        {
            const bool on = pos < n / 2;
            q.eqIn = q.harshIn = q.feedIn = on;
        });
        const int from = n / 2 + (int) (sr * 0.1);
        const bool sameAfter = std::memcmp (l2.data() + from, inL.data() + from, sizeof (float) * (size_t) (n - from)) == 0
                            && std::memcmp (r2.data() + from, inR.data() + from, sizeof (float) * (size_t) (n - from)) == 0;
        check (sameAfter, "switched IN then OUT: bit-identical again once the 20 ms fade is over");
        check (lb2.getLatencySamples() == 0, "reports zero latency");
    }

    //==========================================================================
    // CLASS-A EQ
    {
        const int n = (int) sr * 2;
        const Buf inL = lt::pink (n, 3, -12.0f), inR = lt::pink (n, 4, -12.0f);
        Buf l = inL, r = inR;
        Lunchbox lb;
        lb.prepare (sr, 256);
        Lunchbox::Settings s;
        s.eqIn = true;
        lt::render (lb, l, r, s);
        double worst = 0;
        for (int i = 0; i < n; ++i)
            worst = std::max ({ worst, (double) std::abs (l[(size_t) i] - inL[(size_t) i]), (double) std::abs (r[(size_t) i] - inR[(size_t) i]) });
        std::snprintf (msg, sizeof msg, "EQ IN at flat (IRON off): transparent (largest difference %.1f dB, < -120)", lt::db (worst));
        check (worst == 0.0 || lt::db (worst) < -120.0, msg);
    }

    // LOW: the inductor shelf's shape on every step
    {
        Buf l, r;
        for (int step = 0; step < 4; ++step)
        {
            const double f = Lunchbox::lowHzSteps[(size_t) step];
            Lunchbox::Settings s;
            s.eqIn = true; s.lowFreq = step; s.lowGainDb = 16.0f;
            lt::impulse (sr, s, l, r, false);
            const double plateau = lt::respDb (l, f / 4.0, sr);
            double peak = -100, peakHz = 0, dip = 100, dipHz = 0, model = 0;
            for (double hz = 0.3 * f; hz < 1.5 * f; hz *= 1.01)
                if (const double v = lt::respDb (l, hz, sr); v > peak) { peak = v; peakHz = hz; }
            for (double hz = 1.5 * f; hz < 10.0 * f; hz *= 1.01)
                if (const double v = lt::respDb (l, hz, sr); v < dip) { dip = v; dipHz = hz; }
            for (double hz = std::max (10.0, f / 4.0); hz < 3000.0; hz *= 1.05)
                model = std::max (model, std::abs (lt::respDb (l, hz, sr) - Lunchbox::lowShelfAnalogDb (hz, f, 16.0)));
            std::snprintf (msg, sizeof msg, "LOW +16 at %.0f Hz: plateau %+.2f dB, overshoot %+.2f dB at %.2f x, dip %+.2f dB at %.2f x (target +2.0 at ~0.77 x, -1.4 at ~3.5 x); off the analogue curve by %.3f dB at most",
                           f, plateau, peak - 16.0, peakHz / f, dip, dipHz / f, model);
            check (std::abs (plateau - 16.0) < 0.5 && peak - 16.0 > 1.5 && peak - 16.0 < 2.5 && peakHz / f > 0.6 && peakHz / f < 0.95
                   && dip < -1.0 && dip > -1.9 && dipHz / f > 2.5 && dipHz / f < 4.5 && model < 0.1, msg);

            if (step == 1)
            {
                // Cut mirrors boost; the resonance grows with the knob
                Buf cl, cr, hl, hr;
                s.lowGainDb = -16.0f;
                lt::impulse (sr, s, cl, cr, false);
                double mirror = 0;
                for (double hz = 10.0; hz < 2000.0; hz *= 1.1)
                    mirror = std::max (mirror, std::abs (lt::respDb (cl, hz, sr) + lt::respDb (l, hz, sr)));
                const double at2k = lt::respDb (cl, 2000.0, sr);
                s.lowGainDb = 8.0f;
                lt::impulse (sr, s, hl, hr, false);
                double peak8 = -100;
                for (double hz = 0.3 * f; hz < 1.5 * f; hz *= 1.01) peak8 = std::max (peak8, lt::respDb (hl, hz, sr));
                std::snprintf (msg, sizeof msg, "LOW -16 at 60 Hz mirrors +16 (within %.3f dB), %.2f dB at 2 kHz; at +8 dB the overshoot is only %+.2f dB",
                               mirror, at2k, peak8 - 8.0);
                check (mirror < 0.05 && std::abs (at2k) < 0.3 && peak8 - 8.0 > 0.2 && peak8 - 8.0 < 0.8, msg);
            }
        }
    }

    // MID: gain, proportional Q, HI Q, reciprocal cut, analogue shape up to Nyquist
    {
        Buf l, r;
        auto bell = [&] (int freq, bool hq, float g, Buf& out)
        {
            Lunchbox::Settings s;
            s.eqIn = true; s.midFreq = freq; s.midHighQ = hq; s.midGainDb = g;
            Buf rr;
            lt::impulse (sr, s, out, rr, false, 1 << 15);
        };
        bell (2, false, 18.0f, l);
        const double mid = lt::respDb (l, 1600.0, sr);
        bell (5, false, -18.0f, r);
        const double midCut = lt::respDb (r, 7200.0, sr);
        std::snprintf (msg, sizeof msg, "MID +18 dB at 1.6 kHz: %.2f dB; -18 dB at 7.2 kHz: %.2f dB", mid, midCut);
        check (std::abs (mid - 18.0) < 0.2 && std::abs (midCut + 18.0) < 0.2, msg);

        // Width 3 dB under the peak, in octaves (proportional Q: narrower the more it boosts)
        auto widthUnderPeak = [&] (const Buf& h, double centre, double under)
        {
            const double top = lt::respDb (h, centre, sr);
            double lo = 0, hi = 0;
            for (double f = centre; f > 20.0; f /= 1.005) if (lt::respDb (h, f, sr) < top - under) { lo = f; break; }
            for (double f = centre; f < 0.49 * sr; f *= 1.005) if (lt::respDb (h, f, sr) < top - under) { hi = f; break; }
            return lo > 0 && hi > 0 ? std::log2 (hi / lo) : 99.0;
        };
        Buf b6, b18, b12, c12, h12;
        bell (2, false, 6.0f, b6);
        bell (2, false, 12.0f, b12);
        bell (2, false, -12.0f, c12);
        bell (2, true, 12.0f, h12);
        const double w6 = widthUnderPeak (b6, 1600.0, 3.0), w18 = widthUnderPeak (l, 1600.0, 3.0);
        const double half = widthUnderPeak (b12, 1600.0, 6.0), halfHq = widthUnderPeak (h12, 1600.0, 6.0);
        std::snprintf (msg, sizeof msg, "MID proportional Q: 3 dB under the peak it is %.2f oct wide at +6 dB, %.2f oct at +18 dB (Q %.2f)", w6, w18,
                       std::sqrt (std::exp2 (w18)) / (std::exp2 (w18) - 1.0));
        check (w18 < 0.8 * w6 && w18 > 0.8 && w18 < 1.3, msg);
        std::snprintf (msg, sizeof msg, "HI Q narrows the bell: %.2f octaves wide at half gain (+12 dB), normal %.2f", halfHq, half);
        check (halfHq < 0.6 * half, msg);
        double recip = 0;
        for (double f = 50.0; f < 0.45 * sr; f *= 1.1)
            recip = std::max (recip, std::abs (lt::respDb (b12, f, sr) + lt::respDb (c12, f, sr)));
        std::snprintf (msg, sizeof msg, "MID cut is the boost's mirror image (the hardware's reciprocal curves): within %.3f dB", recip);
        check (recip < 0.05, msg);

        // No cramping: the digital bell against the analogue one, up to 20 kHz
        double worstLow = 0, worstHigh = 0;
        for (int freq : { 0, 2, 3, 5 })
            for (float g : { 18.0f, -18.0f, 8.0f })
            {
                Buf h;
                bell (freq, freq == 3, g, h);
                const double c = Lunchbox::midHzSteps[(size_t) freq];
                for (double f = 30.0; f < std::min (20000.0, 0.46 * sr); f *= 1.03)
                {
                    const double e = std::abs (lt::respDb (h, f, sr) - Lunchbox::midBellAnalogDb (f, c, freq == 3, g));
                    (c <= 1600.0 ? worstLow : worstHigh) = std::max (c <= 1600.0 ? worstLow : worstHigh, e);
                }
            }
        Buf naive;
        bell (5, false, 18.0f, naive);
        std::snprintf (msg, sizeof msg, "MID follows the analogue bell to 20 kHz: within %.2f dB (360 / 1600 Hz), %.2f dB (3.2 / 7.2 kHz, +-18); 7.2 kHz +18 at 16 kHz: %.2f dB (analogue %.2f)",
                       worstLow, worstHigh, lt::respDb (naive, 16000.0, sr), Lunchbox::midBellAnalogDb (16000.0, 7200.0, false, 18.0));
        check (worstLow < 0.3 && worstHigh < 1.0, msg);
    }

    // HIGH: first-order 12 kHz shelf, analogue shape up to 20 kHz
    {
        Buf l, r;
        Lunchbox::Settings s;
        s.eqIn = true;
        double worst = 0, steepest = 0;
        for (float g : { 16.0f, 8.0f, -16.0f })
        {
            s.highGainDb = g;
            lt::impulse (sr, s, l, r, false, 1 << 14);
            for (double f = 500.0; f < std::min (20000.0, 0.46 * sr); f *= 1.03)
                worst = std::max (worst, std::abs (lt::respDb (l, f, sr) - Lunchbox::highShelfAnalogDb (f, g)));
            for (double f = 500.0; f < std::min (20000.0, 0.46 * sr) / 1.2; f *= 1.2)
                steepest = std::max (steepest, std::abs (lt::respDb (l, f * 1.2, sr) - lt::respDb (l, f, sr)) / std::log2 (1.2));
        }
        s.highGainDb = 16.0f;
        lt::impulse (sr, s, l, r, false, 1 << 14);
        std::snprintf (msg, sizeof msg, "HIGH +16: %+.2f dB at 500 Hz, %+.2f at 5 kHz, %+.2f at 12 kHz, %+.2f at 20 kHz; off the analogue first-order shelf by %.3f dB at most (to 20 kHz), steepest %.1f dB/oct",
                       lt::respDb (l, 500.0, sr), lt::respDb (l, 5000.0, sr), lt::respDb (l, 12000.0, sr), lt::respDb (l, std::min (20000.0, 0.46 * sr), sr), worst, steepest);
        check (std::abs (lt::respDb (l, 12000.0, sr) - 8.0) < 0.3 && std::abs (lt::respDb (l, 500.0, sr)) < 0.2 && worst < 0.2 && steepest < 6.2, msg);
    }

    // HPF: 18 dB/oct
    {
        Buf l, r;
        Lunchbox::Settings s;
        s.eqIn = true;
        s.hpf = 2;
        lt::impulse (sr, s, l, r, false);
        const double at80 = lt::respDb (l, 80.0, sr), below = lt::respDb (l, 80.0 / std::pow (2.0, 1.5), sr);
        const double oct = lt::respDb (l, 20.0, sr) - lt::respDb (l, 10.0, sr);
        double ripple = -100.0;
        for (double f = 80.0; f < 2000.0; f *= 1.05) ripple = std::max (ripple, lt::respDb (l, f, sr));
        std::snprintf (msg, sizeof msg, "HPF 80 Hz: %.2f dB at 80 Hz, %.1f dB at 28 Hz (1.5 octaves below), %.1f dB/oct far below, passband peak %+.2f dB",
                       at80, below, oct, ripple);
        check (std::abs (at80 + 3.0) < 0.5 && below <= -30.0 && std::abs (oct - 18.0) < 0.5 && ripple < 0.6, msg);
        s.hpf = 4;
        lt::impulse (sr, s, l, r, false);
        const double at300 = lt::respDb (l, 300.0, sr);
        std::snprintf (msg, sizeof msg, "HPF 300 Hz: %.2f dB at 300 Hz", at300);
        check (std::abs (at300 + 3.0) < 0.5, msg);
    }

    //==========================================================================
    // IRON
    {
        Lunchbox::Settings s;
        s.eqIn = true; s.iron = true;
        const double hzs[4] = { 30.0, 100.0, 1000.0, 5000.0 };
        const float lvls[3] = { -18.0f, -6.0f, 0.0f };
        lt::Thd t[4][3];
        std::printf ("  IRON harmonics (THD %%, 2nd / 3rd in dB under the fundamental):\n");
        for (int i = 0; i < 4; ++i)
        {
            std::printf ("    %6.0f Hz:", hzs[i]);
            for (int j = 0; j < 3; ++j)
            {
                t[i][j] = lt::thd (sr, s, hzs[i], lvls[j]);
                std::printf ("   %+4.0f dBFS %6.3f %% (%5.1f / %5.1f)", lvls[j], t[i][j].percent,
                             lt::db (t[i][j].h2 / t[i][j].fundamental), lt::db (t[i][j].h3 / t[i][j].fundamental));
            }
            std::printf ("\n");
        }
        const auto& q = t[1][0]; const auto& m = t[1][1]; const auto& loud = t[1][2];
        std::snprintf (msg, sizeof msg, "IRON THD at 100 Hz: %.3f %% at -18 dBFS, %.3f %% at -6, %.2f %% at 0 dBFS (rises with level)", q.percent, m.percent, loud.percent);
        check (q.percent < 0.1 && q.percent < m.percent && m.percent < loud.percent && loud.percent > 0.3 && loud.percent < 2.0, msg);
        std::snprintf (msg, sizeof msg, "IRON is a core effect: THD at 0 dBFS %.2f %% at 30 Hz, %.2f %% at 100 Hz, %.3f %% at 1 kHz, %.3f %% at 5 kHz",
                       t[0][2].percent, loud.percent, t[2][2].percent, t[3][2].percent);
        check (t[0][2].percent > 3.0 * loud.percent && t[3][2].percent < 0.25 * loud.percent && t[3][0].percent < q.percent, msg);
        std::snprintf (msg, sizeof msg, "IRON harmonic structure: 3rd over 2nd at 30 / 100 Hz (%.1f / %.1f dB), 2nd over 3rd at 1 / 5 kHz (%.1f / %.1f dB) - the core, then the class-A stage",
                       lt::db (t[0][2].h3 / t[0][2].h2), lt::db (loud.h3 / loud.h2), lt::db (t[2][2].h2 / t[2][2].h3), lt::db (t[3][2].h2 / t[3][2].h3));
        check (t[0][2].h3 > t[0][2].h2 && loud.h3 > loud.h2 && t[2][2].h2 > t[2][2].h3 && t[3][2].h2 > t[3][2].h3, msg);
        const double amp18 = std::pow (10.0, -18.0 / 20.0);
        std::snprintf (msg, sizeof msg, "IRON is level-neutral: 100 Hz %+.3f dB at -18 dBFS, %+.3f at 0 dBFS; 1 kHz %+.3f / %+.3f dB",
                       lt::db (q.fundamental / amp18), lt::db (loud.fundamental), lt::db (t[2][0].fundamental / amp18), lt::db (t[2][2].fundamental));
        check (std::abs (lt::db (q.fundamental / amp18)) < 0.3 && std::abs (lt::db (loud.fundamental)) < 0.3
               && std::abs (lt::db (t[2][0].fundamental / amp18)) < 0.1 && std::abs (lt::db (t[2][2].fundamental)) < 0.1, msg);

        // Small-signal response: the transformers' gentle ends
        Buf l, r;
        lt::impulse (sr, s, l, r, false, 1 << 17, 1.0e-3f);
        auto at = [&] (double hz) { return lt::respDb (l, hz, sr) + 60.0; };
        const double top = std::min (20000.0, 0.45 * sr);
        std::snprintf (msg, sizeof msg, "IRON small-signal response: %+.2f dB at 5 Hz, %+.2f at 10 Hz, %+.2f at 20 Hz, %+.3f at 1 kHz, %+.2f at 10 kHz, %+.2f at %.0f Hz",
                       at (5.0), at (10.0), at (20.0), at (1000.0), at (10000.0), at (top), top);
        check (at (5.0) < -3.0 && std::abs (at (10.0)) < 0.5 && at (20.0) > 0.05 && at (20.0) < 0.4 && std::abs (at (1000.0)) < 0.02
               && at (10000.0) >= 0.0 && at (10000.0) < 0.1 && at (top) > 0.05 && at (top) < 0.35, msg);

        // On noise, the audible level with IRON against without
        const int n = (int) sr * 2;
        Buf nl = lt::pink (n, 9, -18.0f), nr = lt::pink (n, 10, -18.0f);
        const double inDb = lt::audibleDb (nl, n / 4, n, sr);
        Lunchbox lb;
        lb.prepare (sr, 256);
        lt::render (lb, nl, nr, s);
        const double change = lt::audibleDb (nl, n / 4, n, sr) - inDb;
        std::snprintf (msg, sizeof msg, "IRON on pink noise at -18 dBFS: level %+.3f dB (20 Hz - 20 kHz)", change);
        check (std::abs (change) < 0.2, msg);

        // Aliasing: a loud high tone. Its harmonics above Nyquist must not fold back (the class-A stage
        // runs at twice the rate); compared with the real harmonics of a 1 kHz tone
        {
            const double f = 0.3 * sr;
            const int len = (int) sr, from = len / 4;
            Buf al = lt::sine (len, f, sr, 1.0f), ar = al;
            Lunchbox u;
            u.prepare (sr, 256);
            lt::render (u, al, ar, s);
            const double a2 = lt::db (lt::tone (al, from, len, sr - 2.0 * f, sr)), a3 = lt::db (lt::tone (al, from, len, 3.0 * f - sr, sr));
            const double real2 = lt::db (t[2][2].h2);
            std::snprintf (msg, sizeof msg, "IRON aliasing, a 0 dBFS tone at %.1f kHz: folded 2nd %.1f dBFS, folded 3rd %.1f dBFS (a 1 kHz tone's real 2nd: %.1f dBFS)",
                           f / 1000.0, a2, a3, real2);
            check (a2 < real2 - 40.0 && a3 < real2 - 40.0 && a2 < -110.0 && a3 < -110.0, msg);
        }
    }

    //==========================================================================
    // DE-HARSH
    {
        const int n = (int) sr * 4, from = (int) sr * 1;
        auto run = [&] (const Buf& inL, const Buf& inR, float amount, int freq, Buf& outL, double& meanCut, double& maxCut)
        {
            Buf l = inL, r = inR;
            Lunchbox lb;
            lb.prepare (sr, 256);
            Lunchbox::Settings s;
            s.harshIn = true; s.harshAmount = amount; s.harshFreq = freq;
            double sum = 0; int count = 0; maxCut = 0;
            lt::render (lb, l, r, s, 256, {}, [&] (int pos, const Lunchbox& u)
            {
                if (pos >= from) { sum += u.getHarshReductionDb(); ++count; maxCut = std::max (maxCut, (double) u.getHarshReductionDb()); }
            });
            meanCut = sum / std::max (1, count);
            outL = l;
        };

        const Buf pL = lt::pink (n, 11, -20.0f), pR = lt::pink (n, 12, -20.0f);
        const int freqs[3] = { 0, 1, 2 };
        const double hz[3] = { 2500.0, 4000.0, 6500.0 };
        for (int k : freqs)
        {
            Buf out;
            double mean = 0, most = 0;
            run (pL, pR, 10.0f, k, out, mean, most);
            const double band = lt::bandDb (out, from, n, hz[k], sr) - lt::bandDb (pL, from, n, hz[k], sr);
            std::snprintf (msg, sizeof msg, "balanced pink noise, %.1f kHz, AMOUNT 10: cut %.2f dB on average (most %.2f), band level %+.2f dB",
                           hz[k] / 1000.0, mean, most, band);
            check (mean < 1.0 && band > -1.0, msg);
        }

        // Idle it is a wire: material with nothing in the band passes bit for bit
        {
            Buf qL = pL, qR = pR;
            const auto lp = enh::dsp::BiquadCoeffs::lowPass (sr, 400.0, 0.7071);
            enh::dsp::BiquadState a, b, c, d;
            for (int i = 0; i < n; ++i) { qL[(size_t) i] = b.process (lp, a.process (lp, qL[(size_t) i])); qR[(size_t) i] = d.process (lp, c.process (lp, qR[(size_t) i])); }
            Buf out, outR = qR;
            double mean = 0, most = 0;
            run (qL, qR, 10.0f, 1, out, mean, most);
            const bool same = std::memcmp (out.data() + from, qL.data() + from, sizeof (float) * (size_t) (n - from)) == 0;
            std::snprintf (msg, sizeof msg, "DE-HARSH IN but not cutting (dark material, cut %.2f dB): output bit-identical to the input: %s", most, same ? "yes" : "no");
            check (same && most == 0.0, msg);
        }

        // The same noise with a +12 dB peak at the watched band
        for (int k : freqs)
        {
            Buf hL = pL, hR = pR;
            const auto peak = enh::dsp::BiquadCoeffs::peaking (sr, hz[k], 2.0, 12.0);
            enh::dsp::BiquadState a, b;
            for (int i = 0; i < n; ++i) { hL[(size_t) i] = a.process (peak, hL[(size_t) i]); hR[(size_t) i] = b.process (peak, hR[(size_t) i]); }
            Buf out;
            double mean = 0, most = 0;
            run (hL, hR, 10.0f, k, out, mean, most);
            const double band = lt::bandDb (out, from, n, hz[k], sr) - lt::bandDb (hL, from, n, hz[k], sr);
            // "Untouched": two octaves under and over the band, and 200 Hz
            const double below = hz[k] / 4.0, above = std::min (hz[k] * 4.0, 0.4 * sr);
            const double atBelow = lt::bandDb (out, from, n, below, sr) - lt::bandDb (hL, from, n, below, sr);
            const double atAbove = lt::bandDb (out, from, n, above, sr) - lt::bandDb (hL, from, n, above, sr);
            const double at200 = lt::bandDb (out, from, n, 200.0, sr) - lt::bandDb (hL, from, n, 200.0, sr);
            const double broad = lt::db (lt::rms (out, from, n)) - lt::db (lt::rms (hL, from, n));
            std::snprintf (msg, sizeof msg, "+12 dB peak at %.1f kHz, AMOUNT 10: meter %.2f dB, band %+.2f dB; %.0f Hz %+.2f, %.0f Hz %+.2f, 200 Hz %+.2f; broadband %+.2f dB",
                           hz[k] / 1000.0, mean, band, below, atBelow, above, atAbove, at200, broad);
            check (mean >= 3.0 && band <= -3.0 && std::abs (atBelow) < 0.5 && std::abs (atAbove) < 0.5 && std::abs (at200) < 0.1 && std::abs (mean + band) < 3.0, msg);

            if (k == 1)
            {
                double mean5 = 0, most5 = 0, mean0 = 0, most0 = 0;
                Buf o5, o0;
                run (hL, hR, 5.0f, k, o5, mean5, most5);
                run (hL, hR, 0.0f, k, o0, mean0, most0);
                std::snprintf (msg, sizeof msg, "AMOUNT sets the depth: %.2f dB at 10, %.2f at 5 (at most 4.5), %.2f at 0", mean, mean5, mean0);
                check (mean5 < mean && most5 <= 4.51 && most0 == 0.0, msg);

                // Loud but balanced: the same pink noise 20 dB louder is still left alone
                Buf lL = pL, lR = pR;
                for (auto& v : lL) v *= 10.0f;
                for (auto& v : lR) v *= 10.0f;
                double meanLoud = 0, mostLoud = 0;
                Buf ol;
                run (lL, lR, 10.0f, k, ol, meanLoud, mostLoud);
                std::snprintf (msg, sizeof msg, "loud but balanced (pink, +20 dB): cut %.2f dB (level does not matter, balance does)", meanLoud);
                check (meanLoud < 1.0, msg);
            }
        }

        // Bursts: attack within a few ms; a short one lets go at SPEED, a long one more slowly
        {
            auto burst = [&] (double seconds, double& at5ms, double& at100, double& at150, double& at250, double& peakCut)
            {
                const int m = (int) sr * 3;
                Buf l = lt::pink (m, 13, -20.0f), r = lt::pink (m, 14, -20.0f);
                const Buf tn = lt::sine (m, 4000.0, sr, 0.3f);
                const int on = (int) sr, off = on + (int) (sr * seconds);
                for (int i = on; i < off; ++i) { l[(size_t) i] += tn[(size_t) i]; r[(size_t) i] += tn[(size_t) i]; }
                Lunchbox lb;
                lb.prepare (sr, 32);
                Lunchbox::Settings s;
                s.harshIn = true; s.harshAmount = 10.0f; s.harshFreq = 1; s.harshSpeedMs = 30.0f;
                at5ms = at100 = at150 = at250 = -1; peakCut = 0;
                lt::render (lb, l, r, s, 32, {}, [&] (int pos, const Lunchbox& u)
                {
                    const double c = u.getHarshReductionDb();
                    if (pos >= on && pos < off) peakCut = std::max (peakCut, c);
                    if (pos + 32 >= on + (int) (sr * 0.005) && at5ms < 0) at5ms = c;
                    if (pos + 32 >= off + (int) (sr * 0.100) && at100 < 0) at100 = c;
                    if (pos + 32 >= off + (int) (sr * 0.150) && at150 < 0) at150 = c;
                    if (pos + 32 >= off + (int) (sr * 0.250) && at250 < 0) at250 = c;
                });
            };
            double s5, s100, s150, s250, sPk, m5, m100, m150, m250, mPk, l5, l100, l150, l250, lPk;
            burst (0.02, s5, s100, s150, s250, sPk);
            burst (0.2, m5, m100, m150, m250, mPk);
            burst (1.0, l5, l100, l150, l250, lPk);
            std::snprintf (msg, sizeof msg, "4 kHz bursts (SPEED 30 ms): %.2f dB cut 5 ms in; after a 20 ms burst %.2f / %.2f dB left at +100 / 150 ms; after 200 ms %.2f at +150 ms; after 1 s %.2f / %.2f / %.2f at +100 / 150 / 250 ms (program-dependent release)",
                           s5, s100, s150, m150, l100, l150, l250);
            check (s5 > 5.0 && s150 < 0.5 && m150 < 1.0 && l100 > s100 + 0.5 && l150 > s150 + 0.3 && l250 < 0.5, msg);
        }
    }

    //==========================================================================
    // CROSSFEED
    {
        Buf l, r;
        Lunchbox::Settings s;
        s.feedIn = true;
        struct Preset { float amount; double fc, level; const char* name; };
        for (const Preset& p : { Preset { 10.0f, 700.0, 4.5, "bs2b default" }, Preset { 7.0f, 700.0, 6.0, "C. Moy" }, Preset { 4.0f, 650.0, 9.5, "J. Meier" } })
        {
            s.feedAmount = p.amount;
            lt::impulse (sr, s, l, r, false);
            // The interaural level difference against bs2b's own filters, and the delay at low frequencies
            double worst = 0;
            for (double f : { 30.0, 100.0, 300.0, 700.0, 1500.0, 3000.0, 6000.0 })
            {
                std::complex<double> d, x;
                lt::bs2b (sr, p.fc, p.level, f, d, x);
                worst = std::max (worst, std::abs ((lt::respDb (r, f, sr) - lt::respDb (l, f, sr)) - 20.0 * std::log10 (std::abs (x / d))));
            }
            const double ild = lt::respDb (l, 30.0, sr) - lt::respDb (r, 30.0, sr);
            const double itd = std::arg (lt::respC (l, 200.0, sr) / lt::respC (r, 200.0, sr)) / (lt::twoPi * 200.0) * 1000.0;
            const double nearHf = lt::respDb (l, 10000.0, sr), farHf = lt::respDb (r, 10000.0, sr);
            std::snprintf (msg, sizeof msg, "CROSSFEED %.0f (%s, %.0f Hz / %.1f dB), hard left: far ear %.2f dB under at 30 Hz, %.1f dB under at 10 kHz, %.3f ms late at 200 Hz; follows bs2b within %.3f dB; near ear %+.2f dB at 10 kHz",
                           p.amount, p.name, p.fc, p.level, ild, nearHf - farHf, itd, worst, nearHf);
            check (std::abs (ild - p.level) < 0.15 && worst < 0.05 && itd > 0.2 && itd < 0.35 && std::abs (nearHf) < 0.5 && nearHf - farHf > 20.0, msg);
        }

        s.feedAmount = 10.0f;
        lt::impulse (sr, s, l, r, true);
        const double m100 = lt::respDb (l, 100.0, sr), m5k = lt::respDb (l, 5000.0, sr);
        std::snprintf (msg, sizeof msg, "CROSSFEED 10, centred impulse: %+.4f dB at 100 Hz, %+.4f dB at 5 kHz", m100, m5k);
        check (std::abs (m100) < 0.01 && std::abs (m5k) < 0.01, msg);

        // Mono (L == R) passes bit for bit
        {
            const int n = (int) sr;
            const Buf mono = lt::pink (n, 16, -12.0f);
            Buf ml = mono, mr = mono;
            Lunchbox lb;
            lb.prepare (sr, 256);
            lt::render (lb, ml, mr, s, 256, [&] (int pos, Lunchbox::Settings& q) { q.feedAmount = pos < n / 2 ? 10.0f : 3.0f; });
            const bool same = std::memcmp (ml.data(), mono.data(), sizeof (float) * (size_t) n) == 0 && std::memcmp (mr.data(), mono.data(), sizeof (float) * (size_t) n) == 0;
            check (same, "CROSSFEED on mono material (L = R, AMOUNT moving): bit-identical");
        }

        // One channel: nothing to feed
        Lunchbox lb;
        lb.prepare (sr, 256);
        Buf mono = lt::pink (4096, 15, -12.0f), ref = mono;
        float* chans[1] = { mono.data() };
        lb.process (chans, 1, 4096, s);
        check (std::memcmp (mono.data(), ref.data(), sizeof (float) * 4096) == 0, "CROSSFEED on one channel: untouched");
    }

    //==========================================================================
    // No clicks: every switch and knob jump during a tone
    {
        struct Case { const char* what; std::function<void (Lunchbox::Settings&)> a, b; };
        auto eqOn = [] (Lunchbox::Settings& q) { q.eqIn = true; q.lowGainDb = 6.0f; q.midGainDb = 8.0f; q.highGainDb = 5.0f; };
        const std::vector<Case> cases {
            { "EQ IN",          [] (auto& q) { q.lowGainDb = 6.0f; q.midGainDb = 8.0f; q.highGainDb = 5.0f; }, [&] (auto& q) { eqOn (q); } },
            { "EQ OUT",         [&] (auto& q) { eqOn (q); }, [] (auto& q) { q.lowGainDb = 6.0f; q.midGainDb = 8.0f; q.highGainDb = 5.0f; } },
            { "HPF off -> 300", [&] (auto& q) { eqOn (q); }, [&] (auto& q) { eqOn (q); q.hpf = 4; } },
            { "HPF 50 -> 300",  [&] (auto& q) { eqOn (q); q.hpf = 1; }, [&] (auto& q) { eqOn (q); q.hpf = 4; } },
            { "HPF 300 -> off", [&] (auto& q) { eqOn (q); q.hpf = 4; }, [&] (auto& q) { eqOn (q); } },
            { "LOW 35 -> 220",  [&] (auto& q) { eqOn (q); q.lowGainDb = 16.0f; q.lowFreq = 0; }, [&] (auto& q) { eqOn (q); q.lowGainDb = 16.0f; q.lowFreq = 3; } },
            { "LOW -16 -> +16", [&] (auto& q) { eqOn (q); q.lowGainDb = -16.0f; }, [&] (auto& q) { eqOn (q); q.lowGainDb = 16.0f; } },
            { "MID 360 -> 7.2k", [&] (auto& q) { eqOn (q); q.midGainDb = 18.0f; q.midFreq = 0; }, [&] (auto& q) { eqOn (q); q.midGainDb = 18.0f; q.midFreq = 5; } },
            { "MID -18 -> +18", [&] (auto& q) { eqOn (q); q.midGainDb = -18.0f; q.midFreq = 3; }, [&] (auto& q) { eqOn (q); q.midGainDb = 18.0f; q.midFreq = 3; } },
            { "HI Q",           [&] (auto& q) { eqOn (q); q.midGainDb = 18.0f; q.midFreq = 3; }, [&] (auto& q) { eqOn (q); q.midGainDb = 18.0f; q.midFreq = 3; q.midHighQ = true; } },
            { "HIGH -16 -> +16", [&] (auto& q) { eqOn (q); q.highGainDb = -16.0f; }, [&] (auto& q) { eqOn (q); q.highGainDb = 16.0f; } },
            { "IRON on",        [&] (auto& q) { eqOn (q); }, [&] (auto& q) { eqOn (q); q.iron = true; } },
            { "IRON off",       [&] (auto& q) { eqOn (q); q.iron = true; }, [&] (auto& q) { eqOn (q); } },
            { "DE-HARSH IN",    [] (auto& q) { q.harshAmount = 10.0f; }, [] (auto& q) { q.harshIn = true; q.harshAmount = 10.0f; } },
            { "DE-HARSH OUT",   [] (auto& q) { q.harshIn = true; q.harshAmount = 10.0f; }, [] (auto& q) { q.harshAmount = 10.0f; } },
            { "DE-HARSH 2.5k -> 6.5k", [] (auto& q) { q.harshIn = true; q.harshAmount = 10.0f; q.harshFreq = 0; }, [] (auto& q) { q.harshIn = true; q.harshAmount = 10.0f; q.harshFreq = 2; } },
            { "DE-HARSH 6.5k -> 2.5k", [] (auto& q) { q.harshIn = true; q.harshAmount = 10.0f; q.harshFreq = 2; }, [] (auto& q) { q.harshIn = true; q.harshAmount = 10.0f; q.harshFreq = 0; } },
            { "DE-HARSH AMOUNT 0 -> 10", [] (auto& q) { q.harshIn = true; q.harshAmount = 0.0f; }, [] (auto& q) { q.harshIn = true; q.harshAmount = 10.0f; } },
            { "CROSSFEED IN",   [] (auto& q) { q.feedAmount = 10.0f; }, [] (auto& q) { q.feedIn = true; q.feedAmount = 10.0f; } },
            { "CROSSFEED OUT",  [] (auto& q) { q.feedIn = true; q.feedAmount = 10.0f; }, [] (auto& q) { q.feedAmount = 10.0f; } },
            { "CROSSFEED 0 -> 10", [] (auto& q) { q.feedIn = true; q.feedAmount = 0.0f; }, [] (auto& q) { q.feedIn = true; q.feedAmount = 10.0f; } },
            { "CROSSFEED 10 -> 1", [] (auto& q) { q.feedIn = true; q.feedAmount = 10.0f; }, [] (auto& q) { q.feedIn = true; q.feedAmount = 1.0f; } },
            { "everything IN",  [] (auto&) {}, [&] (auto& q) { eqOn (q); q.hpf = 3; q.iron = true; q.harshIn = true; q.harshAmount = 10.0f; q.feedIn = true; q.feedAmount = 10.0f; } },
        };

        // Two tones (a bass and an upper-mid one that DE-HARSH sees as sticking out), different on the
        // two sides so CROSSFEED has something to do
        const int n = (int) (sr * 0.8), sw = (int) (sr * 0.3), trans = (int) (sr * 0.2);
        const Buf bassL = lt::sine (n, 150.0, sr, 0.3f), bassR = lt::sine (n, 150.0, sr, 0.1f, 1.0);
        const Buf hiL = lt::sine (n, 3300.0, sr, 0.1f), hiR = lt::sine (n, 3300.0, sr, 0.2f, 2.0);
        double worstRatio = 0, worstSecond = 0;
        const char* worstWhat = "";
        const char* worstSecondWhat = "";
        for (const auto& c : cases)
        {
            Buf l ((size_t) n), r ((size_t) n);
            for (int i = 0; i < n; ++i) { l[(size_t) i] = bassL[(size_t) i] + hiL[(size_t) i]; r[(size_t) i] = bassR[(size_t) i] + hiR[(size_t) i]; }
            Lunchbox lb;
            lb.prepare (sr, 64);
            Lunchbox::Settings s;
            lt::render (lb, l, r, s, 64, [&] (int pos, Lunchbox::Settings& q)
            {
                q = Lunchbox::Settings {};
                (pos < sw ? c.a : c.b) (q);
            });
            for (const Buf* x : { &l, &r })
            {
                const auto before = lt::slope (*x, sw - (int) (sr * 0.15), sw);
                const auto during = lt::slope (*x, sw, sw + trans);
                const auto afterS = lt::slope (*x, sw + trans, n);
                const double ratio = during.first / std::max (1e-9, std::max (before.first, afterS.first));
                const double second = during.second / std::max (1e-9, std::max (before.second, afterS.second));
                if (ratio > worstRatio) { worstRatio = ratio; worstWhat = c.what; }
                if (second > worstSecond) { worstSecond = second; worstSecondWhat = c.what; }
            }
        }
        // (For scale: with the glides and fades taken out, a click, this reads 16x - 40x and 40x - 250x)
        std::snprintf (msg, sizeof msg, "no clicks (%d switches and knob jumps during a tone): largest step %.3fx the steady tone's (%s), second difference %.3fx (%s)",
                       (int) cases.size(), worstRatio, worstWhat, worstSecond, worstSecondWhat);
        check (worstRatio < 1.25 && worstSecond < 2.5, msg);
    }

    //==========================================================================
    // Extremes, and silence
    {
        const int n = (int) sr * 3;
        Buf l = lt::pink (n, 21, 6.0f), r = lt::pink (n, 22, 6.0f);
        for (int i = n / 3; i < n / 3 + 4800; ++i) { l[(size_t) i] = (i / 50) % 2 ? 4.0f : -4.0f; r[(size_t) i] = -l[(size_t) i]; }
        Lunchbox lb;
        lb.prepare (sr, 256);
        Lunchbox::Settings s;
        lt::Rng rng (5);
        lt::render (lb, l, r, s, 100, [&] (int pos, Lunchbox::Settings& q)
        {
            q.eqIn = q.harshIn = q.feedIn = q.iron = true;
            q.lowGainDb = 1.0e9f; q.midGainDb = 18.0f; q.highGainDb = 16.0f; q.harshAmount = 1.0e9f; q.feedAmount = 10.0f;
            q.hpf = 99; q.lowFreq = -3; q.midFreq = 42; q.harshFreq = 7; q.harshSpeedMs = 0.0f;
            if (pos > n / 2)
            {
                // Now everything moving at random every block, and NaN / infinity in the knobs
                q.eqIn = rng.next() > -0.5f; q.harshIn = rng.next() > 0.0f; q.feedIn = rng.next() > 0.0f; q.iron = rng.next() > 0.0f;
                q.hpf = (int) (rng.next() * 5.0f); q.lowFreq = (int) (rng.next() * 4.0f); q.midFreq = (int) (rng.next() * 6.0f);
                q.midHighQ = rng.next() > 0.0f; q.harshFreq = (int) (rng.next() * 3.0f);
                q.lowGainDb = rng.next() * 16.0f; q.midGainDb = std::nanf (""); q.highGainDb = -INFINITY;
                q.harshAmount = rng.next() * 10.0f; q.feedAmount = INFINITY; q.harshSpeedMs = std::nanf ("");
            }
        });
        std::snprintf (msg, sizeof msg, "extreme settings (out of range, NaN, infinity, random switching, +6 dBFS noise, a +12 dBFS square): finite (meter %.2f, peak %.2f)",
                       (double) lb.getHarshReductionDb(), (double) lb.getOutputPeak());
        check (lt::allFinite (l) && lt::allFinite (r) && std::isfinite (lb.getHarshReductionDb()) && std::isfinite (lb.getOutputPeak()), msg);

        // Silence after sound, everything IN: no denormal slowdown
        Lunchbox::Settings all;
        all.eqIn = all.harshIn = all.feedIn = all.iron = true;
        all.hpf = 2; all.lowGainDb = 8.0f; all.midGainDb = 6.0f; all.highGainDb = 4.0f; all.harshAmount = 10.0f; all.feedAmount = 10.0f;
        Lunchbox u;
        u.prepare (sr, 256);
        const int m = (int) sr * 2;
        Buf nl = lt::pink (m, 23, -12.0f), nr = lt::pink (m, 24, -12.0f);
        double noiseS = 1e9;
        for (int k = 0; k < 3; ++k)   // the quickest of three: the least disturbed by other work on the machine
        {
            Buf al = nl, ar = nr;
            const auto t0 = std::chrono::steady_clock::now();
            lt::render (u, al, ar, all);
            noiseS = std::min (noiseS, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
        }
        Buf zl ((size_t) m * 10, 0.0f), zr ((size_t) m * 10, 0.0f);
        lt::render (u, zl, zr, all);   // 20 s of silence: the filters' tails decay far past the denormal range
        Buf sl ((size_t) m, 0.0f), srr ((size_t) m, 0.0f);
        const auto t0 = std::chrono::steady_clock::now();
        lt::render (u, sl, srr, all);
        const double silenceS = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool zero = true;
        for (int i = 0; i < m; ++i) zero = zero && sl[(size_t) i] == 0.0f && srr[(size_t) i] == 0.0f;
        std::snprintf (msg, sizeof msg, "silence after sound, everything IN: %.2f ms per second of stereo audio (noise %.2f ms), output exactly zero: %s",
                       1000.0 * silenceS / 2.0, 1000.0 * noiseS / 2.0, zero ? "yes" : "no");
        check (silenceS < 2.0 * noiseS + 0.002 && zero, msg);
    }
}

static void runLunchboxTests (double /*engineSr*/)
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        runLunchboxTestsAt (sr);
}
