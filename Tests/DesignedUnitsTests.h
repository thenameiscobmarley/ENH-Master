// The designed units (LATINSPHIEL PRO X4, VELVETIZER, TAKEBACK). Included into EnhDspTests.cpp after check();
// run with   EnhDspTests --designed   (every check at 44.1, 48 and 96 kHz)

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>
#include "DSP/ProX4.h"
#include "DSP/Velvetizer.h"
#include "DSP/Takeback.h"

namespace designedtest
{
    using enh::dsp::ProX4;
    using enh::dsp::Velvetizer;
    using enh::dsp::Takeback;
    using Buf = std::vector<float>;
    constexpr double twoPi = 6.283185307179586;

    /** A stereo test signal: a bass line, a midrange tone, a little noise (so every band has something). */
    inline std::array<Buf, 2> programme (double sr, double seconds, float levelDb, unsigned seed = 1)
    {
        const int n = (int) (sr * seconds);
        std::array<Buf, 2> x { Buf ((size_t) n), Buf ((size_t) n) };
        unsigned s = seed * 747796405u + 1u;
        const float g = std::pow (10.0f, levelDb / 20.0f);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            s = s * 1664525u + 1013904223u;
            const float noise = ((float) (s >> 8) / 8388608.0f - 1.0f) * 0.05f;
            const float v = (float) (0.6 * std::sin (twoPi * 55.0 * t) + 0.3 * std::sin (twoPi * 1000.0 * t) + 0.1 * std::sin (twoPi * 6000.0 * t));
            x[0][(size_t) i] = g * (v + noise);
            x[1][(size_t) i] = g * (0.9f * v - noise);
        }
        return x;
    }

    inline double rmsDb (const Buf& b, int from, int to)
    {
        double e = 0.0;
        for (int i = from; i < to; ++i) e += (double) b[(size_t) i] * b[(size_t) i];
        return 10.0 * std::log10 (e / std::max (1, to - from) + 1.0e-20);
    }

    /** Runs a unit over a stereo signal in blocks; `settingsAt (sample)` gives the settings for each block. */
    template <typename Unit, typename Settings>
    void run (Unit& unit, std::array<Buf, 2>& x, int block, std::function<Settings (int)> settingsAt)
    {
        const int n = (int) x[0].size();
        for (int pos = 0; pos < n; pos += block)
        {
            const int k = std::min (block, n - pos);
            float* ch[2] { x[0].data() + pos, x[1].data() + pos };
            unit.process (ch, 2, k, settingsAt (pos));
        }
    }

    /** High-frequency burst (above 9 kHz) in the 30 ms after `at`, over the steady level before it, dB. */
    inline double burstDb (const Buf& y, double sr, int at)
    {
        auto hp = enh::dsp::SvfCoeffs::make (sr, 9000.0, 0.7071);
        enh::dsp::SvfState a, b;
        double steady = 0.0, burst = 0.0;
        const int w = (int) (0.03 * sr);
        for (int i = at - (int) (0.3 * sr); i < at + w; ++i)
        {
            const float h = b.process (hp, a.process (hp, y[(size_t) i]).high).high;
            if (i < at - 256 && i > at - (int) (0.25 * sr)) steady = std::max (steady, (double) std::abs (h));
            if (i >= at) burst = std::max (burst, (double) std::abs (h));
        }
        return 20.0 * std::log10 ((burst + 1.0e-7) / (steady + 1.0e-5));
    }

    /** A drum-like signal: a decaying 90 Hz thump with a click, four times a second, over a quiet bed; squashed:
        through a hard limiter's curve (4x into a soft clip) - what a loud master leaves of it. */
    inline std::array<Buf, 2> drums (double sr, double seconds, bool squashed)
    {
        const int n = (int) (sr * seconds), period = (int) (sr / 4.0);
        std::array<Buf, 2> x { Buf ((size_t) n), Buf ((size_t) n) };
        for (int i = 0; i < n; ++i)
        {
            const double t = (i % period) / sr;
            float v = (float) (std::exp (-t * 30.0) * std::sin (twoPi * 90.0 * t) + 0.3 * std::exp (-t * 300.0) * std::sin (twoPi * 2500.0 * t)
                               + 0.12 * std::sin (twoPi * 220.0 * (i / sr)));
            if (squashed) v = std::tanh (4.0f * v) / 4.0f;
            x[0][(size_t) i] = x[1][(size_t) i] = 0.25f * v;
        }
        return x;
    }

    inline double crestDb (const Buf& b, int from, int to)
    {
        double pk = 0.0;
        for (int i = from; i < to; ++i) pk = std::max (pk, (double) std::abs (b[(size_t) i]));
        return 20.0 * std::log10 (pk + 1.0e-12) - rmsDb (b, from, to);
    }

    /** How far each hit's attack (its first 5 ms) stands above its body (30 - 120 ms in), dB, averaged over
        the drums' hits from `from` on. */
    inline double attackDb (const Buf& b, double sr, int from)
    {
        const int period = (int) (sr / 4.0), a = (int) (0.005 * sr), b0 = (int) (0.030 * sr), b1 = (int) (0.120 * sr);
        double sum = 0.0; int hits = 0;
        for (int h = (from / period + 1) * period; h + b1 < (int) b.size(); h += period, ++hits)
        {
            double pk = 0.0;
            for (int i = h; i < h + a; ++i) pk = std::max (pk, (double) std::abs (b[(size_t) i]));
            sum += 20.0 * std::log10 (pk + 1.0e-12) - rmsDb (b, h + b0, h + b1);
        }
        return sum / std::max (1, hits);
    }

    /** The level above 9 kHz, dB. */
    inline double airDb (const Buf& y, double sr, int from)
    {
        auto hp = enh::dsp::SvfCoeffs::make (sr, 9000.0, 0.7071);
        enh::dsp::SvfState a, b;
        double e = 0.0;
        for (size_t i = 0; i < y.size(); ++i)
        {
            const float h = b.process (hp, a.process (hp, y[i]).high).high;
            if ((int) i >= from) e += (double) h * h;
        }
        return 10.0 * std::log10 (e / (double) (y.size() - (size_t) from) + 1.0e-20);
    }
}

static void runDesignedUnitsTests (double sr)
{
    using namespace designedtest;
    std::printf ("\n== DESIGNED UNITS (PRO X4, VELVETIZER, TAKEBACK) at %.1f kHz ==\n", sr / 1000.0);

    // 1. Off: not a bit changed
    {
        auto x = programme (sr, 1.0, -18.0f), ref = x;
        ProX4 u; u.prepare (sr);
        run<ProX4, ProX4::Settings> (u, x, 256, [] (int) { return ProX4::Settings {}; });
        Velvetizer v; v.prepare (sr);
        run<Velvetizer, Velvetizer::Settings> (v, x, 256, [] (int) { return Velvetizer::Settings {}; });
        auto byp = ref;
        Velvetizer vb; vb.prepare (sr);
        run<Velvetizer, Velvetizer::Settings> (vb, byp, 256, [] (int) { Velvetizer::Settings s; s.power = true; s.bypass = true; return s; });
        Takeback t; t.prepare (sr);
        run<Takeback, Takeback::Settings> (t, x, 256, [] (int) { return Takeback::Settings {}; });
        check (x == ref && byp == ref, "off (and the Velvetizer's BYPASS): bit-for-bit what came in");
    }

    // 2. Loudness: the knobs change the colour, not the level
    {
        auto meas = [&] (auto& unit, auto settings)
        {
            auto x = programme (sr, 4.0, -18.0f), ref = x;
            using U = std::decay_t<decltype (unit)>;
            run<U, decltype (settings)> (unit, x, 256, [&] (int) { return settings; });
            const int from = (int) (2.0 * sr), to = (int) x[0].size();
            return rmsDb (x[0], from, to) - rmsDb (ref[0], from, to);
        };
        ProX4::Settings xs; xs.power = true;
        ProX4 u1; u1.prepare (sr);
        const double dx = meas (u1, xs);
        ProX4::Settings xh = xs; xh.x2 = true; xh.saturate = 10.0f; xh.populate = 10.0f;
        ProX4 u2; u2.prepare (sr);
        const double dxh = meas (u2, xh);
        Velvetizer::Settings vs; vs.power = true;
        Velvetizer v1; v1.prepare (sr);
        const double dv = meas (v1, vs);
        Velvetizer::Settings va = vs; va.balanceMode = false;
        Velvetizer v2; v2.prepare (sr);
        const double dva = meas (v2, va);
        std::printf ("  level change: PRO X4 %+.2f dB (pushed hard %+.2f), VELVETIZER BALANCE %+.2f, ADD %+.2f\n", dx, dxh, dv, dva);
        check (std::abs (dx) < 1.0 && std::abs (dxh) < 1.0, "PRO X4 comes out as loud as it went in");
        check (std::abs (dv) < 1.0, "VELVETIZER (BALANCE) comes out as loud as it went in");
        check (dva > 0.3 && dva < 2.0, "VELVETIZER (ADD) comes out a little fuller and louder");
        Takeback::Settings ts; ts.power = true;
        Takeback t1; t1.prepare (sr);
        const double dt = meas (t1, ts);
        Takeback::Settings th = ts; th.mix = 100.0f; th.sharpen = th.colour = th.raw = th.shine = 10.0f;
        Takeback t2; t2.prepare (sr);
        const double dth = meas (t2, th);
        std::printf ("  level change: TAKEBACK %+.2f dB (everything up %+.2f)\n", dt, dth);
        check (std::abs (dt) < 1.0 && std::abs (dth) < 1.0, "TAKEBACK comes out as loud as it went in");
    }

    // 3. PID: the density held at the DRIVE knob's setpoint
    {
        auto density = [&] (bool pid, float levelDb)
        {
            auto x = programme (sr, 6.0, levelDb);
            ProX4 u; u.prepare (sr);
            ProX4::Settings s; s.power = true; s.pid = pid; s.p = 5.0f; s.i = 0.5f;
            run<ProX4, ProX4::Settings> (u, x, 256, [&] (int) { return s; });
            return u.getReadout().pvDb[1];   // 200 Hz - 1 kHz: the midrange tone
        };
        const float loudOff = density (false, -10.0f), quietOff = density (false, -24.0f);
        const float loudOn = density (true, -10.0f), quietOn = density (true, -24.0f);
        const float sp = -40.0f + 0.30f * ProX4::Settings {}.side[0].drive[1];
        std::printf ("  midrange density: PID off %.1f / %.1f dB (loud / quiet), on %.1f / %.1f dB (setpoint %.1f)\n", loudOff, quietOff, loudOn, quietOn, sp);
        check (std::abs (loudOff - quietOff) < 2.0f, "the valves' colour is the same loud or quiet (driven against each band's level)");
        check (std::abs (quietOn - sp) < 2.0f && std::abs (loudOn - sp) < 2.0f, "PID brings the density to its setpoint, loud and quiet");
        check (std::abs (loudOff - sp) > 3.0f, "PID actually moves it (off, the density is elsewhere)");
    }

    // 4. MONO: one channel out on both sides
    {
        auto x = programme (sr, 1.0, -18.0f);
        ProX4 u; u.prepare (sr);
        run<ProX4, ProX4::Settings> (u, x, 256, [] (int) { ProX4::Settings s; s.power = true; s.mono = true; return s; });
        double diff = 0.0;
        for (size_t i = (size_t) (0.1 * sr); i < x[0].size(); ++i) diff = std::max (diff, (double) std::abs (x[0][i] - x[1][i]));
        check (diff < 1.0e-6, "PRO X4 MONO: the same on both sides");
    }

    // 4b. TAKEBACK: SHARPEN gives a squashed drum its attack back, BLUR takes it away; AUTO hears the
    //     difference; SHINE builds a top octave that was not there
    {
        auto after = [&] (bool squashed, Takeback::Settings s, float* lost = nullptr)
        {
            auto x = drums (sr, 4.0, squashed);
            Takeback t; t.prepare (sr);
            run<Takeback, Takeback::Settings> (t, x, 256, [&] (int) { return s; });
            if (lost != nullptr) *lost = t.getReadout().lost;
            return x;
        };
        auto crestAfter = [&] (bool squashed, Takeback::Settings s, float* lost = nullptr)
        {
            auto x = after (squashed, s, lost);
            return crestDb (x[0], (int) (2.0 * sr), (int) x[0].size());
        };
        Takeback::Settings flat; flat.power = true; flat.autoOn = false; flat.mix = 100.0f; flat.sharpen = flat.colour = flat.raw = flat.shine = 0.0f;
        Takeback::Settings sharp = flat; sharp.sharpen = 10.0f;
        Takeback::Settings blur = flat; blur.blur = 10.0f;
        const double c0 = crestAfter (true, flat), cs = crestAfter (true, sharp);
        const double ao = attackDb (after (false, flat)[0], sr, (int) (2.0 * sr)), ab = attackDb (after (false, blur)[0], sr, (int) (2.0 * sr));
        std::printf ("  drums: squashed crest %.1f dB, SHARPEN %.1f; open attack over body %.1f dB, BLUR %.1f\n", c0, cs, ao, ab);
        check (cs > c0 + 2.0, "TAKEBACK SHARPEN gives the attacks back (2 dB more crest at least)");
        check (ab < ao - 2.0, "TAKEBACK BLUR rounds the attacks off (2 dB softer at least)");
        Takeback::Settings au = flat; au.autoOn = true;
        float lostSquashed = 0.0f, lostOpen = 0.0f;
        crestAfter (true, au, &lostSquashed);
        crestAfter (false, au, &lostOpen);
        std::printf ("  AUTO: what was lost - squashed %.2f, open %.2f\n", lostSquashed, lostOpen);
        check (lostSquashed > lostOpen + 0.3f, "TAKEBACK AUTO hears a squashed sound from an open one");

        // SHINE: tones at 1, 3.5 and 5 kHz (nothing above 9 kHz): the air it builds
        auto air = [&] (float shine)
        {
            const int n = (int) (2.0 * sr);
            std::array<Buf, 2> x { Buf ((size_t) n), Buf ((size_t) n) };
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                x[0][(size_t) i] = x[1][(size_t) i] = (float) (0.1 * std::sin (designedtest::twoPi * 1000.0 * t) + 0.06 * std::sin (designedtest::twoPi * 3500.0 * t) + 0.05 * std::sin (designedtest::twoPi * 5000.0 * t));
            }
            Takeback::Settings s = flat; s.shine = shine;
            Takeback t; t.prepare (sr);
            run<Takeback, Takeback::Settings> (t, x, 256, [&] (int) { return s; });
            return airDb (x[0], sr, (int) sr);
        };
        const double a0 = air (0.0f), a10 = air (10.0f);
        std::printf ("  above 9 kHz: %.1f dB without SHINE, %.1f dB with it all the way\n", a0, a10);
        check (a10 > a0 + 10.0 && a10 < -30.0, "TAKEBACK SHINE builds air, well under the sound");
    }

    // 5. Every switch flips without a click (a steady tone, the switch at 1 s)
    {
        int worst = 0;
        auto flip = [&] (const char* name, auto before, auto after, auto& unit)
        {
            auto x = programme (sr, 1.5, -18.0f);
            const int at = (int) sr;
            using U = std::decay_t<decltype (unit)>;
            using S = decltype (before);
            run<U, S> (unit, x, 256, [&] (int pos) { return pos >= at ? after : before; });
            const double b = burstDb (x[0], sr, (at / 256 + 1) * 256);
            if (b > 12.0) { ++worst; std::printf ("  %-26s a burst %+.1f dB over the steady sound\n", name, b); }
        };
        ProX4::Settings xo; xo.power = true;
        auto x4 = [&] (const char* name, std::function<void (ProX4::Settings&)> f) { ProX4::Settings a = xo; f (a); ProX4 u; u.prepare (sr); flip (name, xo, a, u); };
        x4 ("PRO X4 PWR", [] (ProX4::Settings& s) { s.power = false; });
        x4 ("PRO X4 MONO", [] (ProX4::Settings& s) { s.mono = true; });
        x4 ("PRO X4 X2", [] (ProX4::Settings& s) { s.x2 = true; });
        x4 ("PRO X4 PID", [] (ProX4::Settings& s) { s.pid = true; });
        Velvetizer::Settings vo; vo.power = true;
        auto vz = [&] (const char* name, std::function<void (Velvetizer::Settings&)> f) { Velvetizer::Settings a = vo; f (a); Velvetizer u; u.prepare (sr); flip (name, vo, a, u); };
        vz ("VELVETIZER POWER", [] (Velvetizer::Settings& s) { s.power = false; });
        vz ("VELVETIZER BYPASS", [] (Velvetizer::Settings& s) { s.bypass = true; });
        vz ("VELVETIZER ADD / BALANCE", [] (Velvetizer::Settings& s) { s.balanceMode = false; });
        for (int m = 0; m < Velvetizer::numColours; ++m)
        {
            vz ("VELVETIZER COLOR TYPE A", [m] (Velvetizer::Settings& s) { s.colourA = m; });
            vz ("VELVETIZER COLOR TYPE B", [m] (Velvetizer::Settings& s) { s.colourB = m; });
        }
        Takeback::Settings to; to.power = true;
        auto tk = [&] (const char* name, std::function<void (Takeback::Settings&)> f) { Takeback::Settings a = to; f (a); Takeback u; u.prepare (sr); flip (name, to, a, u); };
        tk ("TAKEBACK POWER", [] (Takeback::Settings& s) { s.power = false; });
        tk ("TAKEBACK AUTO", [] (Takeback::Settings& s) { s.autoOn = false; });
        check (worst == 0, "no switch clicks (" + juce::String (worst) + " clicked)");
    }
}
