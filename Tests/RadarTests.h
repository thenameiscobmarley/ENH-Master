#pragma once

/*  FOOTSTEP RADAR tests (EnhDspTests --radar [table]): footsteps of every kind, near and far, alone and
    under every kind of game audio, against distractors that look like steps. Everything is synthesised
    with its ground truth, so every decision can be scored.

    Scored: a hit is an accepted step within 60 ms of a true one (the heel; the toe that follows it is
    part of the step); anything else accepted is a false alarm.
*/

#include <functional>
#include <map>
#include "DSP/FootstepRadar.h"

namespace radartests
{
    using enh::dsp::FootstepRadar;
    using enh::dsp::BiquadCoeffs;
    using enh::dsp::BiquadState;
    constexpr double twoPi = 6.283185307179586;

    struct Audio
    {
        std::vector<float> l, r;
        std::vector<double> steps;                                   // true step onsets (s)
        std::map<std::string, std::vector<double>> distractors;      // what else happens, by kind
        double sr = 48000.0;
    };

    inline Audio blank (double sr, double seconds)
    {
        Audio a;
        a.sr = sr;
        a.l.assign ((size_t) (seconds * sr), 0.0f);
        a.r.assign ((size_t) (seconds * sr), 0.0f);
        return a;
    }

    inline float fromDb (float db) { return std::pow (10.0f, db / 20.0f); }

    struct Rng
    {
        juce::Random r;
        explicit Rng (int seed) : r (seed) {}
        float noise() { return r.nextFloat() * 2.0f - 1.0f; }
        float uni (float a, float b) { return a + (b - a) * r.nextFloat(); }
        int pick (int n) { return r.nextInt (n); }
    };

    /** A short mono sound, placed into the scene at a pan (-1..1), optionally from behind (right out of phase). */
    inline void place (Audio& a, double t, const std::vector<float>& mono, float pan, bool rear = false)
    {
        const float gl = std::sqrt (0.5f * (1.0f - pan)), gr = std::sqrt (0.5f * (1.0f + pan));
        const auto start = (size_t) std::max (0.0, t * a.sr);
        for (size_t i = 0; i < mono.size() && start + i < a.l.size(); ++i)
        {
            a.l[start + i] += mono[i] * gl * 1.41f;
            a.r[start + i] += mono[i] * gr * 1.41f * (rear ? -1.0f : 1.0f);
        }
    }

    inline std::vector<float> filteredNoise (Rng& rng, double sr, double seconds, BiquadCoeffs c, bool twice, double attack, double decay)
    {
        std::vector<float> out ((size_t) (seconds * sr));
        BiquadState a, b;
        for (size_t i = 0; i < out.size(); ++i)
        {
            const double t = i / sr;
            float x = a.process (c, rng.noise());
            if (twice) x = b.process (c, x);
            const double env = (attack > 0.0 ? std::min (1.0, t / attack) : 1.0) * std::exp (-t / decay);
            out[i] = x * (float) env;
        }
        return out;
    }

    inline std::vector<float> dampedSine (double sr, double seconds, double hz, double decay, double glideTo = 0.0)
    {
        std::vector<float> out ((size_t) (seconds * sr));
        double ph = 0.0;
        for (size_t i = 0; i < out.size(); ++i)
        {
            const double t = i / sr;
            const double f = glideTo > 0.0 ? glideTo + (hz - glideTo) * std::exp (-t / 0.03) : hz;
            ph += f / sr;
            out[i] = (float) (std::sin (twoPi * ph) * std::exp (-t / decay) * std::min (1.0, t / 0.0008));
        }
        return out;
    }

    inline void mix (std::vector<float>& into, const std::vector<float>& x, float gain, size_t offset = 0)
    {
        if (into.size() < x.size() + offset) into.resize (x.size() + offset, 0.0f);
        for (size_t i = 0; i < x.size(); ++i) into[offset + i] += x[i] * gain;
    }

    inline void normalisePeak (std::vector<float>& x, float peakDb)
    {
        float p = 1.0e-9f;
        for (float v : x) p = std::max (p, std::abs (v));
        const float g = fromDb (peakDb) / p;
        for (float& v : x) v *= g;
    }

    //==============================================================================
    // Footsteps
    enum Surface { concrete, metal, wood, grass, gravel, snow, sand, water, carpet, tile, numSurfaces };
    inline const char* surfaceName (int s)
    {
        static const char* n[] { "concrete", "metal", "wood", "grass", "gravel", "snow", "sand", "water", "carpet", "tile" };
        return n[s];
    }

    /** One impact of a foot (heel or toe) on a surface, unit-ish level. */
    inline std::vector<float> impact (Rng& rng, double sr, int surface, float weight)
    {
        std::vector<float> s;
        auto thump = [&] (double cutoff, double decay, float g) { mix (s, filteredNoise (rng, sr, 0.12, BiquadCoeffs::lowPass (sr, cutoff, 0.8), true, 0.0006, decay), g * weight); };
        auto band = [&] (double hz, double q, double attack, double decay, float g) { mix (s, filteredNoise (rng, sr, std::min (0.4, decay * 6.0), BiquadCoeffs::bandPass (sr, hz, q), true, attack, decay), g); };
        auto grains = [&] (int count, double span, double lo, double hi, float g)
        {
            for (int k = 0; k < count; ++k)
            {
                const auto at = (size_t) (rng.uni (0.0f, (float) span) * sr);
                auto grain = filteredNoise (rng, sr, 0.02, BiquadCoeffs::bandPass (sr, rng.uni ((float) lo, (float) hi), 1.5), false, 0.0003, 0.004);
                mix (s, grain, g * rng.uni (0.4f, 1.0f), at);
            }
        };
        switch (surface)
        {
            case concrete: thump (180.0, 0.022, 5.0f); band (2100.0, 1.4, 0.0005, 0.014, 3.0f); band (4500.0, 1.2, 0.001, 0.010, 1.6f); break;
            case metal:    band (2400.0, 1.6, 0.0004, 0.012, 3.0f); mix (s, dampedSine (sr, 0.25, 1830.0 * rng.uni (0.9f, 1.1f), 0.045), 0.35f);
                           mix (s, dampedSine (sr, 0.25, 3120.0 * rng.uni (0.9f, 1.1f), 0.030), 0.25f); thump (160.0, 0.018, 2.5f); break;
            case wood:     mix (s, dampedSine (sr, 0.2, 260.0 * rng.uni (0.85f, 1.15f), 0.028), 0.8f * weight); thump (200.0, 0.025, 3.5f); band (1500.0, 1.2, 0.0008, 0.012, 1.5f); break;
            case grass:    band (3000.0, 0.7, 0.006, 0.045, 2.2f); thump (150.0, 0.030, 1.2f); grains (6, 0.05, 2500.0, 6000.0, 0.4f); break;
            case gravel:   grains (14, 0.07, 2000.0, 9000.0, 1.0f); thump (170.0, 0.020, 2.0f); break;
            case snow:     grains (10, 0.08, 800.0, 3000.0, 0.8f); thump (140.0, 0.035, 2.0f); band (1200.0, 0.8, 0.004, 0.05, 1.0f); break;
            case sand:     band (2400.0, 0.8, 0.008, 0.050, 2.0f); thump (130.0, 0.030, 1.5f); break;
            case water:    band (2000.0, 0.5, 0.003, 0.090, 2.5f); band (5500.0, 0.8, 0.002, 0.060, 1.5f); thump (200.0, 0.030, 1.5f); break;
            case carpet:   thump (220.0, 0.022, 4.0f); band (900.0, 1.0, 0.001, 0.010, 0.6f); break;
            case tile:     band (4200.0, 1.5, 0.0003, 0.007, 3.5f); band (1800.0, 1.4, 0.0004, 0.010, 1.6f); thump (190.0, 0.018, 3.0f); break;
            default: break;
        }
        return s;
    }

    /** A step: heel, and often the toe 30-90 ms later, a little quieter. */
    inline std::vector<float> footstep (Rng& rng, double sr, int surface)
    {
        auto s = impact (rng, sr, surface, 1.0f);
        if (rng.uni (0.0f, 1.0f) < 0.7f)
        {
            auto toe = impact (rng, sr, surface, 0.6f);
            mix (s, toe, rng.uni (0.35f, 0.7f), (size_t) (rng.uni (0.03f, 0.09f) * sr));
        }
        return s;
    }

    /** Distance: quieter, the air's top taken off, and more of a room around it. */
    inline std::vector<float> atDistance (std::vector<float> s, double sr, float distance, Rng& rng)
    {
        const double cutoff = 18000.0 * std::pow (3500.0 / 18000.0, (double) distance);
        auto lp = BiquadCoeffs::lowPass (sr, std::min (cutoff, 0.45 * sr), 0.7);
        BiquadState st;
        for (float& v : s) v = st.process (lp, v);
        // A small room: a few decaying echoes, more of them further away
        const float wet = 0.08f + 0.55f * distance;
        std::vector<float> out (s.size() + (size_t) (0.25 * sr), 0.0f);
        mix (out, s, 1.0f);
        for (int k = 0; k < 10; ++k)
        {
            const auto at = (size_t) ((0.008 + 0.018 * k + rng.uni (0.0f, 0.006f)) * sr);
            mix (out, s, wet * std::exp (-0.35f * (float) k) * (k % 2 == 0 ? 1.0f : -0.8f), at);
        }
        return out;
    }

    struct Walk { int surface; float distance; float pan; bool rear; double period; float levelDb; };

    inline void addWalk (Audio& a, Rng& rng, const Walk& w, double from, double to)
    {
        for (double t = from; t < to; t += w.period * rng.uni (0.94f, 1.06f))
        {
            auto s = atDistance (footstep (rng, a.sr, w.surface), a.sr, w.distance, rng);
            normalisePeak (s, w.levelDb + rng.uni (-2.0f, 2.0f));
            place (a, t, s, std::clamp (w.pan + rng.uni (-0.05f, 0.05f), -1.0f, 1.0f), w.rear);
            a.steps.push_back (t);
        }
    }

    //==============================================================================
    // Everything else a game plays
    inline void addAmbience (Audio& a, Rng& rng, float levelDb)
    {
        float bl0 = 0, bl1 = 0, bl2 = 0, br0 = 0, br1 = 0, br2 = 0;
        const float g = fromDb (levelDb) * 3.0f;
        for (size_t i = 0; i < a.l.size(); ++i)
        {
            const float wl = rng.noise(), wr = rng.noise();
            bl0 = 0.997f * bl0 + 0.03f * wl; bl1 = 0.96f * bl1 + 0.2f * wl; bl2 = 0.57f * bl2 + 0.4f * wl;
            br0 = 0.997f * br0 + 0.03f * wr; br1 = 0.96f * br1 + 0.2f * wr; br2 = 0.57f * br2 + 0.4f * wr;
            a.l[i] += g * (bl0 + bl1 + bl2) * 0.3f;
            a.r[i] += g * (br0 + br1 + br2) * 0.3f;
        }
    }

    inline void addWind (Audio& a, Rng& rng, float levelDb)
    {
        auto lp = BiquadCoeffs::lowPass (a.sr, 500.0, 0.7);
        BiquadState sl, sr;
        const float g = fromDb (levelDb) * 4.0f;
        for (size_t i = 0; i < a.l.size(); ++i)
        {
            const double t = i / a.sr;
            const float gust = 0.6f + 0.4f * (float) std::sin (twoPi * 0.13 * t) * (float) std::sin (twoPi * 0.31 * t + 1.0);
            a.l[i] += g * gust * sl.process (lp, rng.noise());
            a.r[i] += g * gust * sr.process (lp, rng.noise());
        }
    }

    inline void addRain (Audio& a, Rng& rng, float levelDb, double from, double to)
    {
        for (double t = from; t < to; t += rng.uni (0.005f, 0.045f))
        {
            auto drop = filteredNoise (rng, a.sr, 0.02, BiquadCoeffs::bandPass (a.sr, rng.uni (2500.0f, 8000.0f), 2.0), false, 0.0002, 0.003);
            normalisePeak (drop, levelDb + rng.uni (-8.0f, 0.0f));
            place (a, t, drop, rng.uni (-1.0f, 1.0f));
        }
        a.distractors["rain"].push_back (from);
    }

    inline void addShot (Audio& a, Rng& rng, double t, float levelDb, float pan)
    {
        std::vector<float> s;
        mix (s, filteredNoise (rng, a.sr, 0.5, BiquadCoeffs::lowPass (a.sr, 9000.0, 0.7), false, 0.0003, 0.09), 1.0f);
        mix (s, filteredNoise (rng, a.sr, 0.5, BiquadCoeffs::bandPass (a.sr, 700.0, 1.0), true, 0.0003, 0.05), 3.0f);
        mix (s, dampedSine (a.sr, 0.5, 90.0, 0.08, 55.0), 0.6f);
        normalisePeak (s, levelDb);
        place (a, t, s, pan);
        a.distractors["gunshot"].push_back (t);
    }

    inline void addBurst (Audio& a, Rng& rng, double t, int rounds, float levelDb, float pan)
    {
        for (int k = 0; k < rounds; ++k)
            addShot (a, rng, t + k * 0.095, levelDb + rng.uni (-1.5f, 0.0f), pan);
        a.distractors["auto fire"].push_back (t);
    }

    inline void addExplosion (Audio& a, Rng& rng, double t, float levelDb)
    {
        std::vector<float> s;
        mix (s, filteredNoise (rng, a.sr, 2.0, BiquadCoeffs::lowPass (a.sr, 300.0, 0.7), false, 0.004, 0.5), 3.0f);
        mix (s, filteredNoise (rng, a.sr, 1.0, BiquadCoeffs::lowPass (a.sr, 5000.0, 0.7), false, 0.001, 0.12), 1.0f);
        mix (s, dampedSine (a.sr, 2.0, 45.0, 0.6, 32.0), 1.5f);
        normalisePeak (s, levelDb);
        place (a, t, s, 0.0f);
        a.distractors["explosion"].push_back (t);
    }

    /** A voice: a glottal buzz (120-220 Hz) through two moving formants, in syllables. */
    inline void addSpeech (Audio& a, Rng& rng, double t0, double t1, float levelDb, float pan)
    {
        std::vector<float> s ((size_t) ((t1 - t0) * a.sr));
        const double f0 = rng.uni (110.0f, 210.0f);
        double ph = 0.0;
        BiquadState f1a, f2a;
        for (size_t i = 0; i < s.size(); ++i)
        {
            const double t = i / a.sr;
            ph += f0 * (1.0 + 0.06 * std::sin (twoPi * 0.7 * t)) / a.sr;
            const float buzz = (float) (2.0 * (ph - std::floor (ph)) - 1.0) + 0.2f * rng.noise();
            const double syl = std::pow (std::max (0.0, std::sin (twoPi * 4.4 * t + std::sin (twoPi * 0.9 * t))), 1.5);
            const auto f1 = BiquadCoeffs::bandPass (a.sr, 600.0 + 250.0 * std::sin (twoPi * 2.1 * t), 3.0);
            const auto f2 = BiquadCoeffs::bandPass (a.sr, 1700.0 + 500.0 * std::sin (twoPi * 1.3 * t + 1.0), 4.0);
            s[i] = (float) syl * (f1a.process (f1, buzz) + 0.6f * f2a.process (f2, buzz));
        }
        normalisePeak (s, levelDb);
        place (a, t0, s, pan);
        a.distractors["speech"].push_back (t0);
    }

    /** Music: kick on every beat, snare on 2 and 4, hi-hats in eighths, a bass line and a pad (120 bpm). */
    inline void addMusic (Audio& a, Rng& rng, double t0, double t1, float levelDb)
    {
        std::vector<float> m ((size_t) ((t1 - t0) * a.sr) + (size_t) a.sr, 0.0f);
        const double beat = 0.5;
        for (double t = 0.0; t < t1 - t0; t += beat)
        {
            mix (m, dampedSine (a.sr, 0.4, 150.0, 0.18, 52.0), 1.0f, (size_t) (t * a.sr));
            const int n = (int) std::lround (t / beat);
            if (n % 2 == 1)
            {
                mix (m, filteredNoise (rng, a.sr, 0.3, BiquadCoeffs::bandPass (a.sr, 1800.0, 0.5), false, 0.0005, 0.07), 1.6f, (size_t) (t * a.sr));
                mix (m, dampedSine (a.sr, 0.2, 185.0, 0.06), 0.5f, (size_t) (t * a.sr));
            }
            for (int h = 0; h < 2; ++h)
                mix (m, filteredNoise (rng, a.sr, 0.08, BiquadCoeffs::highPass (a.sr, 7000.0, 0.7), false, 0.0003, 0.02), 0.35f, (size_t) ((t + h * beat / 2) * a.sr));
        }
        double ph = 0.0, pp = 0.0;
        const double notes[] { 55.0, 55.0, 65.4, 49.0 };
        for (size_t i = 0; i + (size_t) a.sr < m.size(); ++i)
        {
            const double t = i / a.sr;
            ph += notes[(int) (t / 2.0) % 4] / a.sr;
            pp += 220.0 / a.sr;
            m[i] += 0.35f * (float) std::sin (twoPi * ph) + 0.10f * (float) (std::sin (twoPi * pp) + std::sin (twoPi * pp * 1.26) + std::sin (twoPi * pp * 1.5));
        }
        normalisePeak (m, levelDb);
        place (a, t0, m, 0.0f);
        a.distractors["music"].push_back (t0);
    }

    inline void addClick (Audio& a, Rng& rng, double t, float levelDb)
    {
        auto c = filteredNoise (rng, a.sr, 0.01, BiquadCoeffs::bandPass (a.sr, 3000.0, 1.0), false, 0.0001, 0.0015);
        normalisePeak (c, levelDb);
        place (a, t, c, 0.0f);
        a.distractors["ui click"].push_back (t);
    }

    inline void addReload (Audio& a, Rng& rng, double t, float levelDb, float pan)
    {
        double at = t;
        for (int k = 0; k < 4; ++k)
        {
            std::vector<float> c;
            mix (c, filteredNoise (rng, a.sr, 0.05, BiquadCoeffs::bandPass (a.sr, 2600.0, 2.0), false, 0.0002, 0.006), 2.0f);
            mix (c, dampedSine (a.sr, 0.2, rng.uni (1100.0f, 1500.0f), 0.05), 0.6f);
            normalisePeak (c, levelDb + rng.uni (-4.0f, 0.0f));
            place (a, at, c, pan);
            at += rng.uni (0.08f, 0.25f);
        }
        a.distractors["reload"].push_back (t);
    }

    //==============================================================================
    // Running and scoring
    struct Score
    {
        int trueSteps = 0, hits = 0, falseAlarms = 0;
        double seconds = 0.0;
        float recall() const { return trueSteps > 0 ? (float) hits / (float) trueSteps : 1.0f; }
        float falsePerMinute() const { return seconds > 0.0 ? (float) (falseAlarms / (seconds / 60.0)) : 0.0f; }
    };

    /** The accepted steps' onset times, running the radar on the scene (blocks of 256). */
    inline std::vector<FootstepRadar::Step> runRadar (const Audio& a, FootstepRadar::Settings s, std::vector<float>* outL = nullptr,
                                                      std::vector<FootstepRadar::Decision>* log = nullptr, bool trace = false)
    {
        FootstepRadar radar;
        radar.prepare (a.sr, 256);
        radar.log = log;
        s.active = true;
        std::vector<float> l = a.l, r = a.r;
        std::vector<FootstepRadar::Step> accepted;
        int seen = 0;
        for (size_t pos = 0; pos < l.size(); pos += 256)
        {
            const int n = (int) std::min<size_t> (256, l.size() - pos);
            float* ch[2] { l.data() + pos, r.data() + pos };
            radar.process (ch, 2, n, s);
            if (trace)
            {
                std::printf ("    t %6.3f  ph %d last %.3f onset %5.1f  thr %5.1f  mus %.2f  excess", radar.getClock(), radar.getPhase(), radar.getLastOnset(), radar.getOnsetStrength(), radar.getThreshold(), radar.getMusicality());
                for (float e : radar.getBandExcessDb()) std::printf (" %4.1f", e);
                std::printf ("  bg");
                for (float b : radar.getBackgroundDb()) std::printf (" %5.1f", b);
                std::printf ("\n");
            }
            while (seen < radar.getStepsTotal())
            {
                accepted.push_back (radar.getRecent()[(size_t) (seen % FootstepRadar::recentCapacity)]);
                ++seen;
            }
        }
        if (outL != nullptr) *outL = std::move (l);
        return accepted;
    }

    inline Score score (const Audio& a, const std::vector<FootstepRadar::Step>& found, double latencyS)
    {
        Score sc;
        sc.trueSteps = (int) a.steps.size();
        sc.seconds = (double) a.l.size() / a.sr;
        std::vector<bool> taken (a.steps.size(), false);
        for (auto& st : found)
        {
            const double t = st.time - latencyS * 0.0;   // step times are onsets on the input's timeline
            bool matched = false, partOfStep = false;
            for (size_t k = 0; k < a.steps.size(); ++k)
            {
                const double d = t - a.steps[k];
                if (d >= -0.02 && d <= 0.06 && ! taken[k]) { taken[k] = true; matched = true; break; }
                if (d > -0.02 && d <= 0.16) partOfStep = true;   // its toe, or its room
            }
            if (matched) ++sc.hits;
            else if (! partOfStep) ++sc.falseAlarms;
        }
        return sc;
    }
}

//==============================================================================
// The runner (EnhDspTests --radar [table] [old]): uses the test file's check(), Scene and engine run()
namespace radartests
{
    inline Audio fromScene (const Scene& s, double sr)
    {
        Audio a;
        a.sr = sr;
        a.l = s.left;
        a.r = s.right;
        for (auto& e : s.steps) a.steps.push_back (e.time);
        for (auto& e : s.shots) a.distractors["gunshot"].push_back (e.time);
        for (auto& e : s.crates) a.distractors["crate"].push_back (e.time);
        return a;
    }

    /** The old detector (the enhancer's, before 3.7): its accepted events through the engine. */
    inline std::vector<FootstepRadar::Step> runOld (const Audio& a)
    {
        std::vector<FootstepRadar::Step> out;
#if ENH_HAS_OLD_FOOTSTEP_DETECTOR
        Scene s;
        s.left = a.l;
        s.right = a.r;
        EnhEngine::Parameters p;
        p.footstep = true;
        const int block = 128;
        const auto r = run (s, a.sr, block, p);
        auto last = enh::dsp::FootstepDetector::Phase::idle;
        for (size_t k = 0; k < r.traces.size(); ++k)
        {
            const auto ph = r.traces[k].phase;
            if (ph == enh::dsp::FootstepDetector::Phase::accepted && last != ph)
            {
                FootstepRadar::Step st;
                st.time = (double) (k * block) / a.sr - 0.042;   // it decides 42 ms after the onset
                out.push_back (st);
            }
            last = ph;
        }
#else
        juce::ignoreUnused (a);
#endif
        return out;
    }

    /** For the glass-panel method test: a clear walk, a faint far one at the edge of what the radar hears, and
        a few look-alikes - so DETECTION's three settings take different steps, and ROOM has far ones to place. */
    inline Audio methodScene()
    {
        Rng rng (41);
        auto a = blank (48000.0, 4.0);
        addAmbience (a, rng, -58.0f);
        addWalk (a, rng, { concrete, 0.5f, -0.4f, false, 0.5, -40.0f }, 0.4, 3.8);
        addWalk (a, rng, { gravel, 0.9f, 0.6f, false, 0.45, -54.0f }, 0.6, 3.8);
        addWalk (a, rng, { grass, 0.95f, 0.0f, false, 0.6, -60.0f }, 0.5, 3.8);
        addReload (a, rng, 1.1, -30.0f, 0.3f);
        addClick (a, rng, 2.3, -34.0f);
        addRain (a, rng, -46.0f, 2.6, 3.6);
        return a;
    }

    struct Row { std::string name; Score news, olds; std::vector<FootstepRadar::Step> found {}; };

    /** RADAR_STATS: what the background estimator sees, per band, in a few kinds of sound. */
    inline void printStats (double sr)
    {
        struct K { const char* name; std::function<void (Audio&, Rng&)> add; };
        const K kinds[] { { "ambience", [] (Audio& a, Rng& r) { addAmbience (a, r, -50.0f); } },
                          { "wind",     [] (Audio& a, Rng& r) { addWind (a, r, -30.0f); } },
                          { "rain",     [] (Audio& a, Rng& r) { addRain (a, r, -32.0f, 0.0, 6.0); } },
                          { "music",    [] (Audio& a, Rng& r) { addMusic (a, r, 0.0, 6.0, -10.0f); } },
                          { "speech",   [] (Audio& a, Rng& r) { addSpeech (a, r, 0.0, 6.0, -18.0f, 0.0f); } } };
        for (auto& k : kinds)
        {
            Rng rng (5);
            auto a = blank (sr, 6.0);
            k.add (a, rng);
            FootstepRadar radar;
            radar.prepare (sr, 256);
            FootstepRadar::Settings s; s.active = true;
            for (size_t pos = 0; pos < a.l.size(); pos += 256)
            {
                float* ch[2] { a.l.data() + pos, a.r.data() + pos };
                radar.process (ch, 2, (int) std::min<size_t> (256, a.l.size() - pos), s);
            }
            std::printf ("  %-9s musicality %.2f  periodicity %.2f  flicker dB:", k.name, radar.getMusicality(), radar.getPeriodicity());
            for (float f : radar.getFlickerDb()) std::printf (" %5.2f", f);
            std::printf ("\n");
        }
    }

    inline void runRadarTests (double sr, bool table, bool compareOld)
    {
        std::printf ("\n== FOOTSTEP RADAR: every surface, near to very far, under game audio, against look-alikes ==\n");
        // A real recording (the radar's SAVE THE LAST 30 SECONDS): RADAR_FILE=<wav>, optionally
        // RADAR_STEPS="t1,t2,..." where you hear steps (s). Every event the radar weighed, and what it took.
        if (const auto file = juce::SystemStats::getEnvironmentVariable ("RADAR_FILE", {}); file.isNotEmpty())
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (juce::File (file).createInputStream().release(), true));
            if (reader == nullptr)
            {
                std::printf ("  can't read %s\n", file.toRawUTF8());
                return;
            }
            Audio a;
            a.sr = reader->sampleRate;
            const int n = (int) reader->lengthInSamples;
            juce::AudioBuffer<float> buf (2, n);
            reader->read (&buf, 0, n, 0, true, true);
            a.l.assign (buf.getReadPointer (0), buf.getReadPointer (0) + n);
            a.r.assign (buf.getReadPointer (reader->numChannels > 1 ? 1 : 0), buf.getReadPointer (reader->numChannels > 1 ? 1 : 0) + n);
            for (auto& t : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("RADAR_STEPS", {}), ",", {}))
                if (t.trim().isNotEmpty())
                    a.steps.push_back (t.trim().getDoubleValue());
            double peak = 0.0, sum = 0.0;
            for (int i = 0; i < n; ++i) { peak = std::max (peak, (double) std::abs (a.l[(size_t) i])); sum += (double) a.l[(size_t) i] * a.l[(size_t) i]; }
            std::printf ("  %s: %.1f s at %.0f Hz, peak %.1f dBFS, RMS %.1f dBFS\n", file.toRawUTF8(), n / a.sr, a.sr,
                         20.0 * std::log10 (peak + 1e-12), 10.0 * std::log10 (sum / std::max (1, n) + 1e-20));
            FootstepRadar::Settings s;
            std::vector<FootstepRadar::Decision> log;
            log.reserve (100000);
            const auto found = runRadar (a, s, nullptr, &log, false);
            std::printf ("  --- every event (time, ACCEPT, P, base, match, attack ms, decay dB, excess dB, tonal, level dBFS, spread, ...)\n");
            for (auto& d : log)
                std::printf ("  %7.3f %s P %.2f base %.2f m %.2f  att %5.1f  dec %5.1f  ex %5.1f  ton %.2f  lvl %6.1f  spr %.0f  dur %4.1f body %.2f rate %4.1f mus %.2f imp %.2f ring %.2f bang %.2f rap %.1f nar %.2f grid %.2f\n",
                             d.time, d.accepted ? (d.withdrawn == 1 ? "SUSTND" : d.withdrawn == 2 ? "KICKED" : "ACCEPT") : "reject", d.probability, d.base, d.match, d.attackMs, d.decayDb, d.excessDb, d.tonal, d.levelDb, d.spread,
                             d.durationMs, d.body, d.rate, d.music, d.impact, d.ring, d.bang, d.rapid, d.narrow, d.grid);
            if (std::getenv ("RADAR_BANDS") != nullptr)
                for (auto& d : log)
                {
                    std::printf ("  %7.3f bands p/att/dec:", d.time);
                    for (int b = 0; b < 6; ++b)
                        std::printf ("  %5.1f/%4.1f/%5.1f", d.bandP[(size_t) b], d.bandAtt[(size_t) b], d.bandDec[(size_t) b]);
                    std::printf ("\n");
                }
            std::printf ("  %zu events weighed, %zu steps taken\n", log.size(), found.size());
            if (! a.steps.empty())
            {
                const auto sc = score (a, found, 0.0);
                std::printf ("  your %d marked steps: %d found, %d false lifts\n", sc.trueSteps, sc.hits, sc.falseAlarms);
            }
            return;
        }
        if (juce::SystemStats::getEnvironmentVariable ("RADAR_METHODS", {}).isNotEmpty())
        {
            const auto a = methodScene();
            for (int d = 0; d < 3; ++d)
            {
                FootstepRadar::Settings m;
                m.detection = d;
                std::printf ("  DETECTION %d: %zu steps taken\n", d, runRadar (a, m).size());
            }
            return;
        }
        if (juce::SystemStats::getEnvironmentVariable ("RADAR_STATS", {}).isNotEmpty())
        {
            printStats (sr);
            return;
        }
        FootstepRadar::Settings s;   // the defaults: SENSITIVITY 6, standard detection
        std::vector<Row> rows;
        const juce::String dumpWhat = juce::SystemStats::getEnvironmentVariable ("RADAR_DUMP", {});
        auto runOne = [&] (const std::string& name, const Audio& a)
        {
            std::vector<FootstepRadar::Decision> log;
            log.reserve (20000);
            const bool dumping = dumpWhat.isNotEmpty() && juce::String (name).containsIgnoreCase (dumpWhat);
            auto found = runRadar (a, s, nullptr, &log, dumping && std::getenv ("RADAR_TRACE") != nullptr);
            Row row { name, score (a, found, 0.0), {}, found };
            if (dumpWhat.isNotEmpty() && juce::String (name).containsIgnoreCase (dumpWhat))
            {
                std::printf ("  --- %s: every event (time, ACCEPT, P, base, match, attack ms, decay dB, excess dB, tonal, level dBFS, spread, distance, nearest step)\n", name.c_str());
                for (auto& d : log)
                {
                    double near = 99.0;
                    for (double t : a.steps) if (std::abs (d.time - t) < std::abs (near)) near = d.time - t;
                    std::printf ("  %7.3f %s P %.2f base %.2f m %.2f  att %5.1f  dec %5.1f  ex %5.1f  ton %.2f  lvl %6.1f  spr %.0f  dur %4.1f body %.2f rate %4.1f mus %.2f imp %.2f ring %.2f bang %.2f rap %.1f nar %.2f grid %.2f step %+.3f\n",
                                 d.time, d.accepted ? (d.withdrawn == 1 ? "SUSTND" : d.withdrawn == 2 ? "KICKED" : "ACCEPT") : "reject", d.probability, d.base, d.match, d.attackMs, d.decayDb, d.excessDb, d.tonal, d.levelDb, d.spread,
                                 d.durationMs, d.body, d.rate, d.music, d.impact, d.ring, d.bang, d.rapid, d.narrow, d.grid, near);
                    if (std::getenv ("RADAR_BANDS") != nullptr)
                        for (int b = 0; b < 6; ++b)
                            std::printf ("            band %d: p %5.1f att %5.1f dec %5.1f ton %.2f imp %.2f\n", b, d.bandP[(size_t) b], d.bandAtt[(size_t) b],
                                         d.bandDec[(size_t) b], d.bandTon[(size_t) b], d.bandImp[(size_t) b]);
                }
            }
            if (dumpWhat.isNotEmpty() && juce::String (name).containsIgnoreCase (dumpWhat))
                for (double t : a.steps)
                {
                    double near = 99.0;
                    for (auto& d : log) if (std::abs (d.time - t) < std::abs (near)) near = d.time - t;
                    if (near < -0.02 || near > 0.06)
                        std::printf ("  no event for the step at %7.3f (nearest event %+.3f)\n", t, near);
                    else
                    {
                        bool found = false;
                        for (auto& st : row.found) if (st.time - t >= -0.02 && st.time - t <= 0.06) found = true;
                        if (! found)
                            std::printf ("  step at %7.3f: event at %+.3f not counted\n", t, near);
                    }
                }
            if (compareOld)
                row.olds = score (a, runOld (a), 0.0);
            rows.push_back (row);
            return row;
        };

        // 1. Every surface at every distance, over a quiet ambience (steps alone)
        // Distance is quietness, a duller top and more room: very far steps are -60 dBFS in a quiet scene;
        // buried ones are as loud as the ambience around them (the hardest case there is)
        struct Dist { const char* name; float distance, levelDb, ambienceDb; };
        const Dist dists[] { { "near", 0.1f, -20.0f, -64.0f }, { "mid", 0.45f, -36.0f, -64.0f }, { "far", 0.8f, -50.0f, -64.0f },
                             { "very far", 0.95f, -60.0f, -76.0f }, { "buried", 0.8f, -58.0f, -64.0f } };
        std::map<std::string, Score> byDistance, byDistanceOld;
        int seed = 100;
        for (int surface = 0; surface < numSurfaces; ++surface)
            for (auto& d : dists)
            {
                Rng rng (seed++);
                auto a = blank (sr, 9.0);
                addAmbience (a, rng, d.ambienceDb);
                addWalk (a, rng, { surface, d.distance, rng.uni (-0.8f, 0.8f), false, 0.5, d.levelDb }, 0.6, 8.6);
                const auto row = runOne (std::string (surfaceName (surface)) + ", " + d.name, a);
                auto& agg = byDistance[d.name];
                agg.trueSteps += row.news.trueSteps; agg.hits += row.news.hits; agg.falseAlarms += row.news.falseAlarms; agg.seconds += row.news.seconds;
                auto& ago = byDistanceOld[d.name];
                ago.trueSteps += row.olds.trueSteps; ago.hits += row.olds.hits; ago.falseAlarms += row.olds.falseAlarms; ago.seconds += row.olds.seconds;
            }

        // 2. Walking, running, sneaking, from behind, two walkers at once
        {
            Rng rng (7);
            auto a = blank (sr, 12.0); addAmbience (a, rng, -58.0f);
            addWalk (a, rng, { concrete, 0.4f, -0.5f, false, 0.30, -34.0f }, 0.5, 11.5);
            runOne ("running (0.3 s)", a);
        }
        {
            Rng rng (8);
            auto a = blank (sr, 12.0); addAmbience (a, rng, -58.0f);
            addWalk (a, rng, { wood, 0.5f, 0.3f, false, 0.8, -44.0f }, 0.5, 11.5);
            runOne ("sneaking (0.8 s, quiet)", a);
        }
        {
            Rng rng (9);
            auto a = blank (sr, 12.0); addAmbience (a, rng, -58.0f);
            addWalk (a, rng, { gravel, 0.5f, 0.0f, true, 0.5, -38.0f }, 0.5, 11.5);
            runOne ("from behind (out of phase)", a);
        }
        {
            Rng rng (10);
            auto a = blank (sr, 14.0); addAmbience (a, rng, -58.0f);
            addWalk (a, rng, { concrete, 0.3f, -0.7f, false, 0.52, -30.0f }, 0.5, 13.5);
            addWalk (a, rng, { metal, 0.7f, 0.6f, false, 0.37, -44.0f }, 0.8, 13.5);
            std::sort (a.steps.begin(), a.steps.end());
            runOne ("two walkers at once", a);
        }

        // 3. Under game audio: music, a voice, gunfire, explosions, rain, wind
        struct Under { const char* name; std::function<void (Audio&, Rng&)> add; };
        const Under under[] {
            { "under music (steps 16 dB below it)", [] (Audio& a, Rng& r) { addMusic (a, r, 0.0, 14.0, -14.0f); } },
            { "under a voice",                      [] (Audio& a, Rng& r) { addSpeech (a, r, 1.0, 13.0, -20.0f, 0.1f); } },
            { "between gunfire",                    [] (Audio& a, Rng& r) { for (double t = 1.2; t < 13.0; t += 2.3) addBurst (a, r, t, 6, -4.0f, -0.3f); } },
            { "around explosions",                  [] (Audio& a, Rng& r) { for (double t = 2.0; t < 13.0; t += 4.0) addExplosion (a, r, t, -3.0f); } },
            { "in the rain",                        [] (Audio& a, Rng& r) { addRain (a, r, -40.0f, 0.0, 14.0); } },
            { "in the wind",                        [] (Audio& a, Rng& r) { addWind (a, r, -30.0f); } },
        };
        for (auto& u : under)
        {
            Rng rng (200 + (int) rows.size());
            auto a = blank (sr, 14.0);
            addAmbience (a, rng, -56.0f);
            u.add (a, rng);
            for (int w = 0; w < 3; ++w)
                addWalk (a, rng, { rng.pick (numSurfaces), rng.uni (0.2f, 0.7f), rng.uni (-0.9f, 0.9f), false, rng.uni (0.4f, 0.6f), -30.0f },
                         0.5 + w * 4.5, 0.5 + w * 4.5 + 3.8);
            std::sort (a.steps.begin(), a.steps.end());
            runOne (u.name, a);
        }

        // 4. Look-alikes with no steps at all: every one accepted is a false alarm
        const Under alone[] {
            { "music alone",       [] (Audio& a, Rng& r) { addMusic (a, r, 0.0, 30.0, -10.0f); } },
            { "speech alone",      [] (Audio& a, Rng& r) { addSpeech (a, r, 0.5, 29.5, -18.0f, 0.0f); } },
            { "gunfire alone",     [] (Audio& a, Rng& r) { for (double t = 0.8; t < 29.0; t += 1.7) { if (r.pick (2)) addBurst (a, r, t, 5, -3.0f, r.uni (-1, 1)); else addShot (a, r, t, -3.0f, r.uni (-1, 1)); } } },
            { "explosions alone",  [] (Audio& a, Rng& r) { for (double t = 1.0; t < 29.0; t += 3.0) addExplosion (a, r, t, -2.0f); } },
            { "rain alone",        [] (Audio& a, Rng& r) { addRain (a, r, -32.0f, 0.0, 30.0); } },
            { "reloads alone",     [] (Audio& a, Rng& r) { for (double t = 0.8; t < 29.0; t += 1.9) addReload (a, r, t, -24.0f, r.uni (-1, 1)); } },
            { "UI clicks alone",   [] (Audio& a, Rng& r) { for (double t = 0.5; t < 29.5; t += r.uni (0.2f, 1.5f)) addClick (a, r, t, -26.0f); } },
        };
        for (auto& u : alone)
        {
            Rng rng (300 + (int) rows.size());
            auto a = blank (sr, 30.0);
            addAmbience (a, rng, -58.0f);
            u.add (a, rng);
            runOne (u.name, a);
        }

        // 5. The scenes the old detector was tuned on (steps, gunshots, speech, crates)
        runOne ("classic game scene", fromScene (makeScene (sr, 30.0, true, true, 42), sr));
        runOne ("classic varied surfaces", fromScene (makeScene (sr, 30.0, true, true, 7, true), sr));
        runOne ("classic crates", fromScene (makeCrateScene (sr, 40.0, 5), sr));

        // --- the table ---------------------------------------------------------------------------------------
        if (table || compareOld)
        {
            std::printf ("  %-40s %14s %12s", "scene", "steps found", "false/min");
            if (compareOld) std::printf ("   | old: %9s %9s", "found", "false/min");
            std::printf ("\n");
            for (auto& r : rows)
            {
                std::printf ("  %-40s %3d / %3d %3.0f%% %9.1f", r.name.c_str(), r.news.hits, r.news.trueSteps, 100.0f * r.news.recall(), r.news.falsePerMinute());
                if (compareOld) std::printf ("   |      %3.0f%% %9.1f", 100.0f * r.olds.recall(), r.olds.falsePerMinute());
                std::printf ("\n");
            }
        }
        for (auto& [name, sc] : byDistance)
        {
            std::printf ("  every surface, %-9s: %3.0f%% found, %.1f false / min", name.c_str(), 100.0f * sc.recall(), sc.falsePerMinute());
            if (compareOld) std::printf ("   (old: %3.0f%%, %.1f)", 100.0f * byDistanceOld[name].recall(), byDistanceOld[name].falsePerMinute());
            std::printf ("\n");
        }

        // --- what it must do ------------------------------------------------------------------------------
        // Two kinds of line. A check fails the run: what the radar does today, with a little room, so any step
        // back is caught. A goal only reports: where it should get to (a step 16 dB under music, or under a
        // voice 10 dB louder than it, is still often missed).
        auto find = [&] (const std::string& n) -> const Row& { for (auto& r : rows) if (r.name == n) return r; return rows.front(); };
        std::vector<std::string> goals;
        auto goal = [&] (bool ok, const std::string& what) { if (! ok) goals.push_back (what); };
        check (byDistance["near"].recall() >= 0.95f, "near steps on every surface found (>= 95 %)");
        check (byDistance["mid"].recall() >= 0.95f, "mid-distance steps on every surface found (>= 95 %)");
        check (byDistance["far"].recall() >= 0.90f, "far steps (-50 dBFS) on every surface found (>= 90 %)");
        check (byDistance["very far"].recall() >= 0.90f, "very far steps (-60 dBFS, a quiet scene) on every surface found (>= 90 %)");
        check (byDistance["buried"].recall() >= 0.15f, "buried steps (as loud as the ambience): at least 15 % found");
        goal (byDistance["buried"].recall() >= 0.5f, "buried steps: half found");
        for (auto* n : { "running (0.3 s)", "sneaking (0.8 s, quiet)", "from behind (out of phase)" })
            check (find (n).news.recall() >= 0.9f, juce::String (n) + ": >= 90 % found");
        check (find ("two walkers at once").news.recall() >= 0.8f, "two walkers at once: >= 80 % found");
        goal (find ("two walkers at once").news.recall() >= 0.95f, "two walkers at once: 95 % found");
        // Under game audio: today's floor per scene, and the goal of 80 % for all
        const std::map<std::string, float> floors { { "under music (steps 16 dB below it)", 0.4f }, { "under a voice", 0.4f }, { "between gunfire", 0.5f },
                                                    { "around explosions", 0.6f }, { "in the rain", 0.45f }, { "in the wind", 0.85f } };
        for (auto& u : under)
        {
            const float floor = floors.count (u.name) > 0 ? floors.at (u.name) : 0.8f;
            check (find (u.name).news.recall() >= floor, juce::String (u.name) + ": >= " + juce::String (juce::roundToInt (floor * 100.0f)) + " % of steps found");
            goal (find (u.name).news.recall() >= 0.8f, std::string (u.name) + ": 80 % found");
        }
        for (auto& u : alone)
            check (find (u.name).news.falsePerMinute() <= 4.0f, juce::String (u.name) + ": at most 4 false steps a minute");
        for (auto& r : rows)
        {
            // Crates (a latch's clank and a lid's thud are impacts too) and steps under a drum beat (its
            // hits land in step with the walker) are the two known weak spots: held where they are
            const float most = r.name == "classic crates" ? 36.0f : r.name.rfind ("under music", 0) == 0 ? 30.0f : 10.0f;
            check (r.news.falsePerMinute() <= most || r.name.find ("alone") != std::string::npos,
                   juce::String (r.name) + ": at most " + juce::String (most, 0) + " false steps a minute");
            goal (r.news.falsePerMinute() <= 6.0f || r.name.find ("alone") != std::string::npos, r.name + ": at most 6 false steps a minute");
        }
        if (! goals.empty())
        {
            std::printf ("  Goals not reached yet (these do not fail the run):\n");
            for (auto& g : goals)
                std::printf ("    [GOAL] %s\n", g.c_str());
        }
    }
}
