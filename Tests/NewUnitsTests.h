// The newer units (DSP/units): each off bit-for-bit, at its defaults and at random settings always finite and
// under the ceiling, its POWER click-free, and doing something when on. Included into EnhDspTests.cpp after
// check(); run with   EnhDspTests --units16
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "DSP/DesignedUnits.h"
#include "DSP/units/Units.h"

namespace newunitstest
{
    using Buf = std::vector<float>;
    inline std::array<Buf, 2> music (double sr, double seconds, unsigned seed = 7)
    {
        const int n = (int) (sr * seconds); std::array<Buf, 2> x { Buf ((size_t) n), Buf ((size_t) n) };
        unsigned s = seed;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr, beat = std::fmod (t, 0.5);
            s = s * 1664525u + 1013904223u; const float noise = ((float) (s >> 8) / 8388608.0f - 1.0f);
            const float kick = (float) (std::exp (-beat * 25.0) * std::sin (6.2831853 * 60.0 * beat));
            const float v = 0.25f * kick + 0.08f * (float) std::sin (6.2831853 * 220.0 * t) + 0.05f * (float) std::sin (6.2831853 * 1330.0 * t) + 0.02f * noise * (float) std::exp (-beat * 40.0);
            x[0][(size_t) i] = v; x[1][(size_t) i] = 0.9f * v + 0.01f * noise;
        }
        return x;
    }
    inline std::array<Buf, 2> silence12 (double sr, double seconds) { return { Buf ((size_t) (seconds * sr)), Buf ((size_t) (seconds * sr)) }; }
    inline std::vector<float> defaults (int k)
    {
        const auto& in = enh::dsp::units::info[k];
        std::vector<float> p ((size_t) in.numParams);
        for (int i = 0; i < in.numParams; ++i) p[(size_t) i] = enh::dsp::designed::params[(size_t) (in.firstParam + i)].defaultValue;
        return p;
    }
    inline void run (enh::dsp::units::RackUnit& u, std::array<Buf, 2>& x, int block, const std::function<const float* (int)>& pAt)
    {
        for (int pos = 0; pos < (int) x[0].size(); pos += block)
        {
            const int n = std::min (block, (int) x[0].size() - pos);
            float* ch[2] { x[0].data() + pos, x[1].data() + pos };
            u.process (ch, 2, n, pAt (pos));
        }
    }
}

static void runNewUnitsTests16 (double sr)
{
    using namespace newunitstest;
    namespace U = enh::dsp::units;
    std::printf ("\n== THE NEWER UNITS (16) at %.1f kHz ==\n", sr / 1000.0);
    int bad = 0;
    for (int k = 0; k < U::count; ++k)
    {
        const auto& in = U::info[k];
        auto p = defaults (k);
        // off: bit-for-bit
        { auto x = music (sr, 0.5), ref = x; auto u = U::make (k); u->prepare (sr, 512); p[0] = 0.0f;
          run (*u, x, 256, [&] (int) { return p.data(); });
          if (x != ref) { ++bad; std::printf ("  [FAIL] %s: off is not bit-for-bit\n", in.name); } }
        // on, defaults and 20 random settings: finite, under the ceiling, and it does something
        unsigned s = 12345u + (unsigned) k;
        double changed = 0.0; float peak = 0.0f; bool finite = true;
        for (int trial = 0; trial <= 20; ++trial)
        {
            auto q = defaults (k); q[0] = 1.0f;
            if (trial > 0)
                for (int i = 1; i < in.numParams; ++i)
                {
                    const auto& d = enh::dsp::designed::params[(size_t) (in.firstParam + i)];
                    s = s * 1664525u + 1013904223u;
                    q[(size_t) i] = d.minValue + (d.maxValue - d.minValue) * ((float) (s >> 8) / 16777216.0f);
                    if (d.kind != 0) q[(size_t) i] = std::round (q[(size_t) i]);
                }
            auto x = music (sr, 1.5, 3u + (unsigned) trial), ref = x; auto u = U::make (k); u->prepare (sr, 512);
            run (*u, x, 128 + 64 * (trial % 4), [&] (int) { return q.data(); });
            for (int c = 0; c < 2; ++c) for (size_t i = 0; i < x[c].size(); ++i)
            {
                if (! std::isfinite (x[c][i])) finite = false;
                peak = std::max (peak, std::abs (x[c][i]));
                if (trial == 0) changed += std::abs ((double) x[c][i] - ref[c][i]);
            }
        }
        // (a visualiser - HYPERCUBE - must leave the sound exactly as it was, even on)
        const bool visualiser = std::string (in.key) == "hypercube" || std::string (in.key) == "tuner";   // (RACK TUNER too: its level match is the engine's)
        const bool ok = finite && peak <= 2.0f && (visualiser ? changed == 0.0 : changed > 1.0e-3);
        if (! ok) ++bad;
        std::printf ("  [%s] %-14s finite %s, peak %.2f, %s\n", ok ? "PASS" : "FAIL", in.name, finite ? "yes" : "NO", peak,
                     visualiser ? (changed == 0.0 ? "the sound untouched (a visualiser)" : "CHANGES THE SOUND (a visualiser must not)") : changed > 1.0e-3 ? "changes the sound yes" : "changes the sound NO");
        // POWER on at 0.5 s and off at 1 s: no click (the high end of the difference stays small)
        { auto x = music (sr, 1.5), ref = x; auto u = U::make (k); u->prepare (sr, 512); auto q = defaults (k);
          run (*u, x, 256, [&] (int pos) { q[0] = pos >= (int) (0.5 * sr) && pos < (int) sr ? 1.0f : 0.0f; return q.data(); });
          float jump = 0.0f; for (int i = 1; i < (int) x[0].size(); ++i) jump = std::max (jump, std::abs ((x[0][(size_t) i] - x[0][(size_t) i - 1]) - (ref[0][(size_t) i] - ref[0][(size_t) i - 1])));
          if (jump > 0.25f) { ++bad; std::printf ("  [FAIL] %s: POWER clicks (%.3f)\n", in.name, jump); } }
    }
    check (bad == 0, "every newer unit: off bit-for-bit, finite, under the ceiling, click-free, and doing something");
}

// RAY ROOM: its crackle and pops only when asked for and only with sound playing, a bigger room rings
// longer, and what it publishes for its screen stays inside the room
static void runRayRoomTests (double sr)
{
    using namespace newunitstest;
    namespace U = enh::dsp::units;
    int k = -1; for (int i = 0; i < U::count; ++i) if (std::string (U::info[i].key) == "rayroom") k = i;
    std::printf ("\n== RAY ROOM at %.1f kHz ==\n", sr / 1000.0);
    if (k < 0) { check (false, "RAY ROOM is in the unit list"); return; }
    auto runWith = [&] (float space, float mix, float crackle, float pop, std::array<Buf, 2> x, std::vector<float>* state = nullptr)
    {
        auto u = U::make (k); u->prepare (sr, 512); auto q = defaults (k);
        q[0] = 1.0f; q[1] = space; q[3] = crackle; q[4] = pop; q[5] = mix;
        run (*u, x, 256, [&] (int) { return q.data(); });
        if (state != nullptr) { state->assign (256, 0.0f); state->resize ((size_t) u->displayState (state->data(), 256)); }
        return x;
    };
    auto diffEnergy = [] (const std::array<Buf, 2>& a, const std::array<Buf, 2>& b) { double e = 0; for (int c = 0; c < 2; ++c) for (size_t i = 0; i < a[c].size(); ++i) { const double d = a[c][i] - b[c][i]; e += d * d; } return e; };
    const auto m = music (sr, 3.0);
    const auto noGrains = runWith (4.0f, 0.0f, 0.0f, 0.0f, m), grains = runWith (4.0f, 0.0f, 10.0f, 10.0f, m);
    const double added = diffEnergy (grains, noGrains), silentAdded = diffEnergy (runWith (4.0f, 0.0f, 10.0f, 10.0f, std::array<Buf, 2> { Buf (m[0].size()), Buf (m[1].size()) }), std::array<Buf, 2> { Buf (m[0].size()), Buf (m[1].size()) });
    std::printf ("  crackle + pops at 10: %.3g added over music, %.3g over silence\n", added, silentAdded);
    check (added > 1.0e-4 && silentAdded < 1.0e-12, "RAY ROOM: CRACKLE and POPPING add grains from the track, and nothing over silence");
    check (diffEnergy (noGrains, m) < 1.0e-12, "RAY ROOM: MIX 0 with CRACKLE and POPPING at 0 leaves the sound as it was");
    // a click, then how long the room rings (energy after 150 ms)
    std::array<Buf, 2> click { Buf ((size_t) (sr * 1.5)), Buf ((size_t) (sr * 1.5)) }; click[0][(size_t) (0.1 * sr)] = click[1][(size_t) (0.1 * sr)] = 0.8f;
    auto tail = [&] (float space) { auto y = runWith (space, 100.0f, 0.0f, 0.0f, click); double e = 0; for (int c = 0; c < 2; ++c) for (size_t i = (size_t) (0.25 * sr); i < y[c].size(); ++i) e += (double) y[c][i] * y[c][i]; return e; };
    const double small = tail (1.0f), big = tail (9.0f);
    std::printf ("  tail after 150 ms: SPACE 1 %.3g, SPACE 9 %.3g\n", small, big);
    check (big > 2.0 * small, "RAY ROOM: a bigger room rings longer");
    std::vector<float> st; runWith (6.0f, 50.0f, 8.0f, 8.0f, music (sr, 2.0), &st);
    bool ok = st.size() == (size_t) enh::dsp::units::room::State::size;
    if (ok)
    {
        enh::dsp::units::room::State s; s.read (st.data());
        for (int d = 0; d < s.dots; ++d) ok = ok && std::abs (s.dot[(size_t) (d * 4)]) <= s.W * 1.05f && std::abs (s.dot[(size_t) (d * 4 + 1)]) <= s.D * 1.05f;
        ok = ok && s.levelL >= 0.0f && s.levelL < 4.0f && s.W > 1.0f;
    }
    check (ok, "RAY ROOM: its screen's state is complete and every dot is inside the room");
}

// The simulations (units/Sims.h): each one's own behaviour, and what it publishes for its screen
static void runSimTests (double sr)
{
    using namespace newunitstest;
    namespace U = enh::dsp::units;
    auto find = [] (const char* key) { for (int i = 0; i < U::count; ++i) if (std::string (U::info[i].key) == key) return i; return -1; };
    std::printf ("\n== THE SIMULATIONS at %.1f kHz ==\n", sr / 1000.0);
    auto energy = [] (const std::array<Buf, 2>& x, size_t from, size_t to) { double e = 0; for (int c = 0; c < 2; ++c) for (size_t i = from; i < std::min (to, x[c].size()); ++i) e += (double) x[c][i] * x[c][i]; return e / (double) std::max<size_t> (1, to - from); };
    auto silence = [&] (double secs) { return std::array<Buf, 2> { Buf ((size_t) (secs * sr)), Buf ((size_t) (secs * sr)) }; };
    std::vector<float> st (192);

    // VINYL DECK: the dust is on the record - at DUST 10 the screen shows all 24 specks, and they tick over silence
    // (quietly: a record's surface, never loud); at DUST 0 and AGE 0 silence stays nearly silent
    if (const int k = find ("vinyl"); k >= 0)
    {
        auto u = U::make (k); u->prepare (sr, 512); auto q = defaults (k); q[0] = 1.0f; q[3] = 10.0f;
        auto x = silence (3.0); run (*u, x, 256, [&] (int) { return q.data(); });
        float peak = 0.0f; for (auto& ch : x) for (float v : ch) peak = std::max (peak, std::abs (v));
        const int n = u->displayState (st.data(), (int) st.size());
        std::printf ("  VINYL DECK: dust 10 over silence peaks %.3f, %d specks shown\n", peak, n > 4 ? (int) st[4] : -1);
        check (peak > 0.005f && peak < 0.25f && n > 4 && (int) st[4] == 24, "VINYL DECK: DUST ticks over silence, quietly, and every speck is on its screen");
    }
    else check (false, "VINYL DECK is in the unit list");

    // ROTARY CAB: FAST brings the horn up to speed within ACCEL's time; the mics a quarter turn apart make it stereo
    if (const int k = find ("rotary"); k >= 0)
    {
        auto u = U::make (k); u->prepare (sr, 512); auto q = defaults (k); q[0] = 1.0f; q[1] = 1.0f; q[2] = 0.0f;
        auto x = music (sr, 3.0); for (auto& v : x[1]) v = 0.0f; x[1] = x[0];
        run (*u, x, 256, [&] (int) { return q.data(); });
        u->displayState (st.data(), (int) st.size());
        double side = 0; for (size_t i = (size_t) sr; i < x[0].size(); ++i) side += std::abs ((double) x[0][i] - x[1][i]);
        std::printf ("  ROTARY CAB: horn at %.2f Hz after 3 s on FAST, side %.3g\n", st[2], side);
        check (st[2] > 6.0f && side > 1.0, "ROTARY CAB: FAST spins the horn up, and a mono input comes out moving in stereo");
    }
    else check (false, "ROTARY CAB is in the unit list");

    // CASSETTE DECK: STOP winds it down to silence (hiss and all), and letting go brings the sound back
    if (const int k = find ("cassette"); k >= 0)
    {
        auto u = U::make (k); u->prepare (sr, 512); auto q = defaults (k); q[0] = 1.0f;
        auto x = music (sr, 4.0);
        run (*u, x, 256, [&] (int pos) { q[7] = pos >= (int) sr && pos < (int) (2.5 * sr) ? 1.0f : 0.0f; return q.data(); });
        const double playing = energy (x, (size_t) (0.5 * sr), (size_t) sr), stopped = energy (x, (size_t) (2.0 * sr), (size_t) (2.5 * sr)), back = energy (x, (size_t) (3.3 * sr), (size_t) (4.0 * sr));
        std::printf ("  CASSETTE DECK: playing %.3g, stopped %.3g, back %.3g\n", playing, stopped, back);
        check (stopped < playing * 1.0e-4 && back > playing * 0.3, "CASSETTE DECK: STOP winds down to silence, and it plays again after");
    }
    else check (false, "CASSETTE DECK is in the unit list");

    // TAPE ECHO: a click comes back TIME later (head 1); INTENSITY past 100 % runs away but the tape holds it
    if (const int k = find ("tapeecho"); k >= 0)
    {
        auto q = defaults (k); q[0] = 1.0f; q[1] = 250.0f; q[2] = 50.0f; q[3] = 0.0f; q[4] = 0.0f; q[6] = 100.0f;
        auto u = U::make (k); u->prepare (sr, 512);
        auto x = silence (1.0); x[0][(size_t) (0.1 * sr)] = x[1][(size_t) (0.1 * sr)] = 0.8f;
        run (*u, x, 256, [&] (int) { return q.data(); });
        size_t at = 0; float best = 0.0f; for (size_t i = (size_t) (0.15 * sr); i < (size_t) (0.6 * sr); ++i) if (std::abs (x[0][i]) > best) { best = std::abs (x[0][i]); at = i; }
        const double ms = 1000.0 * ((double) at / sr - 0.1);
        auto v = U::make (k); v->prepare (sr, 512); q[2] = 110.0f; q[3] = 5.0f;
        auto y = music (sr, 1.0); y[0].resize ((size_t) (8.0 * sr)); y[1].resize ((size_t) (8.0 * sr));
        run (*v, y, 256, [&] (int) { return q.data(); });
        float peak = 0.0f; bool finite = true; for (auto& ch : y) for (float s : ch) { finite = finite && std::isfinite (s); peak = std::max (peak, std::abs (s)); }
        const int n = v->displayState (st.data(), (int) st.size());
        std::printf ("  TAPE ECHO: first echo at %.1f ms (TIME 250), runaway at 110 %% peaks %.2f, %d echoes shown\n", ms, peak, n > 5 ? (int) st[5] : -1);
        check (std::abs (ms - 250.0) < 3.0 && finite && peak < 2.0f && n > 5, "TAPE ECHO: echoes at TIME, and past 100 % it runs away but holds");
    }
    else check (false, "TAPE ECHO is in the unit list");
}

// The second ten simulations (units/Sims2.h): each one's own physics or behaviour
static void runSimTests2 (double sr)
{
    using namespace newunitstest;
    namespace U = enh::dsp::units;
    auto find = [] (const char* key) { for (int i = 0; i < U::count; ++i) if (std::string (U::info[i].key) == key) return i; return -1; };
    auto rms = [] (const Buf& b, size_t from, size_t to) { double e = 0; to = std::min (to, b.size()); for (size_t i = from; i < to; ++i) e += (double) b[i] * b[i]; return std::sqrt (e / (double) std::max<size_t> (1, to - from)); };
    auto sine = [&] (double hz, double secs, float a) { std::array<Buf, 2> x { Buf ((size_t) (secs * sr)), Buf ((size_t) (secs * sr)) }; for (size_t i = 0; i < x[0].size(); ++i) x[0][i] = x[1][i] = a * (float) std::sin (6.283185307 * hz * (double) i / sr); return x; };
    auto with = [&] (int k, std::initializer_list<std::pair<int, float>> set) { auto q = defaults (k); q[0] = 1.0f; for (auto [i, v] : set) q[(size_t) i] = v; return q; };
    auto process = [&] (int k, std::vector<float> q, std::array<Buf, 2> x) { auto u = U::make (k); u->prepare (sr, 512); run (*u, x, 256, [&] (int) { return q.data(); }); return x; };
    std::printf ("\n== THE SIMULATIONS, SECOND TEN at %.1f kHz ==\n", sr / 1000.0);
    std::vector<float> st (192);

    if (const int k = find ("valveamp"); k >= 0)
    {
        const auto in = sine (220.0, 1.0, 0.3f);
        auto diff = [&] (float drive) { auto y = process (k, with (k, { { 1, drive } }), in); double e = 0; for (size_t i = (size_t) (0.2 * sr); i < y[0].size(); ++i) { const double d = y[0][i] - in[0][i]; e += d * d; } return e; };
        const auto y5 = process (k, with (k, { { 1, 5.0f } }), in);
        const double lo = diff (1.0f), hi = diff (10.0f), gainDb = 20.0 * std::log10 (rms (y5[0], (size_t) (0.2 * sr), y5[0].size()) / rms (in[0], (size_t) (0.2 * sr), in[0].size()));
        std::printf ("  VALVE AMP: change at DRIVE 1 %.3g, DRIVE 10 %.3g; level at DRIVE 5 %+.1f dB\n", lo, hi, gainDb);
        check (hi > 3.0 * lo && std::abs (gainDb) < 6.0, "VALVE AMP: DRIVE colours it more, and the level stays near the input's");
    }
    if (const int k = find ("speakercab"); k >= 0)
    {
        auto gain = [&] (double hz) { const auto in = sine (hz, 0.6, 0.2f); const auto y = process (k, with (k, {}), in); return 20.0 * std::log10 (rms (y[0], (size_t) (0.2 * sr), y[0].size()) / rms (in[0], (size_t) (0.2 * sr), in[0].size())); };
        const double g1k = gain (1000.0), g10k = gain (10000.0), g30 = gain (30.0);
        std::printf ("  SPEAKER CAB: 30 Hz %+.1f dB, 1 kHz %+.1f dB, 10 kHz %+.1f dB\n", g30, g1k, g10k);
        check (std::abs (g1k) < 9.0 && g10k < g1k - 15.0 && g30 < g1k - 10.0, "SPEAKER CAB: a speaker's band - no deep lows, no extreme highs");
    }
    if (const int k = find ("radio"); k >= 0)
    {
        auto u = U::make (k); u->prepare (sr, 512); auto q = with (k, { { 3, 0.0f } }); auto x = sine (8000.0, 0.5, 0.3f); const auto in = x;
        run (*u, x, 256, [&] (int) { return q.data(); });
        const double hf = 20.0 * std::log10 (rms (x[0], (size_t) (0.2 * sr), x[0].size()) / rms (in[0], (size_t) (0.2 * sr), in[0].size()));
        auto v = U::make (k); v->prepare (sr, 512); auto q2 = with (k, { { 2, 8.0f } }); auto y = music (sr, 0.5); run (*v, y, 256, [&] (int) { return q2.data(); });
        v->displayState (st.data(), (int) st.size());
        std::printf ("  RADIO: AM passes 8 kHz at %+.1f dB; TUNE 8 leaves %.2f of the signal\n", hf, st[1]);
        check (hf < -15.0 && st[1] < 0.2f, "RADIO: AM is narrow, and off the station the signal is gone");
    }
    if (const int k = find ("pendulum"); k >= 0)
    {
        auto u = U::make (k); u->prepare (sr, 512); auto q = with (k, { { 1, 100.0f }, { 5, 0.0f } });
        auto x = silence12 (sr, 12.0);
        int crossings = 0; float prev = 0.0f, peak = 0.0f;
        run (*u, x, 256, [&] (int pos) { u->displayState (st.data(), (int) st.size());
            if (pos > (int) (2.0 * sr)) { if ((st[0] < 0.0f) != (prev < 0.0f)) ++crossings; peak = std::max (peak, std::abs (st[0])); }
            prev = st[0]; return q.data(); });
        const double period = 2.0 * 10.0 / std::max (1, crossings), expect = 6.2832 * std::sqrt (1.0 / 9.81);
        std::printf ("  PENDULUM: 1 m swings every %.2f s (a real one: %.2f s), kept to %.2f rad (SWING 5: %.2f)\n", period, expect, peak, st[1]);
        check (std::abs (period - expect) < 0.25 && std::abs (peak - st[1]) < 0.35f * st[1], "PENDULUM: it swings as a real pendulum does, and is kept swinging");
    }
    if (const int k = find ("bounce"); k >= 0)
    {
        auto x = silence12 (sr, 2.0); x[0][(size_t) (0.1 * sr)] = x[1][(size_t) (0.1 * sr)] = 0.8f;
        const auto y = process (k, with (k, { { 1, 400.0f }, { 2, 0.7f }, { 3, 0.0f }, { 4, 0.0f }, { 5, 100.0f } }), x);
        auto peakIn = [&] (double a, double b, size_t& at) { float m = 0.0f; for (size_t i = (size_t) (a * sr); i < (size_t) (b * sr); ++i) if (std::abs (y[0][i]) > m) { m = std::abs (y[0][i]); at = i; } return m; };
        size_t a1 = 0, a2 = 0; const float p1 = peakIn (0.3, 0.6, a1), p2 = peakIn (0.6, 0.9, a2);
        const double t1 = 1000.0 * ((double) a1 / sr - 0.1), t2 = 1000.0 * ((double) a2 / sr - 0.1);
        std::printf ("  BOUNCE DELAY: echoes at %.0f and %.0f ms (400, 680), %.2f then %.2f\n", t1, t2, p1, p2);
        check (std::abs (t1 - 400.0) < 5.0 && std::abs (t2 - 680.0) < 5.0 && p2 < p1, "BOUNCE DELAY: each bounce sooner and quieter, by BOUNCE");
    }
    if (const int k = find ("sympathy"); k >= 0)
    {
        auto burst = music (sr, 0.3); burst[0].resize ((size_t) (2.5 * sr)); burst[1].resize ((size_t) (2.5 * sr));
        auto tail = [&] (float decay) { auto y = process (k, with (k, { { 3, decay }, { 5, 100.0f } }), burst); return rms (y[0], (size_t) (1.2 * sr), y[0].size()); };
        const double shortT = tail (1.0f), longT = tail (10.0f);
        std::printf ("  SYMPATHY: ring 0.9 s after the sound stops: DECAY 1 %.3g, DECAY 10 %.3g\n", shortT, longT);
        check (longT > 5.0 * shortT && longT > 1.0e-4, "SYMPATHY: the strings ring on after the sound, as long as DECAY says");
    }
    if (const int k = find ("flyby"); k >= 0)
    {
        const auto y = process (k, with (k, { { 1, 30.0f }, { 2, 8.0f }, { 3, 0.0f }, { 4, 0.0f } }), sine (1000.0, 4.0, 0.3f));
        auto rate = [&] (double a, double b) { int n = 0; for (size_t i = (size_t) (a * sr) + 1; i < (size_t) (b * sr); ++i) if ((y[0][i] >= 0.0f) != (y[0][i - 1] >= 0.0f)) ++n; return 0.5 * n / (b - a); };
        const double coming = rate (1.0, 1.7), going = rate (2.3, 3.0);
        std::printf ("  FLYBY: 1 kHz heard at %.0f Hz coming, %.0f Hz going (30 m/s)\n", coming, going);
        check (coming > 1050.0 && going < 950.0, "FLYBY: the pitch rises coming and falls going (Doppler)");
    }
    if (const int k = find ("tesla"); k >= 0)
    {
        const auto quiet = process (k, with (k, { { 5, 100.0f } }), silence12 (sr, 1.0));
        const auto loud = process (k, with (k, { { 5, 100.0f } }), sine (220.0, 1.0, 0.4f));
        float pq = 0.0f, pl = 0.0f; for (float v : quiet[0]) pq = std::max (pq, std::abs (v)); for (float v : loud[0]) pl = std::max (pl, std::abs (v));
        std::printf ("  TESLA COIL: silence peaks %.2g, a loud note %.2f\n", pq, pl);
        check (pq < 1.0e-6f && pl > 0.05f && pl < 1.0f, "TESLA COIL: no arc over silence; a loud note strikes it, never louder than full scale");
    }
    if (const int k = find ("talkbox"); k >= 0)
    {
        auto f1 = [&] (float vowel) { auto u = U::make (k); u->prepare (sr, 512); auto q = with (k, { { 1, vowel }, { 2, 0.0f } }); auto x = music (sr, 0.3); run (*u, x, 256, [&] (int) { return q.data(); }); u->displayState (st.data(), (int) st.size()); return st[1]; };
        const float a = f1 (0.0f), i = f1 (2.0f);
        std::printf ("  TALK BOX: F1 at A %.0f Hz, at I %.0f Hz\n", a, i);
        check (a > 650.0f && i < 350.0f, "TALK BOX: the formants move with the vowel (A open, I closed)");
    }
    if (const int k = find ("lavalamp"); k >= 0)
    {
        auto u = U::make (k); u->prepare (sr, 512); auto q = with (k, { { 1, 10.0f } }); auto x = music (sr, 20.0, 5u);
        float lo = 1.0f, hi = 0.0f;
        run (*u, x, 512, [&] (int) { u->displayState (st.data(), (int) st.size()); lo = std::min (lo, st[2]); hi = std::max (hi, st[2]); return q.data(); });
        std::printf ("  LAVA LAMP: a blob went between %.2f and %.2f of the lamp's height in 20 s\n", lo, hi);
        check (hi - lo > 0.3f, "LAVA LAMP: the wax rises and sinks");
    }
}

// The twenty mastering simulations (units/Sims3.h): each one's own job
static void runSimTests3 (double sr)
{
    using namespace newunitstest;
    namespace U = enh::dsp::units;
    auto find = [] (const char* key) { for (int i = 0; i < U::count; ++i) if (std::string (U::info[i].key) == key) return i; return -1; };
    auto rms = [] (const Buf& b, size_t from, size_t to) { double e = 0; to = std::min (to, b.size()); for (size_t i = from; i < to; ++i) e += (double) b[i] * b[i]; return std::sqrt (e / (double) std::max<size_t> (1, to - from)); };
    auto db = [] (double a, double b) { return 20.0 * std::log10 (std::max (1.0e-12, a) / std::max (1.0e-12, b)); };
    auto sine = [&] (double hz, double secs, float a) { std::array<Buf, 2> x { Buf ((size_t) (secs * sr)), Buf ((size_t) (secs * sr)) }; for (size_t i = 0; i < x[0].size(); ++i) x[0][i] = x[1][i] = a * (float) std::sin (6.283185307 * hz * (double) i / sr); return x; };
    auto with = [&] (int k, std::initializer_list<std::pair<int, float>> set) { auto q = defaults (k); q[0] = 1.0f; for (auto [i, v] : set) q[(size_t) i] = v; return q; };
    auto process = [&] (int k, std::vector<float> q, std::array<Buf, 2> x) { auto u = U::make (k); u->prepare (sr, 512); run (*u, x, 256, [&] (int) { return q.data(); }); return x; };
    auto peakOf = [] (const std::array<Buf, 2>& x, size_t from = 0) { float p = 0.0f; for (auto& c : x) for (size_t i = from; i < c.size(); ++i) p = std::max (p, std::abs (c[i])); return p; };
    auto state = [&] (int k, std::vector<float> q, std::array<Buf, 2> x) { auto u = U::make (k); u->prepare (sr, 512); run (*u, x, 256, [&] (int) { return q.data(); }); std::vector<float> st (192); u->displayState (st.data(), 192); return st; };
    std::printf ("\n== THE MASTERING SIMULATIONS at %.1f kHz ==\n", sr / 1000.0);
    const size_t s1 = (size_t) (0.5 * sr);

    if (const int k = find ("clarity"); k >= 0)
    {
        auto in = sine (200.0, 1.0, 0.3f); const auto pres = sine (3000.0, 1.0, 0.01f); for (int c = 0; c < 2; ++c) for (size_t i = 0; i < in[c].size(); ++i) in[c][i] += pres[c][i];
        const auto y = process (k, with (k, { { 1, 10.0f }, { 3, 0.0f } }), in);
        // the presence lifted: the difference at 3 kHz (correlate with the 3 kHz tone)
        double a = 0, b = 0; for (size_t i = s1; i < y[0].size(); ++i) { const double ref = std::sin (6.283185307 * 3000.0 * (double) i / sr); a += y[0][i] * ref; b += in[0][i] * ref; }
        std::printf ("  CLARITY LENS: a buried 3 kHz detail comes up %+.1f dB\n", db (std::abs (a), std::abs (b)));
        check (db (std::abs (a), std::abs (b)) > 3.0, "CLARITY LENS: a detail the mix buries is brought forward");
    }
    // MULTIPLY and STRENGTH, on every unit that has them (each powered, at its defaults, on music)
    {
        int tested = 0, ok0 = 0, okUp = 0, okMul = 0;
        for (int k = 0; k < U::count; ++k)
        {
            int mul = -1, str = -1;
            for (int i = 0; i < U::info[k].numParams; ++i)
            {
                const auto label = enh::dsp::designed::params[(size_t) (U::info[k].firstParam + i)].label;
                if (label == "MULTIPLY") mul = i;
                if (label == "STRENGTH") str = i;
            }
            if (mul < 0 || str < 0) continue;
            ++tested;
            const auto x = music (sr, 3.0, 11u);
            auto change = [&] (float m, float st)
            {
                auto q = with (k, { { mul, m }, { str, st } });
                const auto y = process (k, q, x);
                double e = 0; for (size_t i = s1; i < y[0].size(); ++i) e += (double) (y[0][i] - x[0][i]) * (y[0][i] - x[0][i]);
                return std::sqrt (e);
            };
            const double base = change (1.0f, 100.0f), none = change (1.0f, 0.0f), more = change (1.0f, 200.0f), less = change (0.25f, 100.0f);
            if (none < 1.0e-6) ++ok0;
            if (base < 1.0e-9 || more > base * 1.3) ++okUp;
            if (base < 1.0e-9 || less <= base * 1.05) ++okMul;
            else std::printf ("  (%s: MULTIPLY 0.25 changed it %.3g against %.3g)\n", U::info[k].name, less, base);
        }
        std::printf ("  MULTIPLY / STRENGTH on %d units: STRENGTH 0 dry %d, 200 more %d, MULTIPLY 0.25 less %d\n", tested, ok0, okUp, okMul);
        check (tested >= 22, "MULTIPLY and STRENGTH: on all of this release's new sound units");
        check (ok0 == tested, "STRENGTH at 0: the sound as it came in, on every unit");
        check (okUp == tested, "STRENGTH at 200 %: more of the unit's change, on every unit");
        check (okMul == tested, "MULTIPLY under 1: less of the unit's change (its amounts scaled down), on every unit");
    }
    if (const int k = find ("detail"); k >= 0)
    {
        // a loud 1 kHz note and, just above it, a quiet 1.4 kHz one the ear can't hear under it (40 dB down)
        auto in = sine (1000.0, 3.0, 0.3f); const auto hidden = sine (1400.0, 3.0, 0.003f);
        for (int c = 0; c < 2; ++c) for (size_t i = 0; i < in[c].size(); ++i) in[c][i] += hidden[c][i];
        const auto y = process (k, with (k, { { 1, 10.0f } }), in);
        const size_t from = (size_t) (2.0 * sr);
        double a = 0, b = 0;
        for (size_t i = from; i < y[0].size(); ++i) { const double ref = std::sin (6.283185307 * 1400.0 * (double) i / sr); a += y[0][i] * ref; b += in[0][i] * ref; }
        const double up = db (std::abs (a), std::abs (b)), level = db (rms (y[0], from, y[0].size()), rms (in[0], from, in[0].size()));
        std::printf ("  SPECTRAL DETAIL ENHANCER: the masked 1.4 kHz note comes up %+.1f dB; the level %+.2f dB\n", up, level);
        check (up > 4.0, "SPECTRAL DETAIL ENHANCER: what the mix masks is brought out");
        check (std::abs (level) < 1.5, "SPECTRAL DETAIL ENHANCER: at the same level (it brings out, it doesn't make louder)");
        const auto z = process (k, with (k, { { 1, 0.0f }, { 3, 0.0f }, { 4, 0.0f } }), in);
        double dmax = 0; for (size_t i = from; i < z[0].size(); ++i) dmax = std::max (dmax, (double) std::abs (z[0][i] - in[0][i]));
        check (dmax < 1.0e-3, "SPECTRAL DETAIL ENHANCER: DETAIL, CLARITY and AIR at 0 leave the sound as it was");
        const auto n = process (k, with (k, { { 1, 10.0f }, { 3, 10.0f }, { 4, 10.0f } }), sine (60.0, 2.0, 0.9f));
        check (peakOf (n, (size_t) sr) < 1.2f, "SPECTRAL DETAIL ENHANCER: all the way up on a loud low note, no runaway");
    }
    if (const int k = find ("subdriver"); k >= 0)
    {
        const auto in50 = sine (55.0, 1.0, 0.2f), in20 = sine (20.0, 1.0, 0.2f);
        const double g50 = db (rms (process (k, with (k, { { 1, 10.0f } }), in50)[0], s1, in50[0].size()), rms (in50[0], s1, in50[0].size()));
        const double g20 = db (rms (process (k, with (k, { { 1, 10.0f } }), in20)[0], s1, in20[0].size()), rms (in20[0], s1, in20[0].size()));
        std::printf ("  SUB DRIVER: 55 Hz %+.1f dB, 20 Hz (under its tuning) %+.1f dB\n", g50, g20);
        check (g50 > 2.0 && g20 < g50 - 2.0, "SUB DRIVER: the sub lifted, and rolled away under the cone's tuning");
    }
    if (const int k = find ("lathe"); k >= 0)
    {
        auto in = sine (60.0, 1.0, 0.3f); for (auto& v : in[1]) v = -v;   // (all side, all bass)
        const auto y = process (k, with (k, {}), in);
        double side = 0, sIn = 0; for (size_t i = s1; i < y[0].size(); ++i) { side += std::pow (0.5 * (y[0][i] - y[1][i]), 2.0); sIn += std::pow (0.5 * (in[0][i] - in[1][i]), 2.0); }
        std::printf ("  VINYL CUTTER: side-to-side bass at 60 Hz %+.1f dB\n", 10.0 * std::log10 (side / sIn));
        check (10.0 * std::log10 (side / sIn) < -12.0, "VINYL CUTTER: the bass cut mono");
    }
    if (const int k = find ("cartest"); k >= 0)
    {
        const auto st = state (k, with (k, { { 1, 10.0f } }), sine (60.0, 1.0, 0.3f));
        std::printf ("  CAR TEST: the cabin's boom at 60 Hz %.2f\n", st[2]);
        check (st[2] > 0.3f, "CAR TEST: the cabin booms with the bass");
    }
    if (const int k = find ("phonecheck"); k >= 0)
    {
        const auto in = sine (100.0, 1.0, 0.2f);
        const double g = db (rms (process (k, with (k, { { 2, 0.0f } }), in)[0], s1, in[0].size()), rms (in[0], s1, in[0].size()));
        std::printf ("  PHONE CHECK: 100 Hz on the phone %+.1f dB\n", g);
        check (g < -20.0, "PHONE CHECK: a phone has no bass");
    }
    if (const int k = find ("club"); k >= 0)
    {
        auto click = sine (1000.0, 1.0, 0.0f); for (size_t i = 0; i < 200; ++i) click[0][i + 100] = click[1][i + 100] = 0.5f * (float) std::sin (0.3 * (double) i);
        const auto y = process (k, with (k, {}), click);
        const double tail = rms (y[0], (size_t) (0.15 * sr), (size_t) (0.4 * sr));
        std::printf ("  CLUB SYSTEM: the room after a click %.3g\n", tail);
        check (tail > 1.0e-4, "CLUB SYSTEM: the room rings on after the sound");
    }
    for (const char* key : { "pressure", "hourglass" })
        if (const int k = find (key); k >= 0)
        {
            auto in = music (sr, 1.5); for (auto& c : in) for (auto& v : c) v *= 3.0f;
            const bool pv = std::string (key) == "pressure";
            const float ceil = std::pow (10.0f, (pv ? -1.0f : -0.3f) / 20.0f);
            const float pk = peakOf (process (k, with (k, pv ? std::initializer_list<std::pair<int, float>> { { 1, 24.0f }, { 2, -1.0f } } : std::initializer_list<std::pair<int, float>> { { 1, -0.3f }, { 2, 12.0f } }), in), (size_t) (0.1 * sr));   // (after POWER's fade-in, when dry and wet are blended)
            std::printf ("  %s: driven hard, peaks %.4f (ceiling %.4f)\n", pv ? "PRESSURE" : "HOURGLASS", pk, ceil);
            check (pk <= ceil * 1.0001f, pv ? "PRESSURE: nothing passes the valve" : "HOURGLASS: nothing passes the ceiling");
        }
    if (const int k = find ("balance"); k >= 0)
    {
        const auto st = state (k, with (k, { { 2, 10.0f }, { 3, 10.0f } }), sine (120.0, 4.0, 0.3f));   // (all bottom, no top)
        std::printf ("  BALANCE: a bass-only sound tips it %.2f, corrected %+.2f (of 6 dB)\n", st[0], st[1]);
        check (st[0] < -0.5f && st[1] > 0.3f, "BALANCE: too much low end is levelled by lifting the top");
    }
    if (const int k = find ("field"); k >= 0)
    {
        auto in = music (sr, 1.0);
        const auto y = process (k, with (k, { { 1, 0.0f } }), in);
        double d = 0; for (size_t i = s1; i < y[0].size(); ++i) d += std::abs (y[0][i] - y[1][i]);
        std::printf ("  STEREO FIELD: WIDTH 0 leaves %.3g between left and right\n", d);
        check (d < 1.0e-3, "STEREO FIELD: WIDTH 0 is mono");
    }
    if (const int k = find ("sonar"); k >= 0)
    {
        auto in = sine (1.0, 2.0, 0.0f); for (int hit = 0; hit < 8; ++hit) for (size_t i = 0; i < (size_t) (0.2 * sr); ++i) { const size_t at = (size_t) (0.1 * sr) + (size_t) hit * (size_t) (0.24 * sr) + i; if (at < in[0].size()) in[0][at] = in[1][at] = 0.3f * (float) std::exp (-(double) i / (0.05 * sr)) * (float) std::sin (0.2 * (double) i); }
        // each hit's first 8 ms against its 60 - 150 ms (hits 2 on, after POWER's fade-in)
        auto front = [&] (const std::array<Buf, 2>& x) { double a = 0, b = 0; for (int hit = 2; hit < 8; ++hit) { const size_t at = (size_t) (0.1 * sr) + (size_t) hit * (size_t) (0.24 * sr);
                                                          a += rms (x[0], at, at + (size_t) (0.008 * sr)); b += rms (x[0], at + (size_t) (0.06 * sr), at + (size_t) (0.15 * sr)); } return db (a, b); };
        const double up = front (process (k, with (k, { { 1, 10.0f }, { 2, 0.0f } }), in)), down = front (process (k, with (k, { { 1, -10.0f }, { 2, 0.0f } }), in)), dry = front (in);
        std::printf ("  SONAR: each hit's start over its tail: dry %.1f dB, ATTACK +10 %.1f dB, -10 %.1f dB\n", dry, up, down);
        check (up > dry + 2.0 && down < dry - 2.0, "SONAR: ATTACK sharpens or softens the hits");
    }
    if (const int k = find ("seismo"); k >= 0)
    {
        const auto lo = sine (60.0, 1.0, 0.5f), hi = sine (2000.0, 1.0, 0.5f);
        const double gl = db (rms (process (k, with (k, { { 1, -30.0f }, { 2, 10.0f } }), lo)[0], s1, lo[0].size()), rms (lo[0], s1, lo[0].size()));
        const double gh = db (rms (process (k, with (k, { { 1, -30.0f }, { 2, 10.0f } }), hi)[0], s1, hi[0].size()), rms (hi[0], s1, hi[0].size()));
        std::printf ("  SEISMOGRAPH: a loud 60 Hz %+.1f dB, 2 kHz %+.1f dB\n", gl, gh);
        check (gl < -6.0 && std::abs (gh) < 1.0, "SEISMOGRAPH: the bass quake damped, the rest untouched");
    }
    if (const int k = find ("furnace"); k >= 0)
    {
        const auto st = state (k, with (k, { { 1, 10.0f }, { 2, 0.0f } }), sine (200.0, 3.0, 0.5f));
        std::printf ("  FURNACE: after 3 s of a loud tone it runs at %.2f\n", st[0]);
        check (st[0] > 0.2f, "FURNACE: it heats up as it is played");
    }
    if (const int k = find ("dither"); k >= 0)
    {
        const auto y = process (k, with (k, { { 1, 0.0f }, { 2, 1.0f } }), music (sr, 0.5));
        bool onSteps = true; for (auto& c : y) for (size_t i = (size_t) (0.1 * sr); i < c.size(); ++i) { const float v = c[i]; const double q = (double) v * 32768.0; onSteps = onSteps && std::abs (q - std::round (q)) < 1.0e-3; }
        std::printf ("  DITHER: 16 bits, every sample on a step %s\n", onSteps ? "yes" : "NO");
        check (onSteps, "DITHER: its output is 16-bit");
    }
    if (const int k = find ("rider"); k >= 0)
    {
        const auto st = state (k, with (k, { { 1, -14.0f }, { 2, 10.0f }, { 3, 6.0f } }), sine (500.0, 5.0, 0.02f));   // (about -37 LUFS)
        std::printf ("  RIDER: a quiet track, fader at %+.2f of its range\n", st[0]);
        check (st[0] > 0.95f, "RIDER: it rides a quiet track up, as far as RANGE allows");
    }
    if (const int k = find ("compass"); k >= 0)
    {
        auto in = sine (120.0, 3.0, 0.3f); const int lag = (int) (0.0006 * sr);
        for (size_t i = in[1].size() - 1; i >= (size_t) lag; --i) in[1][i] = in[0][i - (size_t) lag];   // (R late by 0.6 ms)
        auto corr = [&] (const std::array<Buf, 2>& x) { double lr = 0, ll = 0, rr = 0; for (size_t i = (size_t) (2.0 * sr); i < x[0].size(); ++i) { lr += x[0][i] * x[1][i]; ll += x[0][i] * x[0][i]; rr += x[1][i] * x[1][i]; } return lr / std::sqrt (ll * rr + 1e-12); };
        const double before = corr (in), after = corr (process (k, with (k, { { 1, 10.0f }, { 2, 1.0f }, { 3, 0.0f } }), in));
        std::printf ("  COMPASS: left against right before %.4f, after %.4f\n", before, after);
        check (after > before && after > 0.999, "COMPASS: a late channel brought back into line");
    }
    if (const int k = find ("suspension"); k >= 0)
    {
        auto in = sine (300.0, 2.0, 0.05f); for (size_t i = (size_t) sr; i < in[0].size(); ++i) { in[0][i] *= 8.0f; in[1][i] *= 8.0f; }   // (a bump: +18 dB)
        const auto y = process (k, with (k, {}), in);
        const double jump = db (rms (y[0], (size_t) sr, (size_t) (1.05 * sr)), rms (y[0], (size_t) (0.9 * sr), (size_t) sr));
        std::printf ("  SUSPENSION: an 18 dB bump comes through as %+.1f dB\n", jump);
        check (jump < 16.0, "SUSPENSION: a bump in level is soaked up");
    }
    if (const int k = find ("skyline"); k >= 0)
    {
        auto in = music (sr, 2.0); const auto tone = sine (1000.0, 2.0, 0.3f); for (int c = 0; c < 2; ++c) for (size_t i = 0; i < in[c].size(); ++i) in[c][i] += tone[c][i];
        const auto y = process (k, with (k, { { 1, 10.0f } }), in);
        double a = 0, b = 0; for (size_t i = (size_t) sr; i < y[0].size(); ++i) { const double ref = std::sin (6.283185307 * 1000.0 * (double) i / sr); a += y[0][i] * ref; b += in[0][i] * ref; }
        std::printf ("  SKYLINE: a ringing 1 kHz tower trimmed %+.1f dB\n", db (std::abs (a), std::abs (b)));
        check (db (std::abs (a), std::abs (b)) < -2.0, "SKYLINE: a resonance standing over its neighbours is trimmed");
    }
    if (const int k = find ("aurora"); k >= 0)
    {
        const auto in = sine (4000.0, 1.0, 0.2f);
        const auto y = process (k, with (k, { { 1, 10.0f }, { 3, 0.0f } }), in);
        double hf = 0; std::array<float, 2> h {}; const float kk = 1.0f - (float) std::exp (-6.283185307 * 10000.0 / sr);
        for (size_t i = s1; i < y[0].size(); ++i) { h[0] += kk * (y[0][i] - in[0][i] - h[0]); const double v = (y[0][i] - in[0][i]) - h[0]; hf += v * v; }
        std::printf ("  AURORA: air added above 10 kHz %.3g\n", std::sqrt (hf / (double) (y[0].size() - s1)));
        check (hf > 1.0e-6 * (double) (y[0].size() - s1), "AURORA: it adds air over the top");
    }
}

// CHROMA SPACE (units/Sims4.h): its space from subtle to vast, and its chops - captured only when asked, never busy
static void runChromaTests (double sr)
{
    using namespace newunitstest;
    namespace U = enh::dsp::units;
    int k = -1; for (int i = 0; i < U::count; ++i) if (std::string (U::info[i].key) == "chroma") k = i;
    std::printf ("\n== CHROMA SPACE at %.1f kHz ==\n", sr / 1000.0);
    if (k < 0) { check (false, "CHROMA SPACE is in the unit list"); return; }
    auto with = [&] (std::initializer_list<std::pair<int, float>> set) { auto q = defaults (k); q[0] = 1.0f; for (auto [i, v] : set) q[(size_t) i] = v; return q; };
    auto rmsDbOf = [] (const Buf& b, size_t from, size_t to) { double e = 0; to = std::min (to, b.size()); for (size_t i = from; i < to; ++i) e += (double) b[i] * b[i]; return 10.0 * std::log10 (e / (double) std::max<size_t> (1, to - from) + 1e-20); };
    // the tail 2 - 3 s after a burst: SPACE 3 (a room: gone) against SPACE 10 (a vast hall: still ringing)
    auto tail = [&] (float space)
    {
        auto u = U::make (k); u->prepare (sr, 256); auto q = with ({ { 1, space }, { 8, 100.0f } });
        std::array<Buf, 2> x { Buf ((size_t) (3.5 * sr)), Buf ((size_t) (3.5 * sr)) };
        juce::Random rnd (3); for (size_t i = 0; i < (size_t) (0.05 * sr); ++i) x[0][i] = x[1][i] = 0.5f * (rnd.nextFloat() - 0.5f);
        run (*u, x, 256, [&] (int) { return q.data(); });
        return rmsDbOf (x[0], (size_t) (0.5 * sr), (size_t) (1.0 * sr)) - rmsDbOf (x[0], (size_t) (2.5 * sr), (size_t) (3.0 * sr));   // (how far it fell in 2 s)
    };
    if (std::getenv ("CHROMA_SWEEP") != nullptr) for (float sp : { 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f }) std::printf ("    SPACE %.0f: falls %.1f dB in 2 s\n", sp, tail (sp));
    const double room = tail (3.0f), hall = tail (10.0f);
    std::printf ("  the tail's fall from 0.5 - 1 s to 2.5 - 3 s after a burst: SPACE 3 %.1f dB, SPACE 10 %.1f dB\n", room, hall);
    check (room > 45.0 && hall < 15.0 && hall > 3.0, "CHROMA SPACE: a room at 3 (gone within seconds), a vast tail at 10 (still falling, slowly - never growing)");
    // chops: none without CHOP; with it, captured (in memory) and played - but never more often than they may be
    auto chops = [&] (float chop, int& played, float& banked)
    {
        auto u = U::make (k); u->prepare (sr, 256); auto q = with ({ { 1, 0.0f }, { 4, chop }, { 8, 0.0f } });
        auto x = music (sr, 6.0, 9u); const auto dry = x;
        std::vector<float> st (192); float lastFlashSum = 0.0f; played = 0;
        run (*u, x, 256, [&] (int) { u->displayState (st.data(), 192); float f = 0.0f; for (int c = 0; c < (int) st[145]; ++c) f += st[(size_t) (149 + 4 * c)] > 0.95f ? 1.0f : 0.0f; if (f > lastFlashSum) ++played; lastFlashSum = f; banked = st[145]; return q.data(); });
        double d = 0; for (size_t i = 0; i < x[0].size(); ++i) d += std::abs (x[0][i] - dry[0][i]); return d;
    };
    int p0 = 0, p10 = 0; float b0 = 0, b10 = 0;
    const double d0 = chops (0.0f, p0, b0), d10 = chops (10.0f, p10, b10);
    std::printf ("  CHOP 0: %.0f chops kept, %d played; CHOP 10: %.0f kept, %d played in 6 s (added %.3g against %.3g)\n", b0, p0, b10, p10, d10, d0);
    check (b0 == 0.0f && p0 == 0 && b10 > 0.0f && p10 > 0 && d10 > d0, "CHROMA SPACE: chops only when CHOP is up - captured and played then");
    check (p10 <= 6 * 6, "CHROMA SPACE: never busy - at most a few chops a second");
}

// The LUNCHBOX's 500-series modules: the same checks as the rack's newer units, and each one's own
static void runLbModuleTests (double sr)
{
    using namespace newunitstest;
    namespace U = enh::dsp::units;
    namespace L = enh::dsp::lbmods;
    std::printf ("\n== THE LUNCHBOX MODULES (%d) at %.1f kHz ==\n", L::count, sr / 1000.0);
    auto defs = [] (int k)
    {
        std::vector<float> p ((size_t) L::info[k].numParams);
        for (int i = 0; i < L::info[k].numParams; ++i) p[(size_t) i] = enh::dsp::designed::params[(size_t) (L::info[k].firstParam + i)].defaultValue;
        return p;
    };
    int bad = 0;
    for (int k = 0; k < L::count; ++k)
    {
        const auto& in = L::info[k];
        { auto x = music (sr, 0.5), ref = x; auto u = U::lb::make (k); u->prepare (sr, 512); auto q = defs (k); q[0] = 0.0f;
          run (*u, x, 256, [&] (int) { return q.data(); });
          if (x != ref) { ++bad; std::printf ("  [FAIL] %s: OUT is not bit-for-bit\n", in.name); } }
        unsigned s = 777u + (unsigned) k; bool finite = true; float peak = 0.0f; double changed = 0.0;
        for (int trial = 0; trial <= 20; ++trial)
        {
            auto q = defs (k); q[0] = 1.0f;
            if (trial > 0)
                for (int i = 1; i < in.numParams; ++i)
                {
                    const auto& d = enh::dsp::designed::params[(size_t) (in.firstParam + i)];
                    s = s * 1664525u + 1013904223u;
                    q[(size_t) i] = d.minValue + (d.maxValue - d.minValue) * ((float) (s >> 8) / 16777216.0f);
                    if (d.kind != 0) q[(size_t) i] = std::round (q[(size_t) i]);
                }
            auto x = music (sr, 1.0, 5u + (unsigned) trial), ref = x; auto u = U::lb::make (k); u->prepare (sr, 512);
            run (*u, x, 64 + 64 * (trial % 4), [&] (int) { return q.data(); });
            for (int c = 0; c < 2; ++c) for (size_t i = 0; i < x[c].size(); ++i)
            {
                if (! std::isfinite (x[c][i])) finite = false;
                peak = std::max (peak, std::abs (x[c][i]));
                if (trial > 0) changed += std::abs ((double) x[c][i] - ref[c][i]);
            }
        }
        { auto x = music (sr, 1.5), ref = x; auto u = U::lb::make (k); u->prepare (sr, 512); auto q = defs (k);
          run (*u, x, 256, [&] (int pos) { q[0] = pos >= (int) (0.5 * sr) && pos < (int) sr ? 1.0f : 0.0f; return q.data(); });
          float jump = 0.0f; for (int i = 1; i < (int) x[0].size(); ++i) jump = std::max (jump, std::abs ((x[0][(size_t) i] - x[0][(size_t) i - 1]) - (ref[0][(size_t) i] - ref[0][(size_t) i - 1])));
          if (jump > 0.25f) { ++bad; std::printf ("  [FAIL] %s: IN clicks (%.3f)\n", in.name, jump); } }
        const bool ok = finite && peak <= 2.0f && changed > 1.0e-3;
        if (! ok) ++bad;
        std::printf ("  [%s] %-13s finite %s, peak %.2f, changes the sound %s\n", ok ? "PASS" : "FAIL", in.name, finite ? "yes" : "NO", peak, changed > 1.0e-3 ? "yes" : "NO");
    }
    check (bad == 0, "every LUNCHBOX module: OUT bit-for-bit, finite, under the ceiling, click-free, and doing something");

    auto idx = [&] (const char* key) { for (int k = 0; k < L::count; ++k) if (std::string (L::info[k].key) == key) return k; return -1; };
    auto runK = [&] (int k, std::vector<float> q, std::array<Buf, 2> x) { auto u = U::lb::make (k); u->prepare (sr, 512); q[0] = 1.0f; run (*u, x, 256, [&] (int) { return q.data(); }); return x; };
    auto sine = [&] (float amp, double hz, double secs) { std::array<Buf, 2> x { Buf ((size_t) (sr * secs)), Buf ((size_t) (sr * secs)) };
        for (size_t i = 0; i < x[0].size(); ++i) x[0][i] = x[1][i] = amp * (float) std::sin (6.283185307 * hz * (double) i / sr); return x; };
    auto peakOf = [] (const std::array<Buf, 2>& x, size_t from) { float pk = 0; for (int c = 0; c < 2; ++c) for (size_t i = from; i < x[c].size(); ++i) pk = std::max (pk, std::abs (x[c][i])); return pk; };
    // (after IN's 30 ms fade)
    { const int k = idx ("limit"); auto q = defs (k); q[1] = -6.0f; q[2] = 12.0f; auto y = runK (k, q, music (sr, 1.0));
      const float pk = peakOf (y, (size_t) (0.05 * sr));
      std::printf ("  PEAK LIMITER: ceiling -6 dB, drive +12: peak %.2f dBFS\n", 20.0 * std::log10 (pk));
      check (pk <= std::pow (10.0f, -6.0f / 20.0f) * 1.001f, "PEAK LIMITER: never over its CEILING"); }
    { const int k = idx ("eq550"); auto x = music (sr, 0.5); auto y = runK (k, defs (k), x);
      double d = 0; for (int c = 0; c < 2; ++c) for (size_t i = (size_t) (0.05 * sr); i < x[c].size(); ++i) d = std::max (d, (double) std::abs (y[c][i] - x[c][i]));
      check (d < 1.0e-6, "550 EQ: every band at 0 dB is exactly the input"); }
    { const int k = idx ("width"); auto x = music (sr, 0.5); auto y = runK (k, defs (k), x);
      double d = 0; for (int c = 0; c < 2; ++c) for (size_t i = (size_t) (0.05 * sr); i < x[c].size(); ++i) d = std::max (d, (double) std::abs (y[c][i] - x[c][i]));
      check (d < 1.0e-5, "M/S WIDTH: at 100 % (and no bass mono) it is the input"); }
    { const int k = idx ("comp"); auto q = defs (k); q[1] = -20.0f; q[2] = 2.0f; auto y = runK (k, q, sine (0.5f, 1000.0, 1.0));
      const float pk = peakOf (y, (size_t) (0.5 * sr)); const float grDb = -20.0f * std::log10 (pk / 0.5f);
      std::printf ("  BUS COMP: -6 dBFS sine, thresh -20, 4:1: %.1f dB of gain reduction\n", grDb);
      check (grDb > 7.0f && grDb < 12.0f, "BUS COMP: a steady tone 14 dB over the threshold at 4:1 is turned down ~10 dB"); }
    { const int k = idx ("tubeeq"); auto q = defs (k); q[2] = 10.0f; auto y = runK (k, q, sine (0.1f, 60.0, 1.0));
      const float g = 20.0f * std::log10 (peakOf (y, (size_t) (0.5 * sr)) / 0.1f);
      std::printf ("  TUBE EQ: LOW BOOST 10 at 60 Hz: %+.1f dB\n", g);
      check (g > 9.0f && g < 16.0f, "TUBE EQ: a full LOW BOOST lifts its frequency by about 12 dB"); }
    { const int k = idx ("deess"); auto q = defs (k); auto lo = runK (k, q, sine (0.3f, 500.0, 0.6)), hi = runK (k, q, sine (0.3f, 8000.0, 0.6));
      const float gLo = 20.0f * std::log10 (peakOf (lo, (size_t) (0.3 * sr)) / 0.3f), gHi = 20.0f * std::log10 (peakOf (hi, (size_t) (0.3 * sr)) / 0.3f);
      std::printf ("  DE-ESSER: a loud 500 Hz tone %+.1f dB, a loud 8 kHz one %+.1f dB\n", gLo, gHi);
      check (std::abs (gLo) < 0.5f && gHi < -4.0f, "DE-ESSER: sibilance turned down, the body of the sound untouched"); }
}

// DE-HARSH: presence flicker (a 4 kHz tone whose level an "adaptive unit" jumps 8 dB 20 times a second) is
// evened out; a tone standing far above the mids is taken down; a balanced steady mix is left alone
static void runDeHarshTests (double sr)
{
    using namespace newunitstest;
    namespace U = enh::dsp::units;
    int k = -1; for (int i = 0; i < U::count; ++i) if (std::string (U::info[i].key) == "deharsh") k = i;
    std::printf ("\n== DE-HARSH at %.1f kHz ==\n", sr / 1000.0);
    if (k < 0) { check (false, "DE-HARSH is in the unit list"); return; }
    auto make = [&] (float midAmp, float hiAmp, bool flicker)
    {
        const int n = (int) (sr * 2.0); std::array<Buf, 2> x { Buf ((size_t) n), Buf ((size_t) n) };
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const float wob = flicker ? (std::fmod (t * 20.0, 1.0) < 0.5 ? 1.0f : 0.4f) : 1.0f;
            const float v = midAmp * (float) std::sin (6.2831853 * 900.0 * t) + hiAmp * wob * (float) std::sin (6.2831853 * 3500.0 * t);
            x[0][(size_t) i] = x[1][(size_t) i] = v;
        }
        return x;
    };
    // the 3.5 kHz part's level in 5 ms windows over the last second: how much it moves (dB, standard deviation) and its mean
    auto hiStats = [&] (const Buf& b, double& spread, double& mean)
    {
        BiquadState st; const auto c = BiquadCoeffs::bandPass (sr, 3500.0, 4.0); Buf y (b.size());
        for (size_t i = 0; i < b.size(); ++i) y[i] = st.process (c, b[i]);
        const int w = (int) (0.005 * sr); double sq = 0, sum = 0; int cnt = 0;
        for (int s = (int) b.size() / 2; s + w <= (int) b.size(); s += w)
        {
            double e = 0; for (int i = 0; i < w; ++i) e += (double) y[(size_t) (s + i)] * y[(size_t) (s + i)];
            const double db = 10.0 * std::log10 (e / w + 1e-12); sq += db * db; sum += db; ++cnt;
        }
        mean = sum / cnt; spread = std::sqrt (std::max (0.0, sq / cnt - mean * mean));
    };
    auto runIt = [&] (std::array<Buf, 2> x) { auto u = U::make (k); u->prepare (sr, 512); auto q = defaults (k); q[0] = 1.0f; run (*u, x, 256, [&] (int) { return q.data(); }); return x; };
    double s0, m0, s1, m1;
    auto fl = make (0.2f, 0.12f, true); hiStats (fl[0], s0, m0); hiStats (runIt (fl)[0], s1, m1);
    std::printf ("  flicker: 3.5 kHz level moves %.2f dB -> %.2f dB (std. dev.)\n", s0, s1);
    check (s1 < 0.6 * s0, "DE-HARSH: fast adaptive flicker in the presence region is evened out (its movement down 40% and more)");
    auto hs = make (0.05f, 0.4f, false); hiStats (hs[0], s0, m0); hiStats (runIt (hs)[0], s1, m1);
    std::printf ("  harsh: 3.5 kHz %.1f dB -> %.1f dB (18 dB over the mids)\n", m0, m1);
    check (m1 <= m0 - 3.0 && m1 >= m0 - 4.6, "DE-HARSH: a harsh top standing out is taken down, by no more than DEPTH");
    auto ok = make (0.2f, 0.08f, false); hiStats (ok[0], s0, m0); hiStats (runIt (ok)[0], s1, m1);
    std::printf ("  balanced: 3.5 kHz %.1f dB -> %.1f dB\n", m0, m1);
    check (std::abs (m1 - m0) < 0.5, "DE-HARSH: a balanced, steady mix is left alone (within 0.5 dB)");
}

// CUSTOM: a share code (made here the way the designer makes one: a drive, a compressor and a room, three
// knobs and a switch wired to them) decoded, and its chain run
#include "Custom/DesignCode.h"
static void runCustomUnitTests (double sr)
{
    using namespace newunitstest;
    std::printf ("\n== CUSTOM (a design from its share code) at %.1f kHz ==\n", sr / 1000.0);
    const auto d = pad::custom::decode ("ENH2-1lX7-RWvj-WQ77-32NJ-yD7F-WrSU-UcZ5-pHlE-doJV-ALFp-5DVB-kNnV-QaXa-oDuo-h7hK-IGFq-4Ybp-l31n-2KyZ-wCrQ-bct9-mLQG-PP19-6oHm-Wl4T-U4Aj-g0ik-B0GS-PxpH-5VIP-W0yN-hQd9-P0C2-xarL-g1qU-c4o7-i9DO-fnU1-Nhux-0Dlk-e0Gb-VmFf-LijG-THBx-HONp-nduZ-PnvF-2ojK-Pdke-k02r-ZtwT-3u9s-dkap-O2Nc-Hi1i-2hsw-Xmhv-qkoL-45p3-cT8n-3wi2-bwOm-LSJY-4rep-JNXX-o01O-grYm-BXJs-SUFQ-DATA-4Tcx-QPod-6tvg-eHEE-2lDU-NTfX-Egou-HkEC-rZ1b-mRF2-sqwS-vQmx-jcHD-un7y-8Uik-3vLV-QgZO-a3Ku-crBh-eXtO-sDuA-zfRG-tlPV-Bx7h-ZjnK-2zgd-6ots-z7JJ-Pbzi-VbaY-6t7B-UhNo-fPKe-LwSX-GoZq-rcAf-ih6c-9YN9-G0sv-1VTt-jUV");
    check (d.ok && d.name == "TEST WARMER" && d.config.count == 3 && d.slots[0].used && d.slots[16].used
           && d.config.wires[0].block == 0 && d.config.wires[1].block == 1 && d.config.wires[16].block == 1 && d.config.wires[16].param == -1,
           "a share code decodes: its name, its chain, its knobs and switch wired as designed");
    check (! pad::custom::decode ("ENH2-<script>").ok && ! pad::custom::decode ("hello").ok && ! pad::custom::decode ("ENH2-" + juce::String::repeatedString ("A", 30000)).ok,
           "anything that isn't a design code is refused");
    enh::dsp::units::CustomUnit u; u.prepare (sr, 512); u.setConfig (&d.config);
    std::array<float, 21> p {}; p[0] = 1.0f; for (int i = 1; i <= 16; ++i) p[(size_t) i] = 50.0f; for (int i = 17; i <= 20; ++i) p[(size_t) i] = 1.0f;
    auto render = [&] (float drive)
    {
        auto x = music (sr, 1.0); enh::dsp::units::CustomUnit v; v.prepare (sr, 512); v.setConfig (&d.config);
        p[1] = drive;
        run (v, x, 256, [&] (int) { return p.data(); });
        return x;
    };
    const auto a = render (0.0f), b = render (100.0f), dry = music (sr, 1.0);
    double diffAB = 0, diffDry = 0; bool fin = true;
    for (size_t i = 0; i < a[0].size(); ++i) { diffAB += std::pow (a[0][i] - b[0][i], 2); diffDry += std::pow (a[0][i] - dry[0][i], 2); fin = fin && std::isfinite (b[0][i]); }
    const double low = fin ? std::sqrt (diffDry / a[0].size()) : -1.0, high = std::sqrt (diffAB / a[0].size());
    std::printf ("  the design against the dry sound %.4f; DRIVE 0 against DRIVE 100 %.4f\n", low, high);
    check (low > 0.001 && high > 0.003, "the design's sound runs, and its DRIVE knob drives it");

    // The moving blocks (chorus, pendulum pan, lava-lamp filter, chops, bit crusher, tape wow, shimmer): each at
    // its defaults changes the sound, stays finite, and stays in bounds
    using CC = enh::dsp::units::CustomConfig;
    static const std::array<std::pair<CC::Block, std::array<float, 7>>, 7> moving {{
        { CC::chorus, { 0.8f, 45, 45 } }, { CC::pan, { 0.5f, 60, 0 } }, { CC::wander, { 1200, 50, 0.15f, 2 } }, { CC::stutter, { 4, 70, 40 } },
        { CC::crush, { 6, 50 } }, { CC::wow, { 35, 25 } }, { CC::shimmer, { 4, 50, 30 } } }};
    static std::array<CC, 7> cfgs {};
    bool allOk = true;
    for (size_t k = 0; k < moving.size(); ++k)
    {
        auto& c = cfgs[k]; c = {}; c.count = 1; c.blocks[0].type = moving[k].first; c.blocks[0].p = moving[k].second;
        enh::dsp::units::CustomUnit v; v.prepare (sr, 512); v.setConfig (&c);
        auto x = music (sr, 2.0); const auto dry2 = music (sr, 2.0);
        std::array<float, 21> pp {}; pp[0] = 1.0f;
        run (v, x, 256, [&] (int) { return pp.data(); });
        double diff = 0, peak = 0, peakDry = 0; bool fin = true;
        for (int ch = 0; ch < 2; ++ch) for (size_t i = 0; i < x[(size_t) ch].size(); ++i)
        { fin = fin && std::isfinite (x[(size_t) ch][i]); diff += std::pow (x[(size_t) ch][i] - dry2[(size_t) ch][i], 2); peak = std::max (peak, (double) std::abs (x[(size_t) ch][i])); peakDry = std::max (peakDry, (double) std::abs (dry2[(size_t) ch][i])); }
        const double rms = std::sqrt (diff / (2.0 * x[0].size()));
        const bool ok = fin && rms > 0.002 && peak < peakDry * 2.0 + 0.05;
        std::printf ("  block %d: change %.4f, peak %.3f (dry %.3f)%s\n", (int) moving[k].first, rms, peak, peakDry, ok ? "" : "  <- WRONG");
        allOk = allOk && ok;
    }
    check (allOk, "the seven moving blocks each change the sound, stay finite, and stay under twice the dry peak");
}

// RACK TUNER's level match: a tune makes the rack 6 dB louder - within its 8 s window the trim takes it back to
// how loud it was; A/B back (rematch) and the trim follows; off, it eases to nothing; silence changes nothing
#include "DSP/TunerMatch.h"
static void runTunerMatchTests (double sr)
{
    using namespace newunitstest;
    std::printf ("\n== RACK TUNER level match at %.1f kHz ==\n", sr / 1000.0);
    enh::dsp::TunerMatch tm; tm.prepare (sr);
    const int block = 256;
    std::vector<float> inL ((size_t) block), inR ((size_t) block), outL ((size_t) block), outR ((size_t) block);
    unsigned seed = 7;
    auto noise = [&] { seed = seed * 1664525u + 1013904223u; return (float) ((int) (seed >> 9) - (1 << 22)) / (float) (1 << 22); };
    int capture = 0, rematch = 0;
    auto run = [&] (double seconds, float rackGain, bool enabled, float level = 0.1f)
    {
        double sumIn = 0, sumOut = 0; long cnt = 0;
        const int blocks = (int) (seconds * sr / block);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < block; ++i) { inL[(size_t) i] = level * noise(); inR[(size_t) i] = level * noise(); outL[(size_t) i] = rackGain * inL[(size_t) i]; outR[(size_t) i] = rackGain * inR[(size_t) i]; }
            const float* pre[2] { inL.data(), inR.data() };
            float* io[2] { outL.data(), outR.data() };
            tm.process (io, 2, block, pre, enabled, capture, rematch);
            if (b > blocks * 3 / 4) for (int i = 0; i < block; ++i) { sumIn += inL[(size_t) i] * inL[(size_t) i]; sumOut += outL[(size_t) i] * outL[(size_t) i]; ++cnt; }
        }
        return 10.0 * std::log10 ((sumOut + 1e-20) / (sumIn + 1e-20));   // the rack's gain at the end, trim included
    };
    run (4.0, 1.0f, true);                   // learns the rack as it is (0 dB)
    ++capture;                               // TUNE: remember "as loud as now" ...
    const double tuned = run (8.0, 2.0f, true);   // ... and the tune made it 6 dB louder
    std::printf ("  after a +6 dB tune: the rack %+.2f dB (trim %+.2f dB)\n", tuned, tm.getTrimDb());
    check (std::abs (tuned) < 0.8, "a tune's +6 dB is taken back to within 0.8 dB");
    ++rematch;                               // A/B: back to before (the rack at 0 dB again)
    const double back = run (8.0, 1.0f, true);
    std::printf ("  A/B back to before: %+.2f dB (trim %+.2f dB)\n", back, tm.getTrimDb());
    check (std::abs (back) < 0.8, "after A/B the trim follows: still as loud as before the tune");
    const float trimBefore = tm.getTrimDb();
    run (4.0, 1.0f, true, 0.0f);            // silence: nothing learnt, the trim holds
    check (std::abs (tm.getTrimDb() - trimBefore) < 0.2f, "silence changes nothing");
    run (6.0, 2.0f, false);
    std::printf ("  off: trim %+.2f dB\n", tm.getTrimDb());
    check (std::abs (tm.getTrimDb()) < 0.3f, "LEVEL MATCH off: the trim eases to nothing");
}

#include "DSP/PatchBay.h"
#include "DSP/EnhEngine.h"

/*  THE PATCH BAY: what the cords make of the signal (DSP/PatchBay.h), and the engine's side of it - a cord out of
    the chain fades the rack to silence and back without a jump, and the half-in crackle stays well down. */
static void runPatchBayTests (double sr)
{
    using namespace newunitstest;
    namespace pb = enh::patch;
    std::printf ("\n== THE PATCH BAY at %.1f kHz ==\n", sr / 1000.0);

    const std::vector<int> rack { 5, 0, 3, 8, 17, 18, 19 };
    auto s = pb::straightThrough (rack);
    auto l = pb::follow (s, 0), r = pb::follow (s, 1);
    check (l.complete && r.complete && l.units == rack && r.units == rack, "straight through: both sides reach RACK OUT through every unit in order");

    // Swap two of the newer units (18 before 17): RACK..8 -> 18 -> 17 -> 19
    auto sw = s;
    for (auto& c : sw.cords)
        for (auto* e : { &c.a, &c.b })
        {
            if (e->unit == 17) e->unit = 18; else if (e->unit == 18) e->unit = 17;
        }
    l = pb::follow (sw, 0);
    check (l.complete && l.units == std::vector<int> ({ 5, 0, 3, 8, 18, 17, 19 }), "re-patched: the path follows the cords (18 before 17)");

    auto pulled = s;
    pulled.cords[2].b = {};   // one end out (cords go L, R per link: [2] is the second link's left)
    check (! pb::follow (pulled, 0).complete && pb::follow (pulled, 1).complete, "a pulled plug breaks only its own side");

    auto spare = s;
    spare.cords[2].b = { (int16_t) (pb::spareBase - 40), pb::inL };
    check (! pb::follow (spare, 0).complete, "into a spare jack: the signal goes nowhere");

    auto loop = s;   // unit 3's OUT L back into unit 0's IN L (0's own feed pulled)
    for (auto& c : loop.cords)
        if (c.a.unit == 3 && c.a.port == pb::outL) c.b = { 0, pb::inL };
        else if (c.b.unit == 0 && c.b.port == pb::inL) c.b = {};
    check (pb::follow (loop, 0).loop || ! pb::follow (loop, 0).complete, "a cord back into the chain: a loop, never 'complete'");

    auto out = sw;   // unit 8 taken out of the rack: bridged
    std::vector<int> without = rack;
    without.erase (std::find (without.begin(), without.end(), 8));
    check (pb::reconcile (out, without), "a unit taken out changes the cords");
    l = pb::follow (out, 0);
    check (l.complete && std::find (l.units.begin(), l.units.end(), 8) == l.units.end() && l.units.size() == 6, "... the chain closes over it");
    auto in = s;
    auto with = rack;
    with.push_back (40);
    pb::reconcile (in, with);
    l = pb::follow (in, 0);
    check (l.complete && ! l.units.empty() && l.units.back() == 40, "a unit put in is patched in before RACK OUT");

    pb::State back;
    sw.anything = true;
    check (pb::fromString (pb::toString (sw), back) && back.anything && pb::follow (back, 0).units == pb::follow (sw, 0).units,
           "the cords come back from the session as they were saved");

    // The engine: a 1 kHz tone through the rack, the chain broken, then whole again
    enh::dsp::EnhEngine engine;
    const int block = 256;
    engine.prepare (sr, block, 2);
    enh::dsp::EnhEngine::Parameters p;
    juce::AudioBuffer<float> buf (2, block);
    double phase = 0.0;
    auto runFor = [&] (double seconds, float& peak, float& maxStep, double& rms)
    {
        peak = 0.0f; maxStep = 0.0f; rms = 0.0;
        float last = 0.0f;
        long n = 0;
        for (int b = 0; b < (int) (seconds * sr / block); ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.25f * (float) std::sin (phase);
                phase += 2.0 * 3.14159265358979 * 1000.0 / sr;
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            engine.process (buf, p);
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (0, i);
                peak = std::max (peak, std::abs (v));
                maxStep = std::max (maxStep, std::abs (v - last));
                last = v;
                rms += (double) v * v;
                ++n;
            }
        }
        rms = std::sqrt (rms / (double) std::max (1L, n));
    };
    float peak = 0, step = 0;
    double rmsOn = 0, rmsOff = 0, rmsBack = 0, rmsHalf = 0;
    runFor (2.0, peak, step, rmsOn);
    engine.setPatch (true, 0.0f);
    runFor (0.1, peak, step, rmsOff);
    runFor (0.3, peak, step, rmsOff);
    std::printf ("  playing %.4f rms, chain open %.6f rms\n", rmsOn, rmsOff);
    check (rmsOn > 0.01 && rmsOff < rmsOn * 0.001, "a cord out of the chain: the rack goes silent");
    engine.setPatch (true, 1.0f);
    runFor (1.0, peak, step, rmsHalf);
    std::printf ("  half in: crackle and hum peak %.4f\n", peak);
    check (peak < 0.06f, "a plug half in: its crackle stays well down");
    engine.setPatch (false, 0.0f);
    float stepBack = 0;
    runFor (0.02, peak, stepBack, rmsBack);
    double rmsFirst = rmsBack;
    runFor (1.5, peak, step, rmsBack);
    std::printf ("  whole again: first 20 ms %.4f rms, then %.4f rms\n", rmsFirst, rmsBack);
    check (rmsFirst < rmsOn * 0.2 && rmsBack > rmsOn * 0.8, "whole again: it comes back slowly (no jump), all the way");

    // The cords' order: the units after the enhancer in another order still play, cleanly
    {
        using E = enh::dsp::EnhEngine;
        engine.setChainOrder ({ E::stSeraph, E::stCharacter, E::stNewer + 3, E::stTide, E::stLumen, E::stLunchbox, E::stDeep, E::stNewer + 1 });
        double rmsOrder = 0;
        runFor (1.5, peak, step, rmsOrder);
        std::printf ("  re-ordered: %.4f rms, peak %.3f\n", rmsOrder, peak);
        check (rmsOrder > rmsOn * 0.5 && peak <= 1.01f, "re-ordered by the cords: it plays, within full scale");
        engine.setChainOrder ({});
    }

    // A loop of cords: feedback, filtered and soft-limited, never past full scale; loud and held, it runs away and is muted
    {
        engine.setFeedback (true);
        double rmsLoop = 0;
        runFor (2.0, peak, step, rmsLoop);
        std::printf ("  feedback loop: %.4f rms, peak %.3f, runaway %d\n", rmsLoop, peak, (int) engine.getMeters().patchRunaway.load());
        check (std::isfinite (rmsLoop) && peak <= 1.01f && rmsLoop > 0.01 && (engine.getMeters().patchRunaway.load() || rmsLoop < rmsOn * 2.0),
               "a feedback loop plays, never past full scale, never more than 6 dB up (else muted)");
        // a loop that rings on by itself once the music stops: muted
        {
            juce::AudioBuffer<float> quiet (2, block);
            bool ran = engine.getMeters().patchRunaway.load();
            for (int b = 0; b < (int) (2.0 * sr / block) && ! ran; ++b)
            {
                quiet.clear();
                engine.process (quiet, p);
                ran = engine.getMeters().patchRunaway.load();
            }
            float qp = 0.0f;
            for (int b = 0; b < (int) (0.5 * sr / block); ++b)
            {
                quiet.clear();
                engine.process (quiet, p);
                qp = std::max (qp, quiet.getMagnitude (0, block));
            }
            std::printf ("  music stopped: runaway %d, the loop's tail %.5f\n", (int) ran, qp);
            check (qp < 0.02f, "with the music stopped the loop dies away (or is muted)");
        }
        juce::AudioBuffer<float> loud (2, block);
        bool ran = false;
        for (int b = 0; b < (int) (3.0 * sr / block) && ! ran; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.98f * (float) std::sin (phase);
                phase += 2.0 * 3.14159265358979 * 80.0 / sr;
                loud.setSample (0, i, v);
                loud.setSample (1, i, v);
            }
            engine.process (loud, p);
            ran = engine.getMeters().patchRunaway.load();
        }
        double rmsAfter = 0;
        runFor (0.5, peak, step, rmsAfter);
        std::printf ("  driven hard: runaway %d, then %.6f rms\n", (int) ran, rmsAfter);
        check (! ran || rmsAfter < 0.002, "a runaway loop mutes the rack");
        engine.setFeedback (false);
        engine.setPatch (false, 0.0f);
        double rmsFree = 0;
        runFor (1.5, peak, step, rmsFree);
        check (! engine.getMeters().patchRunaway.load() && rmsFree > rmsOn * 0.5, "re-patched: the runaway clears and it plays again");
    }
}
