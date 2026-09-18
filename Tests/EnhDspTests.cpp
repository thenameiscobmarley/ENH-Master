/*  Offline tests / benchmark for the ENH Master DSP engine.

    Synthetic scenes only (no game recordings are bundled), so treat the detection
    numbers as a sanity check of the detector's logic, not as real-game accuracy.
*/
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <functional>
#include <map>
#include <complex>
#include "DSP/EnhEngine.h"
#include "DSP/ParameterMapping.h"
#include "Parameters/FactoryPresets.h"

using enh::dsp::EnhEngine;
using enh::dsp::BiquadCoeffs;
using enh::dsp::BiquadState;

namespace
{
    constexpr double twoPi = 6.283185307179586;

    struct Event { double time; };

    struct Scene
    {
        std::vector<float> left, right;
        std::vector<Event> steps, shots, crates;
        std::vector<std::pair<double, double>> speech;
    };

    float dbfs (float db) { return std::pow (10.0f, db / 20.0f); }

    struct Pink
    {
        float b0 = 0, b1 = 0, b2 = 0;
        float next (juce::Random& r)
        {
            const float w = r.nextFloat() * 2.0f - 1.0f;
            b0 = 0.99765f * b0 + w * 0.0990460f;
            b1 = 0.96300f * b1 + w * 0.2965164f;
            b2 = 0.57000f * b2 + w * 1.0526913f;
            return (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
        }
    };

    void addFootstep (Scene& s, double sr, double t, float peakDb, juce::Random& r)
    {
        auto lp = BiquadCoeffs::lowPass (sr, 220.0, 0.9);
        auto bp = BiquadCoeffs::bandPass (sr, 3500.0, 1.2);
        BiquadState a, b, c;
        const int start = (int) (t * sr), len = (int) (0.15 * sr);
        const float amp = dbfs (peakDb);

        for (int i = 0; i < len && start + i < (int) s.left.size(); ++i)
        {
            const float tt = (float) i / (float) sr;
            const float thump = a.process (lp, r.nextFloat() * 2 - 1) * std::exp (-tt / 0.030f) * 6.0f;
            const float scuff = tt > 0.005f ? c.process (bp, b.process (bp, r.nextFloat() * 2 - 1)) * std::exp (-(tt - 0.005f) / 0.020f) * 5.0f : 0.0f;
            const float v = amp * (thump + scuff);
            s.left[(size_t) (start + i)] += v * 0.9f;
            s.right[(size_t) (start + i)] += v * 1.1f;
        }
        s.steps.push_back ({ t });
    }

    /** Surface / distance variations: 0 classic, 1 hard floor/metal, 2 wood, 3 gravel, 4 distant. */
    void addFootstepProfile (Scene& s, double sr, double t, float peakDb, int profile, juce::Random& r)
    {
        if (profile == 0)
        {
            addFootstep (s, sr, t, peakDb, r);
            return;
        }

        struct Part { BiquadCoeffs c; bool twoStage; float decay, gain, delay; };
        std::vector<Part> parts;

        switch (profile)
        {
            case 1: parts = { { BiquadCoeffs::bandPass (sr, 1900.0, 2.0), true, 0.015f, 7.0f, 0.0f },
                              { BiquadCoeffs::bandPass (sr, 4500.0, 1.5), true, 0.012f, 4.0f, 0.002f } }; break;
            case 2: parts = { { BiquadCoeffs::bandPass (sr, 350.0, 1.5), true, 0.040f, 7.0f, 0.0f },
                              { BiquadCoeffs::lowPass (sr, 180.0, 0.9), false, 0.030f, 3.0f, 0.0f } }; break;
            case 3: parts = { { BiquadCoeffs::highPass (sr, 7000.0, 0.7), false, 0.025f, 5.0f, 0.0f },
                              { BiquadCoeffs::bandPass (sr, 3000.0, 1.2), true, 0.020f, 3.0f, 0.004f } }; break;
            default: parts = { { BiquadCoeffs::lowPass (sr, 220.0, 0.9), false, 0.035f, 6.0f, 0.0f },
                               { BiquadCoeffs::bandPass (sr, 450.0, 1.0), true, 0.030f, 2.5f, 0.003f } }; break;
        }

        const int start = (int) (t * sr), len = (int) (0.15 * sr);
        const float amp = dbfs (peakDb);

        for (auto& part : parts)
        {
            BiquadState a, b;
            for (int i = 0; i < len && start + i < (int) s.left.size(); ++i)
            {
                const float tt = (float) i / (float) sr - part.delay;
                if (tt < 0.0f) continue;
                float x = a.process (part.c, r.nextFloat() * 2 - 1);
                if (part.twoStage) x = b.process (part.c, x);
                const float v = amp * x * part.gain * std::exp (-tt / part.decay);
                s.left[(size_t) (start + i)] += v * 0.9f;
                s.right[(size_t) (start + i)] += v * 1.1f;
            }
        }

        s.steps.push_back ({ t });
    }

    void addGunshot (Scene& s, double sr, double t, juce::Random& r)
    {
        auto lp = BiquadCoeffs::lowPass (sr, 7000.0, 0.7);
        auto body = BiquadCoeffs::bandPass (sr, 700.0, 1.0);
        BiquadState a, b;
        const int start = (int) (t * sr), len = (int) (0.45 * sr);

        for (int i = 0; i < len && start + i < (int) s.left.size(); ++i)
        {
            const float tt = (float) i / (float) sr;
            const float n = r.nextFloat() * 2 - 1;
            const float v = dbfs (-4.0f) * (a.process (lp, n) * std::exp (-tt / 0.09f) + b.process (body, n) * 3.0f * std::exp (-tt / 0.05f));
            s.left[(size_t) (start + i)] += v;
            s.right[(size_t) (start + i)] += v;
        }
        s.shots.push_back ({ t });
    }

    void addSpeech (Scene& s, double sr, double t0, double t1, juce::Random& r)
    {
        auto bp = BiquadCoeffs::bandPass (sr, 750.0, 0.8);
        BiquadState a, b;
        for (int i = (int) (t0 * sr); i < (int) (t1 * sr) && i < (int) s.left.size(); ++i)
        {
            const double tt = i / sr;
            const float syllables = (float) std::pow (0.5 + 0.5 * std::sin (twoPi * 4.3 * tt), 2.0);
            const float v = dbfs (-22.0f) * b.process (bp, a.process (bp, r.nextFloat() * 2 - 1)) * 4.0f * syllables;
            s.left[(size_t) i] += v;
            s.right[(size_t) i] += v;
        }
        s.speech.push_back ({ t0, t1 });
    }

    /** Crate / supply box opening: latch clank (ringing metal), lid creak (tonal squeak),
        rummaging rattles (short 1-4 kHz noise bursts, 40-150 ms apart) and a lid thud.
        Several parts are individually footstep-like; lasts ~1.1 s. */
    void addCrate (Scene& s, double sr, double t0, int variant, juce::Random& r)
    {
        const size_t n = s.left.size();
        auto add = [&] (double t, float v) { const auto i = (size_t) (t * sr); if (i < n) { s.left[i] += v; s.right[i] += v * 0.95f; } };
        const float level = dbfs (-26.0f - (float) r.nextDouble() * 6.0f);
        const float detune = 0.9f + 0.2f * r.nextFloat();

        if (variant != 1)   // latch clank: noise click exciting inharmonic metal modes
        {
            const float modes[3] { 1450.0f * detune, 2380.0f * detune, 3720.0f * detune };
            for (int i = 0; i < (int) (0.35 * sr); ++i)
            {
                const double tt = i / sr;
                float v = tt < 0.002 ? (r.nextFloat() * 2 - 1) * 2.0f : 0.0f;
                for (int m = 0; m < 3; ++m)
                    v += (float) std::sin (twoPi * modes[m] * tt) * (float) std::exp (-tt / (0.12 - 0.02 * m)) * (1.0f - 0.25f * m);
                add (t0 + tt, v * level * 1.2f);
            }
        }

        if (variant != 2)   // rummaging rattles
        {
            auto bp = BiquadCoeffs::bandPass (sr, 2200.0 * detune, 1.0);
            double t = t0 + (variant == 1 ? 0.0 : 0.18);
            for (int burst = 0; burst < 5 + r.nextInt (3); ++burst)
            {
                BiquadState a, b;
                const float g = level * (0.6f + 0.8f * r.nextFloat()) * 5.0f;
                for (int i = 0; i < (int) (0.05 * sr); ++i)
                {
                    const double tt = i / sr;
                    add (t + tt, b.process (bp, a.process (bp, r.nextFloat() * 2 - 1)) * g * (float) std::exp (-tt / 0.009));
                }
                t += 0.04 + 0.11 * r.nextDouble();
            }
        }

        if (variant != 1)   // lid creak: gliding squeak with harmonics and stick-slip roughness
        {
            double phase = 0.0;
            const double start = t0 + 0.12, len = 0.35;
            for (int i = 0; i < (int) (len * sr); ++i)
            {
                const double tt = i / sr;
                phase += (900.0 + 350.0 * tt / len) * detune / sr;
                const float env = (float) std::sin (juce::MathConstants<double>::pi * tt / len);
                const float rough = 0.7f + 0.3f * (float) std::sin (twoPi * 31.0 * tt);
                const float v = (float) (std::sin (twoPi * phase) + 0.5 * std::sin (2 * twoPi * phase) + 0.3 * std::sin (3 * twoPi * phase));
                add (start + tt, v * env * rough * level * 0.45f);
            }
        }

        // Lid thud shortly after the last rattle / creak
        {
            auto lp = BiquadCoeffs::lowPass (sr, 200.0, 0.9);
            auto bp = BiquadCoeffs::bandPass (sr, 2000.0, 1.2);
            BiquadState a, b, c;
            const double at = t0 + (variant == 2 ? 0.55 : 0.78);
            for (int i = 0; i < (int) (0.12 * sr); ++i)
            {
                const double tt = i / sr;
                const float v = a.process (lp, r.nextFloat() * 2 - 1) * (float) std::exp (-tt / 0.035) * 6.0f
                              + c.process (bp, b.process (bp, r.nextFloat() * 2 - 1)) * (float) std::exp (-tt / 0.015) * 4.0f;
                add (at + tt, v * level);
            }
        }

        s.crates.push_back ({ t0 });
    }

    /** Walking with pauses, and crates opened in the pauses (0 = latch+creak+rattles, 1 = rummage, 2 = latch+creak). */
    Scene makeCrateScene (double sr, double seconds, int seed)
    {
        Scene s;
        const auto n = (size_t) (seconds * sr);
        s.left.assign (n, 0.0f);
        s.right.assign (n, 0.0f);
        juce::Random r (seed);
        Pink pinkL, pinkR;
        for (size_t i = 0; i < n; ++i)
        {
            s.left[i] = pinkL.next (r) * dbfs (-42.0f);
            s.right[i] = pinkR.next (r) * dbfs (-42.0f);
        }

        double t = 0.8;
        int crate = 0;
        while (t < seconds - 2.5)
        {
            for (int k = 0; k < 6 && t < seconds - 2.5; ++k, t += 0.48 + (r.nextDouble() - 0.5) * 0.06)
                addFootstepProfile (s, sr, t, -24.0f - (float) r.nextDouble() * 8.0f, r.nextInt (4), r);

            t += 0.5;
            addCrate (s, sr, t, crate++ % 3, r);
            t += 1.6;
        }

        return s;
    }

    Scene makeScene (double sr, double seconds, bool withShots, bool withSpeech, int seed, bool variedSurfaces = false)
    {
        Scene s;
        const auto n = (size_t) (seconds * sr);
        s.left.assign (n, 0.0f);
        s.right.assign (n, 0.0f);
        juce::Random r (seed);
        Pink pinkL, pinkR;

        for (size_t i = 0; i < n; ++i)
        {
            s.left[i] = pinkL.next (r) * dbfs (-42.0f);
            s.right[i] = pinkR.next (r) * dbfs (-42.0f);
        }

        std::vector<double> shotTimes;
        if (withShots)
            for (double t = 1.3; t < seconds - 0.5; t += 2.1 + r.nextDouble() * 0.8)
                shotTimes.push_back (t);

        for (double t = 0.8; t < seconds - 0.3; t += 0.48 + (r.nextDouble() - 0.5) * 0.06)
        {
            bool nearShot = false;
            for (auto st : shotTimes)
                nearShot = nearShot || std::abs (st - t) < 0.5;

            if (! nearShot)
            {
                if (variedSurfaces)
                {
                    const int profile = r.nextInt (5);
                    const float level = -24.0f - (float) r.nextDouble() * 8.0f - (profile == 4 ? 6.0f : 0.0f);
                    addFootstepProfile (s, sr, t, level, profile, r);
                }
                else
                {
                    addFootstep (s, sr, t, -24.0f - (float) r.nextDouble() * 8.0f, r);
                }
            }
        }

        for (auto st : shotTimes)
            addGunshot (s, sr, st, r);

        if (withSpeech)
        {
            addSpeech (s, sr, 3.0, 6.0, r);
            addSpeech (s, sr, 12.0, 15.0, r);
        }

        return s;
    }

    /** Music-like programme: kick, snare, hats, bass, pad and a formant "vocal". */
    Scene makeMusic (double sr, double seconds, int seed)
    {
        Scene s;
        const auto n = (size_t) (seconds * sr);
        s.left.assign (n, 0.0f);
        s.right.assign (n, 0.0f);
        juce::Random r (seed);

        auto hatHp = BiquadCoeffs::highPass (sr, 7000.0, 0.7);
        auto snareBp = BiquadCoeffs::bandPass (sr, 1800.0, 0.8);
        auto bassLp = BiquadCoeffs::lowPass (sr, 400.0, 0.9);
        auto padLp = BiquadCoeffs::lowPass (sr, 1500.0, 0.7);
        auto f1 = BiquadCoeffs::bandPass (sr, 700.0, 5.0), f2 = BiquadCoeffs::bandPass (sr, 1200.0, 6.0), f3 = BiquadCoeffs::bandPass (sr, 2600.0, 7.0);
        BiquadState hat, snare, bass, padL, padR, v1, v2, v3;

        const double bpm = 110.0, beat = 60.0 / bpm;
        const double notes[4] { 55.0, 55.0, 73.4, 82.4 };
        double bassPhase = 0, padP[3] {}, voxPhase = 0;

        for (size_t i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double inBeat = std::fmod (t, beat), inEighth = std::fmod (t, beat * 0.5);
            const int barIndex = (int) (t / (beat * 4.0)) % 4;

            const float kick = (float) (std::sin (twoPi * (50.0 + 90.0 * std::exp (-inBeat * 30.0)) * inBeat) * std::exp (-inBeat * 9.0)) * 0.55f;
            const double snT = std::fmod (t + beat, beat * 2.0);
            const float sn = snT < 0.25 ? (float) ((snare.process (snareBp, r.nextFloat() * 2 - 1) * 2.5 + 0.3 * std::sin (twoPi * 190.0 * snT)) * std::exp (-snT * 18.0)) * 0.45f : 0.0f;
            const float hh = hat.process (hatHp, r.nextFloat() * 2 - 1) * (float) std::exp (-inEighth * 60.0) * 0.18f;

            bassPhase += notes[barIndex] / sr;
            const float saw = (float) (2.0 * (bassPhase - std::floor (bassPhase)) - 1.0);
            const float bs = bass.process (bassLp, saw) * 0.30f;

            float pad = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                padP[k] += (220.0 * (1.0 + 0.004 * (k - 1)) * (barIndex % 2 == 0 ? 1.0 : 1.335)) / sr;
                pad += (float) (2.0 * (padP[k] - std::floor (padP[k])) - 1.0);
            }

            voxPhase += (180.0 + 20.0 * std::sin (twoPi * 0.5 * t)) / sr;
            const float pulse = (voxPhase - std::floor (voxPhase)) < 0.15 ? 1.0f : -0.18f;
            const float syll = (float) std::pow (std::max (0.0, std::sin (twoPi * 3.1 * t)), 1.5) * (std::fmod (t, 4.0) < 2.5 ? 1.0f : 0.0f);
            const float vox = (v1.process (f1, pulse) * 1.0f + v2.process (f2, pulse) * 0.6f + v3.process (f3, pulse) * 0.35f) * syll * 0.35f;

            const float mono = kick + sn + bs + vox;
            s.left[i]  = 0.5f * (mono + hh * 0.8f + padL.process (padLp, pad) * 0.08f);
            s.right[i] = 0.5f * (mono + hh * 1.2f + padR.process (padLp, pad) * 0.08f);
        }

        return s;
    }

    struct RunResult
    {
        std::vector<float> confidence;   // per block
        std::vector<enh::dsp::FootstepDetector::Trace> traces;
        double meanAbsBandGain = 0.0;
        std::array<double, enh::dsp::numBands> curve {};   // mean displayed EQ curve over the last 40 %
        float treble = 0.0f, bass = 0.0f;                  // measured balance at the end
        float maxAbsBandGain = 0.0f;
        std::vector<float> outL, outR;
        double seconds = 0.0;
        bool finite = true;
        float peak = 0.0f;
    };

    RunResult run (const Scene& scene, double sr, int blockSize, const EnhEngine::Parameters& p,
                   std::function<void (const EnhEngine&)> atEnd = {})
    {
        EnhEngine engine;
        engine.prepare (sr, blockSize, 2);

        RunResult res;
        const int total = (int) scene.left.size();
        res.outL.resize ((size_t) total);
        res.outR.resize ((size_t) total);
        juce::AudioBuffer<float> buf (2, blockSize);

        const auto t0 = juce::Time::getHighResolutionTicks();
        int curveBlocks = 0;

        for (int pos = 0; pos < total; pos += blockSize)
        {
            const int n = std::min (blockSize, total - pos);
            buf.setSize (2, n, false, false, true);
            std::copy_n (scene.left.data() + pos, n, buf.getWritePointer (0));
            std::copy_n (scene.right.data() + pos, n, buf.getWritePointer (1));

            engine.process (buf, p);

            for (int i = 0; i < n; ++i)
            {
                const float l = buf.getSample (0, i), r = buf.getSample (1, i);
                res.finite = res.finite && std::isfinite (l) && std::isfinite (r);
                res.peak = std::max ({ res.peak, std::abs (l), std::abs (r) });
                res.outL[(size_t) (pos + i)] = l;
                res.outR[(size_t) (pos + i)] = r;
            }

            res.confidence.push_back (engine.getFootstepConfidence());

            float blockAbs = 0.0f;
            for (auto& g : engine.getMeters().bandGainDb)
            {
                const float v = std::abs (g.load());
                blockAbs += v;
                res.maxAbsBandGain = std::max (res.maxAbsBandGain, v);
            }
            res.meanAbsBandGain += blockAbs / (float) enh::dsp::numBands;
            res.traces.push_back (engine.getFootstepTrace());

            if (pos >= (int) (0.6 * total))
            {
                for (int k = 0; k < enh::dsp::numBands; ++k)
                    res.curve[(size_t) k] += engine.getEQ().getGainDb (k);
                ++curveBlocks;
            }
        }

        for (auto& c : res.curve)
            c /= std::max (1, curveBlocks);
        res.treble = engine.getEQ().getTrebleBalanceDb();
        res.bass = engine.getEQ().getBassBalanceDb();

        if (atEnd)
            atEnd (engine);

        res.meanAbsBandGain /= (double) std::max<size_t> (1, res.confidence.size());
        res.seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        return res;
    }

    float maxConfidence (const RunResult& r, double sr, int block, double from, double to)
    {
        float m = 0.0f;
        for (int b = std::max (0, (int) (from * sr / block)); b <= (int) (to * sr / block) && b < (int) r.confidence.size(); ++b)
            m = std::max (m, r.confidence[(size_t) b]);
        return m;
    }

    float rmsDb (const std::vector<float>& x, size_t from, size_t to)
    {
        double s = 0;
        for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
        return (float) (10.0 * std::log10 (s / (double) std::max<size_t> (1, to - from) + 1e-20));
    }

    float toneDb (const std::vector<float>& x, double sr, double hz, size_t from, size_t to)
    {
        double re = 0, im = 0;
        for (size_t i = from; i < to; ++i)
        {
            re += x[i] * std::cos (twoPi * hz * i / sr);
            im += x[i] * std::sin (twoPi * hz * i / sr);
        }
        const double amp = 2.0 * std::sqrt (re * re + im * im) / (double) (to - from);
        return (float) (20.0 * std::log10 (amp + 1e-12));
    }

    int failures = 0;

    void check (bool ok, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
        if (! ok) ++failures;
    }
    //==========================================================================
    /*  SPECTRAL LIMITER ("anti-pumping"). A music-like programme - broadband bed, a kick every
        half second (normal bass), steady 2 kHz detail standing in for footsteps - and then one
        abnormal event. Measured at the output of the real chain (ENH at STRENGTH 0, compressor IN)
        with the spectral limiter IN and OUT. */
    struct LimiterRun
    {
        float detailDipDb = 0.0f;       // 2 kHz detail during the event vs just before it
        float bassDuringDb = 0.0f;      // 55-140 Hz level during the event
        float maxCutBeforeDb = 0.0f;    // deepest spectral cut while only normal material played
        float cutDuringDb = 0.0f, cutAfterDb = 0.0f, broadbandDuringDb = 0.0f, cutHz = 0.0f;
        float compressorGrDuringDb = 0.0f, peak = 0.0f;
        std::array<float, 3> levelerBefore {}, levelerDuring {};   // UPWARD LEVELER lift per band
        bool finite = true;
    };

    LimiterRun runLimiterScene (double sr, bool limiterIn, bool broadbandEvent, int blockSize, float eventDb = 0.0f, bool trebleEvent = false,
                                const EnhEngine::Parameters* whole = nullptr)
    {
        const double seconds = 10.5, eventAt = 8.0, eventLen = 0.5;
        const int n = (int) (seconds * sr);
        std::vector<float> l ((size_t) n), r ((size_t) n);
        juce::Random rnd (21);
        Pink pinkL, pinkR, pinkE;
        BiquadState dA, dB, eA, eB;
        const auto detailBp = BiquadCoeffs::bandPass (sr, 2000.0, 3.0);
        const auto rumbleLp = BiquadCoeffs::lowPass (sr, 110.0, 0.8);
        const auto whistleBp = BiquadCoeffs::bandPass (sr, 5200.0, 4.0);

        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            float bed = 0.10f * pinkL.next (rnd), bedR = 0.10f * pinkR.next (rnd);

            // Kick: 58 Hz, falling pitch, -10 dBFS peak, every 0.5 s - loud, but normal for this music
            const double kt = std::fmod (t, 0.5);
            const float kick = dbfs (-10.0f) * (float) (std::sin (twoPi * (58.0 + 40.0 * std::exp (-kt / 0.02)) * kt) * std::exp (-kt / 0.11));

            // Detail band (footsteps / voices stand-in), steady
            const float det = dA.process (detailBp, dB.process (detailBp, rnd.nextFloat() * 2.0f - 1.0f)) * dbfs (-14.0f) * 3.0f;

            float ev = 0.0f;
            if (t >= eventAt && t < eventAt + eventLen)
            {
                const double et = t - eventAt;
                const float env = (float) std::min (1.0, et / 0.010) * (float) std::min (1.0, (eventAt + eventLen - t) / 0.03);
                if (trebleEvent)
                    ev = dbfs (eventDb) * env * 6.0f * eA.process (whistleBp, eB.process (whistleBp, rnd.nextFloat() * 2.0f - 1.0f));   // a piercing whistle
                else if (broadbandEvent)
                    ev = dbfs (eventDb) * 2.2f * pinkE.next (rnd) * env;                     // everything louder
                else
                    ev = dbfs (eventDb) * env * (0.75f * (float) std::sin (twoPi * 72.0 * et)     // a huge bass rumble
                                               + 1.6f * eA.process (rumbleLp, eB.process (rumbleLp, rnd.nextFloat() * 2.0f - 1.0f)));
            }

            l[(size_t) i] = bed + kick + det + ev;
            r[(size_t) i] = bedR + kick + det * 0.95f + ev;
        }

        EnhEngine engine;
        engine.prepare (sr, blockSize, 2);
        EnhEngine::Parameters p;
        p.normalize = 0.0f; p.strength = 0.0f;               // ENH does nothing: only the 1U units act
        p.seraph.mode = enh::dsp::Seraph::off;
        p.tide.active = true;                                // the broadband compressor that used to pump
        p.limiter.active = limiterIn;
        if (whole != nullptr)
            p = *whole;   // a whole-rack setting (the factory presets)

        const auto bassBp = BiquadCoeffs::bandPass (sr, 90.0, 0.9), detBp = BiquadCoeffs::bandPass (sr, 2000.0, 3.0);
        BiquadState b1, b2, d1, d2;
        double detBefore = 0, detDuring = 0, bassDuring = 0;
        int nBefore = 0, nDuring = 0;
        LimiterRun out;
        juce::AudioBuffer<float> buf (2, blockSize);

        for (int pos = 0; pos < n; pos += blockSize)
        {
            const int m = std::min (blockSize, n - pos);
            buf.setSize (2, m, false, false, true);
            for (int i = 0; i < m; ++i) { buf.setSample (0, i, l[(size_t) (pos + i)]); buf.setSample (1, i, r[(size_t) (pos + i)]); }
            engine.process (buf, p);

            const double t = (pos + m) / sr;
            const auto& lim = engine.getLimiter();
            if (t > 3.0 && t < eventAt - 0.02)
                out.maxCutBeforeDb = std::max (out.maxCutBeforeDb, lim.getDeepestCutDb());
            if (t > eventAt - 0.3 && t < eventAt - 0.02)
                for (int b = 0; b < 3; ++b) out.levelerBefore[(size_t) b] = std::max (out.levelerBefore[(size_t) b], engine.getLeveler().getReadout().gainDb[(size_t) b]);
            if (t > eventAt + 0.1 && t < eventAt + 0.45)
            {
                for (int b = 0; b < 3; ++b) out.levelerDuring[(size_t) b] = std::max (out.levelerDuring[(size_t) b], engine.getLeveler().getReadout().gainDb[(size_t) b]);
                out.cutDuringDb = std::max (out.cutDuringDb, lim.getDeepestCutDb());
                out.broadbandDuringDb = std::max (out.broadbandDuringDb, lim.getBroadbandDb());
                out.compressorGrDuringDb = std::max (out.compressorGrDuringDb, engine.getCompressor().getReadout().gainReductionDb);
                for (auto& sl : lim.getSlots())
                    if (sl.depthDb >= out.cutDuringDb - 1.0e-3f && sl.depthDb > 0.5f)
                        out.cutHz = sl.hz;
            }
            if (std::getenv ("LIMITER_TRACE") != nullptr && limiterIn && blockSize == 128 && broadbandEvent == (std::atoi (std::getenv ("LIMITER_TRACE")) == 2) && ((t > (std::getenv ("LIMITER_FROM") ? std::atof (std::getenv ("LIMITER_FROM")) : eventAt - 0.6) && t < eventAt + 0.6 && pos % (blockSize * (std::getenv ("LIMITER_FINE") ? 1 : 24)) == 0)))
            {
                const auto& v = lim.getBandView();
                std::printf ("   t=%.2f cut=%.1f bb=%.1f loc=%.2f tideGR=%.1f |", t, lim.getDeepestCutDb(), lim.getBroadbandDb(), lim.getLocalisation(), engine.getCompressor().getReadout().gainReductionDb);
                for (int k = 0; k < 12; ++k) std::printf (" %3.0f:%+.0f/%.0f", enh::dsp::BandAnalyzer::centreHz (k), v.excursionDb[(size_t) k], v.normalDb[(size_t) k]); std::printf (" | over"); for (int k = 0; k < 12; ++k) std::printf (" %+.0f", v.overDb[(size_t) k]);
                std::printf ("\n");
            }
            if (t > eventAt + eventLen + 0.7 && t < eventAt + eventLen + 0.8)
                out.cutAfterDb = std::max (out.cutAfterDb, lim.getDeepestCutDb());

            for (int i = 0; i < m; ++i)
            {
                const float y = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
                const double ti = (pos + i) / sr;
                const float bass = b1.process (bassBp, b2.process (bassBp, y));
                const float det = d1.process (detBp, d2.process (detBp, y));
                out.peak = std::max ({ out.peak, std::abs (buf.getSample (0, i)), std::abs (buf.getSample (1, i)) });
                out.finite = out.finite && std::isfinite (y);
                // Whole kick cycles either side, so the kick itself averages out
                if (ti >= eventAt - 1.0 && ti < eventAt) { detBefore += det * det; ++nBefore; }
                if (ti >= eventAt + 0.05 && ti < eventAt + 0.45) { detDuring += det * det; bassDuring += bass * bass; ++nDuring; }
            }
        }

        out.detailDipDb = (float) (10.0 * std::log10 ((detDuring / std::max (1, nDuring) + 1e-15) / (detBefore / std::max (1, nBefore) + 1e-15)));
        out.bassDuringDb = (float) (10.0 * std::log10 (bassDuring / std::max (1, nDuring) + 1e-15));
        return out;
    }

    void runLimiterTests (double sr)
    {
        std::printf ("\n== SPECTRAL LIMITER (anti-pumping): huge bass hit over a mix with kicks and 2 kHz detail ==\n");
        const auto off = runLimiterScene (sr, false, false, 128);
        const auto on = runLimiterScene (sr, true, false, 128);
        std::printf ("  limiter OUT: 2 kHz detail during the hit %+.1f dB, bass %.1f dB, compressor GR %.1f dB\n",
                     off.detailDipDb, off.bassDuringDb, off.compressorGrDuringDb);
        std::printf ("  limiter IN : 2 kHz detail during the hit %+.1f dB, bass %.1f dB, compressor GR %.1f dB\n",
                     on.detailDipDb, on.bassDuringDb, on.compressorGrDuringDb);
        std::printf ("  spectral cut: %.1f dB around %.0f Hz during the hit, %.2f dB on normal kicks before it, %.2f dB 0.7 s after; broadband %.2f dB\n",
                     on.cutDuringDb, on.cutHz, on.maxCutBeforeDb, on.cutAfterDb, on.broadbandDuringDb);

        check (on.maxCutBeforeDb < 1.0f, "normal bass (kicks) is left alone");
        check (on.cutDuringDb > 4.0f && on.cutHz > 40.0f && on.cutHz < 300.0f, "the abnormal hit is cut where it is (40-300 Hz)");
        const float balanceOff = off.bassDuringDb - off.detailDipDb, balanceOn = on.bassDuringDb - on.detailDipDb;
        std::printf ("  bass relative to the detail during the hit: OUT %.1f dB, IN %.1f dB\n", balanceOff, balanceOn);
        check (balanceOn < balanceOff - 3.0f, "the offending bass region comes down relative to everything else");
        check (std::abs (on.detailDipDb) < 0.4f * std::abs (off.detailDipDb), "the 2 kHz detail ducks less than half as much (less compressor pumping)");
        check (on.compressorGrDuringDb < off.compressorGrDuringDb - 4.0f, "the compressor is no longer driven by the bass hit");
        check (std::abs (on.detailDipDb) < 1.0f, "the 2 kHz detail stays within 1 dB through the hit");
        check (on.broadbandDuringDb < 0.5f, "no broadband gain reduction for a localised event");
        check (on.cutAfterDb < 1.0f, "the cut lets go once the hit is over");

        std::printf ("\n  same scene, a broadband event instead (everything +12 dB):\n");
        const auto wide = runLimiterScene (sr, true, true, 128, -4.0f);
        std::printf ("  spectral cut %.1f dB, broadband %.1f dB, 2 kHz detail %+.1f dB\n", wide.cutDuringDb, wide.broadbandDuringDb, wide.detailDipDb);
        check (wide.cutDuringDb < 2.0f, "a broadband event is not carved up spectrally");

        std::printf ("\n  same scene, an abnormal treble burst instead (5 kHz whistle):\n");
        const auto treble = runLimiterScene (sr, true, false, 128, -6.0f, true);
        std::printf ("  spectral cut %.1f dB around %.0f Hz, broadband %.2f dB\n", treble.cutDuringDb, treble.cutHz, treble.broadbandDuringDb);
        check (treble.cutDuringDb > 3.0f && treble.cutHz > 2500.0f, "the cut follows the event to wherever it is (not a fixed bass rule)");

        const auto odd = runLimiterScene (sr, true, false, 1024);
        const auto one = runLimiterScene (sr, true, false, 7);
        std::printf ("  block 1024: bass %.2f dB, detail %+.2f dB | block 7: bass %.2f dB, detail %+.2f dB | block 128: bass %.2f dB, detail %+.2f dB\n",
                     odd.bassDuringDb, odd.detailDipDb, one.bassDuringDb, one.detailDipDb, on.bassDuringDb, on.detailDipDb);
        check (std::abs (odd.detailDipDb - on.detailDipDb) < 0.1f && std::abs (one.detailDipDb - on.detailDipDb) < 0.1f
                 && std::abs (odd.bassDuringDb - on.bassDuringDb) < 0.1f && std::abs (one.bassDuringDb - on.bassDuringDb) < 0.1f,
               "same result whatever the host block size (decisions land where they were made)");
        check (on.finite && wide.finite && on.peak <= 1.0f && wide.peak <= 1.0f, "stays finite and under full scale");

        {
            using enh::dsp::SpectralLimiter;
            std::array<SpectralLimiter::Slot, SpectralLimiter::numSlots> cuts {};
            cuts[0] = { SpectralLimiter::Shape::bell, 90.0f, 1.2f, 8.0f };
            const float atCentre = SpectralLimiter::responseDb (cuts, 90.0f), at2k = SpectralLimiter::responseDb (cuts, 2000.0f);
            std::printf ("  8 dB bell at 90 Hz: %.2f dB at 90 Hz, %.3f dB at 2 kHz\n", atCentre, at2k);
            check (std::abs (atCentre + 8.0f) < 0.05f && at2k > -0.05f, "a cut stays in its region");
        }
    }
    //==========================================================================
    /** A factory preset as the engine sees it: the panel values through the same mapping the plugin uses. */
    EnhEngine::Parameters presetParameters (const pad::presets::Preset& preset)
    {
        enh::dsp::KnobValues k;
        namespace id = pad::params::id;
        for (auto& spec : pad::params::allSpecs())
        {
            const float v = pad::presets::valueFor (preset, spec);
            const auto& i = spec.id;
            const bool on = v > 0.5f;
            if (i == id::clarityNorm) k.clarityNorm = v;          else if (i == id::clarityAdd) k.clarityAdd = v;
            else if (i == id::clarityMode) k.clarityAddMode = on; else if (i == id::adaptSpeed) k.adaptPercent = v;
            else if (i == id::sub) k.subPercent = v;              else if (i == id::subBoost) k.subBoost = on;
            else if (i == id::footstep) k.footstep = on;          else if (i == id::enhMultiply) k.enhMultiply = v;
            else if (i == id::enhStrength) k.enhStrength = v;
            else if (i == id::tideMix) k.tideMixPercent = v;      else if (i == id::tideResponse) k.tideResponse = v;
            else if (i == id::tideActive) k.tideActive = on;
            else if (i == id::lumenTarget) k.lumenTargetDb = v;   else if (i == id::lumenResponse) k.lumenResponse = v;
            else if (i == id::lumenActive) k.lumenActive = on;
            else if (i == id::spectralRange) k.spectralRangeDb = v;     else if (i == id::spectralRelease) k.spectralReleaseMs = v;
            else if (i == id::spectralCeiling) k.spectralCeilingDb = v; else if (i == id::spectralActive) k.spectralActive = on;
            else if (i == id::seraphMode) k.seraphMode = juce::roundToInt (v);
            else if (i == id::seraphMultiply) k.seraphMultiply = v; else if (i == id::seraphStrength) k.seraphStrength = v;
            else if (i == id::silkSmooth) k.smooth = v;   else if (i == id::silkAir) k.air = v;
            else if (i == id::silkWarmth) k.warmth = v;   else if (i == id::silkBody) k.body = v;
            else if (i == id::silkOutput) k.outputDb = v; else if (i == id::silkProtect) k.protect = on;
            else if (i == id::silkTape) k.tape = on;      else if (i == id::silkAuto) k.autoGain = on;
            else if (i == id::heavenHold) k.heavenHold = v; else if (i == id::heavenLift) k.heavenLift = v;
            else if (i == id::heavenMode) k.heavenLiftMode = on;
            else if (i == id::haloWidth) k.widthPercent = v; else if (i == id::haloSpace) k.space = v;
            else if (i == id::haloDecay) k.decayS = v;       else if (i == id::haloShimmer) k.shimmer = v;
            else if (i == id::haloTone) k.tone = v;          else if (i == id::haloDuck) k.duck = on;
            else if (i == id::haloBassMono) k.bassMono = on; else if (i == id::haloMod) k.mod = on;
        }
        return enh::dsp::mapKnobs (k);
    }

    double rmsDb (const std::vector<float>& l, const std::vector<float>& r, size_t from)
    {
        double sum = 0.0;
        for (size_t i = from; i < l.size(); ++i)
            sum += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]);
        return 10.0 * std::log10 (sum / (double) std::max<size_t> (1, l.size() - from) + 1e-15);
    }

    void runPresetTests (double sr)
    {
        std::printf ("\n== Factory presets (whole rack) on the game scene and the bass-hit mix ==\n");
        std::printf ("  %-24s %8s %7s %6s %6s %10s %9s\n", "preset", "level", "peak", "events", "cut", "detail dip", "comp GR");
        const auto scene = makeScene (sr, 16.0, true, true, 42);
        const double inDb = rmsDb (scene.left, scene.right, (size_t) (4.0 * sr));

        std::map<juce::String, float> dips, dipsOut;
        int defaultSteps = 0, footstepSteps = 0;
        for (auto& preset : pad::presets::all())
        {
            const auto p = presetParameters (preset);
            int steps = 0;
            const auto r = run (scene, sr, 256, p, [&] (const EnhEngine& e) { steps = e.getFootstepEventCount(); });
            const double outDb = rmsDb (r.outL, r.outR, (size_t) (4.0 * sr));
            // A bass hit peaking around full scale (a game's output), and the test's over-full-scale one
            const auto hit = runLimiterScene (sr, true, false, 256, -6.0f, false, &p);
            auto without = p;
            without.limiter.active = false;
            const auto hitOut = runLimiterScene (sr, true, false, 256, -6.0f, false, &without);
            dips[preset.name] = hit.detailDipDb;
            dipsOut[preset.name] = hitOut.detailDipDb;
            std::printf ("  %-24s %+6.1f dB %7.3f %6d %5.1f %+9.1f dB %6.1f dB   (limiter OUT: %+.1f dB)\n", preset.name, outDb - inDb, r.peak, steps,
                         hit.cutDuringDb, hit.detailDipDb, hit.compressorGrDuringDb, hitOut.detailDipDb);

            check (r.finite && r.peak <= 1.0f && hit.finite && hit.peak <= 1.0f, juce::String (preset.name) + ": stable, under full scale");
            check (std::abs (outDb - inDb) < 9.0, juce::String (preset.name) + ": level within 9 dB of the input");
            if (juce::String (preset.name) == "DEFAULT") defaultSteps = steps;
            if (juce::String (preset.name) == "COMPETITIVE FOOTSTEPS") footstepSteps = steps;
        }

        check (footstepSteps >= (int) scene.steps.size() * 7 / 10, "COMPETITIVE FOOTSTEPS detects most footsteps");
        // With the whole rack running, the output limiter at the end still has the last word on a hot
        // mix; what the spectral limiter must do is take most of the ducking away.
        for (auto& [name, dip] : dips)
        {
            if (name == "TRANSPARENT (ALL OUT)")
                continue;
            const float bound = name == "BASS HEAVY, PROTECTED" ? 5.0f : 2.5f;
            check (std::abs (dip) < std::abs (dipsOut[name]) - 0.15f && std::abs (dip) < bound,
                   name + ": less ducking with the SPECTRAL LIMITER than without, and under " + juce::String (bound, 1) + " dB");
        }
        (void) defaultSteps;

        if (std::getenv ("PRESET_DIAG") != nullptr)
        {
            // Which stage ducks the detail under the bass hit, with the whole rack running (DEFAULT)
            const auto base = presetParameters (pad::presets::all()[(size_t) std::atoi (std::getenv ("PRESET_DIAG"))]);
            auto variant = [&] (const char* what, auto&& change)
            {
                auto p = base;
                change (p);
                const float eventDb = std::getenv ("DIAG_EVENT") != nullptr ? (float) std::atof (std::getenv ("DIAG_EVENT")) : 0.0f;
                const auto hit = runLimiterScene (sr, true, false, 256, eventDb, false, &p);
                std::printf ("    preset %-28s detail %+5.1f dB  cut %4.1f  comp GR %4.1f  peak %.3f  leveler L/M/H before %.1f/%.1f/%.1f during %.1f/%.1f/%.1f\n",
                             what, hit.detailDipDb, hit.cutDuringDb, hit.compressorGrDuringDb, hit.peak,
                             hit.levelerBefore[0], hit.levelerBefore[1], hit.levelerBefore[2], hit.levelerDuring[0], hit.levelerDuring[1], hit.levelerDuring[2]);
            };
            variant ("as is", [] (auto&) {});
            variant ("tone & space OFF", [] (auto& p) { p.seraph.mode = enh::dsp::Seraph::off; });
            variant ("tone & space: no loudness hold", [] (auto& p) { p.seraph.heaven.amount = 0.0f; });
            variant ("leveler OUT", [] (auto& p) { p.lumen.active = false; });
            variant ("compressor OUT", [] (auto& p) { p.tide.active = false; });
            variant ("enhancer STRENGTH 0", [] (auto& p) { p.strength = 0.0f; p.normalize = 0.0f; p.sub = 0.0f; });
            variant ("only leveler", [] (auto& p) { p.strength = 0.0f; p.normalize = 0.0f; p.sub = 0.0f; p.limiter.active = false; p.tide.active = false; p.seraph.mode = enh::dsp::Seraph::off; });
            variant ("limiter OUT", [] (auto& p) { p.limiter.active = false; });
            variant ("only limiter + compressor", [] (auto& p) { p.strength = 0.0f; p.normalize = 0.0f; p.sub = 0.0f; p.lumen.active = false; p.seraph.mode = enh::dsp::Seraph::off; });
        }

        // The reference preset: level unchanged, and nothing processing the bass hit
        {
            const auto p = presetParameters (pad::presets::all().back());
            const auto r = run (scene, sr, 256, p);
            const double outDb = rmsDb (r.outL, r.outR, (size_t) (4.0 * sr));
            const auto hit = runLimiterScene (sr, true, false, 256, 0.0f, false, &p);
            std::printf ("  TRANSPARENT: level %+.2f dB, limiter cut %.2f dB, compressor GR %.2f dB\n", outDb - inDb, hit.cutDuringDb, hit.compressorGrDuringDb);
            check (std::abs (outDb - inDb) < 0.5 && hit.cutDuringDb < 0.01f && hit.compressorGrDuringDb < 0.01f,
                   "TRANSPARENT (ALL OUT): level unchanged, no unit processing");
        }
    }
}

int main (int argc, char** argv)
{
    const double sr = 48000.0;
    const int block = 128;

    if (argc > 1 && juce::String (argv[1]) == "--presets")
    {
        runPresetTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--limiter")
    {
        runLimiterTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    //==========================================================================
    // Real recordings: EnhDspTests --analyze capture.wav [clarity 0..1]
    if (argc > 2 && juce::String (argv[1]) == "--analyze")
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])));
        if (reader == nullptr) { std::printf ("cannot read %s\n", argv[2]); return 2; }

        const double fileRate = reader->sampleRate;
        const int len = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> file (2, len);
        reader->read (&file, 0, len, 0, true, true);

        EnhEngine engine;
        engine.prepare (fileRate, block, 2);
        EnhEngine::Parameters p;
        p.footstep = true;
        p.normalize = argc > 3 ? (float) std::atof (argv[3]) : 0.6f;

        juce::AudioBuffer<float> buf (2, block);
        auto lastPhase = enh::dsp::FootstepDetector::Phase::idle;
        double nextReport = 5.0;
        using Phase = enh::dsp::FootstepDetector::Phase;
        std::printf ("time    event      str  decay tonal ctx  clutter hot  bb   seq  score\n");

        for (int pos = 0; pos < len; pos += block)
        {
            const int n = std::min (block, len - pos);
            buf.setSize (2, n, false, false, true);
            buf.copyFrom (0, 0, file, 0, pos, n);
            buf.copyFrom (1, 0, file, file.getNumChannels() > 1 ? 1 : 0, pos, n);
            engine.process (buf, p);

            const auto& t = engine.getFootstepTrace();
            const double now = (pos + n) / fileRate;
            if (t.phase != lastPhase && (t.phase == Phase::accepted || t.phase == Phase::rejected))
                std::printf ("%7.3f %-9s  %.2f %.2f  %.2f  %.2f %.2f    %.2f %.2f %.2f %.2f\n", now,
                             t.phase == Phase::accepted ? "FOOTSTEP" : "rejected",
                             t.strength, t.decay, t.tonal, t.context, t.clutter, t.hot, t.broadband, t.sequence, t.score);
            lastPhase = t.phase;

            if (now >= nextReport)
            {
                const auto& plan = engine.getHarmonicPlan();
                std::printf ("-- %.0fs  EQ curve (dB):", now);
                for (int k = 0; k < enh::dsp::numBands; k += 2)
                    std::printf (" %.0fHz:%+.1f", enh::dsp::BandAnalyzer::centreHz (k), engine.getEQ().getGainDb (k));
                std::printf ("\n   depth %.0f Hz (%.2f)  clarity %.0f Hz (%.2f)  bass %.0f Hz\n",
                             plan.depth.hz, plan.depth.amount, plan.clarity.hz, plan.clarity.amount, plan.bassHz);
                nextReport += 5.0;
            }
        }

        std::printf ("footsteps accepted %d, events rejected %d\n", engine.getFootstepDetector().getEventCount(),
                     engine.getFootstepDetector().getRejectedCount());
        return 0;
    }

    // Per-event decisions with ground truth: EnhDspTests --events quiet|crates|game
    if (argc > 2 && juce::String (argv[1]) == "--events")
    {
        const juce::String which (argv[2]);
        const auto scene = which == "quiet" ? makeScene (sr, 12.0, false, false, 77)
                         : which == "crates" ? makeCrateScene (sr, 40.0, argc > 3 ? std::atoi (argv[3]) : 11)
                         : which == "varied" ? makeScene (sr, 30.0, true, true, argc > 3 ? std::atoi (argv[3]) : 7, true)
                         : makeScene (sr, 24.0, true, true, argc > 3 ? std::atoi (argv[3]) : 1234);
        EnhEngine engine;
        const int eb = 32;
        engine.prepare (sr, eb, 2);
        EnhEngine::Parameters p;
        p.normalize = 0.6f; p.footstep = true;
        juce::AudioBuffer<float> buf (2, eb);
        using Phase = enh::dsp::FootstepDetector::Phase;
        auto last = Phase::idle;
        double onset = 0.0;

        auto label = [&] (double t)
        {
            for (auto& st : scene.steps) if (t >= st.time - 0.01 && t < st.time + 0.2) return juce::String ("STEP ") + juce::String ((t - st.time) * 1000.0, 0) + "ms";
            for (auto& c : scene.crates) if (t >= c.time - 0.01 && t < c.time + 1.2) return juce::String ("crate+") + juce::String ((t - c.time) * 1000.0, 0) + "ms";
            for (auto& sh : scene.shots) if (t >= sh.time - 0.01 && t < sh.time + 0.5) return juce::String ("shot");
            return juce::String ("-");
        };

        for (int pos = 0; pos + eb <= (int) scene.left.size(); pos += eb)
        {
            std::copy_n (scene.left.data() + pos, eb, buf.getWritePointer (0));
            std::copy_n (scene.right.data() + pos, eb, buf.getWritePointer (1));
            engine.process (buf, p);
            const auto& t = engine.getFootstepTrace();
            const double now = (pos + eb) / sr;
            if (t.phase == Phase::provisional && last != Phase::provisional)
                onset = now;
            if (t.phase != last && (t.phase == Phase::accepted || t.phase == Phase::rejected))
                std::printf ("%7.3f %-8s %-14s str %.2f decay %.2f tonal %.2f ctx %.2f clut %.2f hot %.2f bb %.2f mid %.2f seq %.2f score %.2f (+%.0fms)\n",
                             onset, t.phase == Phase::accepted ? "ACCEPT" : "reject", label (onset).toRawUTF8(),
                             t.strength, t.decay, t.tonal, t.context, t.clutter, t.hot, t.broadband, t.midDominance, t.sequence, t.score, (now - onset) * 1000.0);
            last = t.phase;
        }
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--diagnose")
    {
        for (int variant = 0; variant < 3; ++variant)
        {
            const bool shots = variant != 1, speech = variant != 0;
            const auto scene = makeScene (sr, 24.0, shots, speech, 1234);
            EnhEngine::Parameters p;
            p.footstep = true;
            const auto r = run (scene, sr, block, p);
            int hits = 0;
            std::printf ("\nvariant shots=%d speech=%d\n", shots, speech);
            for (auto& st : scene.steps)
            {
                const float c = maxConfidence (r, sr, block, st.time, st.time + 0.08);
                hits += c >= 0.5f;
                std::printf ("  step %.2fs conf %.2f\n", st.time, c);
                if (c < 0.5f)
                    for (int b = (int) (st.time * sr / block); b < (int) ((st.time + 0.05) * sr / block); ++b)
                    {
                        const auto& t = r.traces[(size_t) b];
                        std::printf ("     +%2.0fms str %.2f decay %.2f tonal %.2f ctx %.2f clutter %.2f hot %.2f bb %.2f mid %.2f seq %.2f score %.2f conf %.2f phase %d\n",
                                     (b * block / sr - st.time) * 1000.0, t.strength, t.decay, t.tonal, t.context, t.clutter, t.hot, t.broadband,
                                     t.midDominance, t.sequence, t.score, t.confidence, (int) t.phase);
                    }
            }
            std::printf ("  hits %d/%zu\n", hits, scene.steps.size());
            for (auto& sh : scene.shots)
            {
                float best = 0.0f; double at = 0.0;
                for (int b = (int) (sh.time * sr / block); b < (int) ((sh.time + 0.25) * sr / block) && b < (int) r.confidence.size(); ++b)
                    if (r.confidence[(size_t) b] > best) { best = r.confidence[(size_t) b]; at = b * block / sr - sh.time; }
                std::printf ("  shot %.2fs maxconf %.2f at +%.0fms\n", sh.time, best, at * 1000.0);
            }
        }
        return 0;
    }

    //==========================================================================
    for (int seed : { 1234, 999, 31337 })
    {
        std::printf ("\n== Footstep detection (synthetic game scene: ambience, steps, gunshots, voice), seed %d ==\n", seed);
        const auto scene = makeScene (sr, 24.0, true, true, seed);
        EnhEngine::Parameters p;
        p.normalize = 0.6f; p.adaptSpeed = 0.5f; p.sub = 0.3f; p.footstep = true;
        const auto r = run (scene, sr, block, p);

        int hits = 0;
        for (auto& s : scene.steps)
            hits += maxConfidence (r, sr, block, s.time, s.time + 0.08) >= 0.5f ? 1 : 0;

        int shotFalse = 0;
        for (auto& s : scene.shots)
            shotFalse += maxConfidence (r, sr, block, s.time, s.time + 0.25) >= 0.5f ? 1 : 0;

        // Voice-only time: confidence >= 0.5 with no step within 250 ms
        int voiceBlocks = 0, voiceFalse = 0;
        for (auto& seg : scene.speech)
            for (int b = (int) (seg.first * sr / block); b < (int) (seg.second * sr / block); ++b)
            {
                const double t = b * block / sr;
                bool nearStep = false;
                for (auto& s : scene.steps)
                    nearStep = nearStep || (t > s.time - 0.05 && t < s.time + 0.25);
                if (nearStep) continue;
                ++voiceBlocks;
                voiceFalse += r.confidence[(size_t) b] >= 0.5f ? 1 : 0;
            }

        const float hitRate = (float) hits / (float) std::max<size_t> (1, scene.steps.size());
        const float voiceRate = (float) voiceFalse / (float) std::max (1, voiceBlocks);

        std::printf ("  steps detected   : %d / %zu (%.0f%%)\n", hits, scene.steps.size(), 100.0f * hitRate);
        std::printf ("  gunshot false    : %d / %zu\n", shotFalse, scene.shots.size());
        std::printf ("  voice false time : %.1f%%\n", 100.0f * voiceRate);
        // v4 trades a little synthetic recall (v3: 86-100%) for rejecting crate / rattle transients
        // (v3 boosted 54-61% of crate time in the crate scene below); steps under loud speech are the misses
        check (hitRate >= 0.80f, "detects >= 80% of footsteps");
        check (shotFalse <= (int) scene.shots.size() / 5, "gunshots rarely flagged as footsteps");
        check (voiceRate <= 0.10f, "voice rarely flagged as footsteps");
        check (r.finite && r.peak <= 1.0f, "output finite and below full scale (peak " + juce::String (r.peak, 3) + ")");
    }

    //==========================================================================
    for (int seed : { 7, 4242 })
    {
        std::printf ("\n== Varied surfaces (thump, metal click, wood, gravel, distant) + gunshots + voice, seed %d ==\n", seed);
        const auto scene = makeScene (sr, 30.0, true, true, seed, true);
        EnhEngine::Parameters p;
        p.normalize = 0.6f; p.footstep = true;
        const auto r = run (scene, sr, block, p);

        int hits = 0;
        for (auto& st : scene.steps)
            hits += maxConfidence (r, sr, block, st.time, st.time + 0.08) >= 0.5f ? 1 : 0;

        int shotFalse = 0;
        for (auto& st : scene.shots)
            shotFalse += maxConfidence (r, sr, block, st.time, st.time + 0.25) >= 0.5f ? 1 : 0;

        int voiceBlocks = 0, voiceFalse = 0;
        for (auto& seg : scene.speech)
            for (int b = (int) (seg.first * sr / block); b < (int) (seg.second * sr / block); ++b)
            {
                const double t = b * block / sr;
                bool nearStep = false;
                for (auto& st : scene.steps)
                    nearStep = nearStep || (t > st.time - 0.05 && t < st.time + 0.25);
                if (nearStep) continue;
                ++voiceBlocks;
                voiceFalse += r.confidence[(size_t) b] >= 0.5f ? 1 : 0;
            }

        const float hitRate = (float) hits / (float) std::max<size_t> (1, scene.steps.size());
        const float voiceRate = (float) voiceFalse / (float) std::max (1, voiceBlocks);
        std::printf ("  steps detected   : %d / %zu (%.0f%%)\n", hits, scene.steps.size(), 100.0f * hitRate);
        std::printf ("  gunshot false    : %d / %zu\n", shotFalse, scene.shots.size());
        std::printf ("  voice false time : %.1f%%\n", 100.0f * voiceRate);
        check (hitRate >= 0.75f, "detects >= 75% of varied-surface footsteps");
        check (shotFalse <= (int) scene.shots.size() / 5, "gunshots rarely flagged");
        check (voiceRate <= 0.10f, "voice rarely flagged");
    }

    std::printf ("\n== Quiet scene: only footsteps over ambience ==\n");
    {
        const auto scene = makeScene (sr, 12.0, false, false, 77);
        EnhEngine::Parameters p;
        p.footstep = true;
        const auto r = run (scene, sr, block, p);
        int hits = 0;
        for (auto& s : scene.steps)
            hits += maxConfidence (r, sr, block, s.time, s.time + 0.08) >= 0.5f ? 1 : 0;
        std::printf ("  steps detected   : %d / %zu\n", hits, scene.steps.size());
        check ((float) hits >= 0.85f * (float) scene.steps.size(), "detects footsteps when they are the loudest sound");
    }

    //==========================================================================
    std::printf ("\n== Neutral settings on a 1 kHz tone (-12 dBFS) ==\n");
    {
        Scene s;
        const auto n = (size_t) (4.0 * sr);
        s.left.resize (n); s.right.resize (n);
        for (size_t i = 0; i < n; ++i)
            s.left[i] = s.right[i] = dbfs (-12.0f) * (float) std::sin (twoPi * 1000.0 * i / sr);

        EnhEngine::Parameters p;
        p.normalize = 0.0f; p.sub = 0.0f; p.footstep = false;
        const auto r = run (s, sr, block, p);
        const float inDb = rmsDb (s.left, n / 2, n), outDb = rmsDb (r.outL, n / 2, n);
        std::printf ("  level change     : %+.2f dB\n", outDb - inDb);
        check (std::abs (outDb - inDb) < 1.5f, "near unity at neutral settings");
    }

    //==========================================================================
    std::printf ("\n== Loudness match at full CLARITY, both modes (pink noise + tones) ==\n");
    {
        Scene s;
        const auto n = (size_t) (8.0 * sr);
        s.left.resize (n); s.right.resize (n);
        juce::Random rnd (5);
        Pink pl, pr;
        for (size_t i = 0; i < n; ++i)
        {
            const float tones = 0.2f * (float) (std::sin (twoPi * 220.0 * i / sr) + 0.5 * std::sin (twoPi * 2400.0 * i / sr));
            s.left[i] = dbfs (-18.0f) * (pl.next (rnd) + tones);
            s.right[i] = dbfs (-18.0f) * (pr.next (rnd) + tones);
        }

        for (bool add : { false, true })
        {
            EnhEngine::Parameters p;
            p.normalize = 1.0f; p.boost = add ? 1.0f : 0.0f; p.adaptSpeed = 0.6f;
            const auto r = run (s, sr, block, p);
            const float inDb = rmsDb (s.left, n / 2, n), outDb = rmsDb (r.outL, n / 2, n);
            std::printf ("  %s RMS change : %+.2f dB\n", add ? "ADD 10 " : "NORM 30", outDb - inDb);
            check (std::abs (outDb - inDb) < 3.0f, juce::String (add ? "ADD" : "NORM") + ": auto gain keeps loudness within 3 dB");
            check (r.finite && r.peak <= 1.0f, "output finite and below full scale");
        }
    }

    //==========================================================================
    std::printf ("\n== SUB on a 50 Hz tone (-24 dBFS) + pink noise ==\n");
    {
        Scene s;
        const auto n = (size_t) (6.0 * sr);
        s.left.resize (n); s.right.resize (n);
        juce::Random rnd (9);
        Pink pl;
        for (size_t i = 0; i < n; ++i)
            s.left[i] = s.right[i] = dbfs (-24.0f) * (float) std::sin (twoPi * 50.0 * i / sr) + dbfs (-30.0f) * pl.next (rnd);

        float base = 0, normal = 0, boosted = 0;
        for (int mode = 0; mode < 3; ++mode)
        {
            EnhEngine::Parameters p;
            p.normalize = 0.0f; p.sub = mode == 0 ? 0.0f : 1.0f; p.subBoost = mode == 2;
            const auto r = run (s, sr, block, p);
            const float lvl = toneDb (r.outL, sr, 50.0, n / 2, n);
            (mode == 0 ? base : mode == 1 ? normal : boosted) = lvl;
        }
        std::printf ("  50 Hz vs SUB off : SUB max %+.2f dB, SUB max + BOOST %+.2f dB\n", normal - base, boosted - base);
        check (normal - base > 2.0f, "SUB lifts the sub region");
        check (boosted - normal > 1.0f, "BOOST lifts further");
    }

    //==========================================================================
    std::printf ("\n== Audible strength on a music-like mix ==\n");
    {
        const auto music = makeMusic (sr, 12.0, 21);
        const size_t from = (size_t) (4.0 * sr), to = music.left.size();

        EnhEngine::Parameters base;
        base.normalize = 0.0f; base.adaptSpeed = 0.5f;
        const auto ref = run (music, sr, block, base);

        auto diffDb = [&] (const RunResult& r)
        {
            double d = 0, e = 0;
            for (size_t i = from; i < to; ++i)
            {
                const double x = r.outL[i] - ref.outL[i];
                d += x * x;
                e += (double) ref.outL[i] * ref.outL[i];
            }
            return (float) (10.0 * std::log10 (d / e + 1e-20));
        };

        for (float c : { 0.5f, 1.0f })
        {
            EnhEngine::Parameters p = base;
            p.normalize = c; p.boost = c;
            const auto r = run (music, sr, block, p);
            std::printf ("  ADD %4.1f: difference %+.1f dB rel.  mean |band gain| %.1f dB  max %.1f dB  loudness %+.1f dB\n",
                         c * 10.0f, diffDb (r), r.meanAbsBandGain, r.maxAbsBandGain, rmsDb (r.outL, from, to) - rmsDb (music.left, from, to));
            if (c > 0.9f)
            {
                // No fixed curve any more: on a balanced mix the change comes from harmonics + small corrections
                check (diffDb (r) > -13.0f, "ADD 10 clearly changes a balanced mix (difference > -13 dB)");
                check (r.finite && r.peak <= 1.0f, "output finite and below full scale");
            }
        }

        EnhEngine::Parameters p = base;
        p.sub = 1.0f; p.subBoost = true;
        const auto rs = run (music, sr, block, p);
        std::printf ("  SUB 100%% + BOOST: difference %+.1f dB rel.\n", diffDb (rs));
        check (diffDb (rs) > -12.0f, "SUB + BOOST changes the sound clearly");
        check (rs.finite && rs.peak <= 1.0f, "SUB + BOOST output below full scale (peak " + juce::String (rs.peak, 3) + ")");
    }

    //==========================================================================
    for (int seed : { 11, 2024 })
    {
        std::printf ("\n== Crate openings between walking (latch ring, creak, rattles, thud), seed %d ==\n", seed);
        const auto scene = makeCrateScene (sr, 40.0, seed);
        EnhEngine::Parameters p;
        p.normalize = 0.6f; p.footstep = true;
        const auto r = run (scene, sr, block, p);

        int hits = 0;
        for (auto& st : scene.steps)
            hits += maxConfidence (r, sr, block, st.time, st.time + 0.08) >= 0.5f ? 1 : 0;

        int flagged = 0, liftBlocks = 0, crateBlocks = 0, sustained = 0;
        for (auto& c : scene.crates)
        {
            flagged += maxConfidence (r, sr, block, c.time, c.time + 1.1) >= 0.5f ? 1 : 0;
            int run = 0, longest = 0;
            for (int b = (int) (c.time * sr / block); b < (int) ((c.time + 1.1) * sr / block) && b < (int) r.confidence.size(); ++b)
            {
                ++crateBlocks;
                const bool lifted = r.confidence[(size_t) b] >= 0.3f;
                liftBlocks += lifted ? 1 : 0;
                run = lifted ? run + 1 : 0;
                longest = std::max (longest, run);
            }
            sustained += longest * block / sr > 0.15 ? 1 : 0;   // lifted for longer than a brief blip
        }

        const float hitRate = (float) hits / (float) std::max<size_t> (1, scene.steps.size());
        const float crateRate = (float) flagged / (float) std::max<size_t> (1, scene.crates.size());
        const float liftTime = (float) liftBlocks / (float) std::max (1, crateBlocks);
        std::printf ("  steps detected   : %d / %zu (%.0f%%)\n", hits, scene.steps.size(), 100.0f * hitRate);
        std::printf ("  crates flagged   : %d / %zu (%.0f%%), lifted > 150 ms: %d, lifted time during crates %.1f%%\n",
                     flagged, scene.crates.size(), 100.0f * crateRate, sustained, 100.0f * liftTime);
        check (hitRate >= 0.80f, "detects >= 80% of footsteps around crates");
        check (crateRate <= 0.40f, "most crate openings never flagged (rummage-only crates may blip once)");
        check (sustained == 0, "no crate gets a sustained footstep lift");
        check (liftTime <= 0.05f, "footstep lift active < 5% of crate time (v3: 54-61%)");
    }

    //==========================================================================
    std::printf ("\n== AutoEQ follows the source (CLARITY 100%%, steady state) ==\n");
    {
        using enh::dsp::BandAnalyzer;
        using enh::dsp::numBands;
        const auto n = (size_t) (10.0 * sr);

        auto coloured = [&] (int kind)
        {
            Scene s;
            s.left.resize (n); s.right.resize (n);
            juce::Random rnd (100 + kind);
            Pink pl, pr;
            auto lp = BiquadCoeffs::highShelf (sr, 1500.0, 0.7071, -14.0), hp = BiquadCoeffs::lowShelf (sr, 400.0, 0.7071, -14.0);
            auto res = BiquadCoeffs::bandPass (sr, 600.0, 4.0), notch = BiquadCoeffs::peaking (sr, 1500.0, 1.5, -12.0);
            BiquadState a1, a2, b1, b2, c1, c2;
            for (size_t i = 0; i < n; ++i)
            {
                float l = pl.next (rnd), r = pr.next (rnd);
                switch (kind)
                {
                    case 1: l = a1.process (lp, l); r = b1.process (lp, r); break;   // muffled (e.g. through a wall)
                    case 2: l = a1.process (hp, l); r = b1.process (hp, r); break;   // thin (no low end)
                    case 3: l += c1.process (res, l) * 3.0f; r += c2.process (res, r) * 3.0f; break;
                    case 4: l = c1.process (notch, l); r = c2.process (notch, r); break;
                    default: break;
                }
                s.left[i] = dbfs (-20.0f) * l;
                s.right[i] = dbfs (-20.0f) * r;
            }
            return s;
        };

        auto at = [] (const RunResult& r, double hz)
        {
            int best = 0;
            for (int k = 1; k < numBands; ++k)
                if (std::abs (std::log (BandAnalyzer::centreHz (k) / hz)) < std::abs (std::log (BandAnalyzer::centreHz (best) / hz)))
                    best = k;
            return (float) r.curve[(size_t) best];
        };
        auto mean = [] (const RunResult& r, double lo, double hi)
        {
            double sum = 0; int c = 0;
            for (int k = 0; k < numBands; ++k)
                if (BandAnalyzer::centreHz (k) >= lo && BandAnalyzer::centreHz (k) <= hi) { sum += r.curve[(size_t) k]; ++c; }
            return (float) (sum / std::max (1, c));
        };
        auto print = [&] (const char* name, const RunResult& r)
        {
            std::printf ("  %-10s", name);
            for (int k = 0; k < numBands; k += 2)
                std::printf (" %+5.1f", r.curve[(size_t) k]);
            std::printf ("   treble %+5.1f bass %+5.1f\n", r.treble, r.bass);
        };

        EnhEngine::Parameters p;
        p.normalize = 1.0f; p.adaptSpeed = 0.5f;

        std::printf ("  %-10s", "Hz");
        for (int k = 0; k < numBands; k += 2)
            std::printf (" %5.0f", BandAnalyzer::centreHz (k));
        std::printf ("\n");

        float solverError = 0.0f;
        const auto pink  = run (coloured (0), sr, block, p);
        const auto dull  = run (coloured (1), sr, block, p);
        const auto thin  = run (coloured (2), sr, block, p);
        const auto reso  = run (coloured (3), sr, block, p, [&] (const EnhEngine& e)
        {
            // Applied response of the actual 24-filter cascade vs. the displayed curve
            for (int i = 0; i < numBands; ++i)
            {
                double db = 0.0;
                for (int j = 0; j < numBands; ++j)
                {
                    const auto c = BiquadCoeffs::peaking (sr, BandAnalyzer::centreHz (j), enh::dsp::AdaptiveEQ::filterQ, e.getEQ().getFilterGainDb (j));
                    const double w = twoPi * BandAnalyzer::centreHz (i) / sr;
                    const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
                    db += 20.0 * std::log10 (std::abs ((double) c.b0 + (double) c.b1 * z1 + (double) c.b2 * z2)
                                             / std::abs (1.0 + (double) c.a1 * z1 + (double) c.a2 * z2));
                }
                solverError = std::max (solverError, (float) std::abs (db - e.getEQ().getGainDb (i)));
            }
        });
        const auto hole  = run (coloured (4), sr, block, p);
        const auto music = run (makeMusic (sr, 12.0, 21), sr, block, p);

        print ("pink", pink); print ("dull", dull); print ("thin", thin);
        print ("600Hz res", reso); print ("1.5k hole", hole); print ("music", music);

        float maxPink = 0.0f;
        for (auto v : pink.curve) maxPink = std::max (maxPink, (float) std::abs (v));
        const float dullTilt = mean (dull, 3000, 12000) - mean (dull, 80, 400);
        const float thinTilt = mean (thin, 3000, 12000) - mean (thin, 80, 400);
        const float resoDip = at (reso, 600) - 0.5f * (at (reso, 250) + at (reso, 1500));
        const float holeLift = at (hole, 1500) - 0.5f * (at (hole, 600) + at (hole, 4000));

        double diff = 0;
        for (int k = 0; k < numBands; ++k) diff += std::pow (dull.curve[(size_t) k] - thin.curve[(size_t) k], 2.0);
        const float curveDiff = (float) std::sqrt (diff / numBands);

        std::printf ("  pink max |gain| %.1f dB; dull hi-lo %+.1f dB; thin hi-lo %+.1f dB; 600 Hz resonance %+.1f dB; 1.5 kHz hole %+.1f dB\n",
                     maxPink, dullTilt, thinTilt, resoDip, holeLift);
        std::printf ("  1-4 kHz mean: pink %+.1f dB, music %+.1f dB; dull vs thin curve RMS difference %.1f dB; cascade vs display max error %.2f dB\n",
                     mean (pink, 1000, 4000), mean (music, 1000, 4000), curveDiff, solverError);

        check (maxPink <= 2.0f, "flat (pink) source gets a nearly flat EQ - no built-in smile curve");
        check (dullTilt > 1.5f, "muffled source: highs lifted relative to lows");
        check (thinTilt < -1.0f, "thin/bright source: the opposite tilt");
        check (curveDiff > 2.0f, "different sources produce clearly different curves");
        check (resoDip < -1.2f, "cuts a resonance where it is (600 Hz, within the midrange cut limit)");
        check (holeLift > 1.5f, "fills a hole where it is (1.5 kHz)");
        check (mean (pink, 1000, 4000) > -1.0f && mean (music, 1000, 4000) > -1.0f, "does not scoop the 1-4 kHz footstep/harmonic range");
        check (solverError < 1.0f, "applied filter response matches the displayed curve (< 1 dB)");
    }

    //==========================================================================
    std::printf ("\n== CLARITY harmonics follow the source ==\n");
    {
        const auto n = (size_t) (6.0 * sr);
        auto toneScene = [&] (double hz, float levelDb)
        {
            Scene s;
            s.left.resize (n); s.right.resize (n);
            juce::Random rnd (3);
            Pink pk;
            for (size_t i = 0; i < n; ++i)
                s.left[i] = s.right[i] = dbfs (levelDb) * (float) std::sin (twoPi * hz * i / sr) + dbfs (levelDb - 30.0f) * pk.next (rnd);
            return s;
        };

        struct Probe { double hz; float level; float depthHz = 0, clarityHz = 0, harm2 = 0, harm3 = 0, harm2Off = 0; };
        std::vector<Probe> probes { { 220.0, -18.0f }, { 1000.0, -18.0f }, { 2500.0, -18.0f }, { 1000.0, -42.0f } };

        for (auto& pr : probes)
        {
            const auto scene = toneScene (pr.hz, pr.level);
            EnhEngine::Parameters p;
            p.adaptSpeed = 0.5f;

            p.normalize = 0.0f;
            const auto off = run (scene, sr, block, p);
            p.normalize = 1.0f; p.boost = 1.0f;   // ADD mode at 10
            const auto on = run (scene, sr, block, p, [&] (const EnhEngine& e)
            {
                pr.depthHz = e.getHarmonicPlan().depth.hz;
                pr.clarityHz = e.getHarmonicPlan().clarity.hz;
            });

            const float fundamental = toneDb (on.outL, sr, pr.hz, n / 2, n);
            pr.harm2 = toneDb (on.outL, sr, 2 * pr.hz, n / 2, n) - fundamental;
            pr.harm3 = toneDb (on.outL, sr, 3 * pr.hz, n / 2, n) - fundamental;
            pr.harm2Off = toneDb (off.outL, sr, 2 * pr.hz, n / 2, n) - toneDb (off.outL, sr, pr.hz, n / 2, n);
            std::printf ("  tone %5.0f Hz @ %3.0f dBFS: depth centre %5.0f Hz, clarity centre %5.0f Hz, 2nd %+.1f dB (off %+.1f), 3rd %+.1f dB\n",
                         pr.hz, pr.level, pr.depthHz, pr.clarityHz, pr.harm2, pr.harm2Off, pr.harm3);
        }

        {
            // NORM 30 must only normalise: no generated harmonics
            const auto scene = toneScene (1000.0, -18.0f);
            EnhEngine::Parameters p;
            p.normalize = 1.0f; p.boost = 0.0f;
            const auto r = run (scene, sr, block, p);
            const float h2 = toneDb (r.outL, sr, 2000.0, n / 2, n) - toneDb (r.outL, sr, 1000.0, n / 2, n);
            std::printf ("  NORM 30 on the 1 kHz tone: 2nd %+.1f dB\n", h2);
            check (h2 < -40.0f, "NORM mode adds no harmonics");
        }

        auto near = [] (float a, double b) { return std::abs (std::log2 (a / b)) < 0.5; };
        check (near (probes[0].depthHz, 220.0), "DEPTH centres on the 220 Hz source");
        check (near (probes[1].clarityHz, 1000.0) && near (probes[2].clarityHz, 2500.0), "CLARITY centre moves with the source (1 kHz vs 2.5 kHz)");
        check (probes[1].harm2 > -35.0f && probes[1].harm2 - probes[1].harm2Off > 15.0f, "CLARITY adds a real 2nd harmonic to a 1 kHz source");
        check (probes[2].harm2 > -35.0f, "CLARITY adds a real 2nd harmonic to a 2.5 kHz source");
        check (std::abs (probes[3].harm2 - probes[1].harm2) < 6.0f, "harmonic amount is level-independent (-18 vs -42 dBFS within 6 dB)");
    }

    //==========================================================================
    std::printf ("\n== ADD: harmonics coupling (rich source keeps a lift, thinning harmonics are topped up) ==\n");
    {
        const auto n = (size_t) (8.0 * sr);
        Scene sc;
        sc.left.resize (n); sc.right.resize (n);
        juce::Random rnd (4);
        Pink pk;
        double phase = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            // 0-4 s: harmonically rich tone (1 kHz + 2nd..5th); 4-8 s: the harmonics fade out over 0.5 s
            const double t = i / sr;
            phase += 1000.0 / sr;
            const float fade = (float) juce::jlimit (0.0, 1.0, 1.0 - (t - 4.0) / 0.5);
            double v = std::sin (twoPi * phase);
            for (int h = 2; h <= 5; ++h)
                v += fade * 0.5 / h * std::sin (twoPi * phase * h);
            sc.left[i] = sc.right[i] = dbfs (-20.0f) * (float) v + dbfs (-50.0f) * pk.next (rnd);
        }

        EnhEngine engine;
        engine.prepare (sr, block, 2);
        EnhEngine::Parameters p;
        p.normalize = 1.0f; p.boost = 1.0f; p.adaptSpeed = 0.5f;
        juce::AudioBuffer<float> buf (2, block);
        float needRich = 1.0f, liftRich = 0.0f, rescueMax = 0.0f, amountAfter = 0.0f;
        int liftBand = 0;
        for (int k = 1; k < enh::dsp::numBands; ++k)
            if (std::abs (std::log (enh::dsp::BandAnalyzer::centreHz (k) / 1000.0)) < std::abs (std::log (enh::dsp::BandAnalyzer::centreHz (liftBand) / 1000.0)))
                liftBand = k;

        for (size_t pos = 0; pos + (size_t) block <= n; pos += (size_t) block)
        {
            std::copy_n (sc.left.data() + pos, block, buf.getWritePointer (0));
            std::copy_n (sc.right.data() + pos, block, buf.getWritePointer (1));
            engine.process (buf, p);
            const double t = (double) pos / sr;
            const auto& plan = engine.getHarmonicPlan().clarity;
            if (t > 3.0 && t < 4.0)
            {
                needRich = std::min (needRich, plan.need);
                liftRich = std::max (liftRich, engine.getEQ().getGainDb (liftBand));
            }
            if (t > 4.0 && t < 5.5)
            {
                rescueMax = std::max (rescueMax, plan.rescue);
                amountAfter = std::max (amountAfter, plan.amount);
            }
        }

        std::printf ("  rich source: harmonic need %.2f, EQ lift near 1 kHz %+.1f dB\n", needRich, liftRich);
        std::printf ("  harmonics fading: rescue %.2f, exciter drive %.2f\n", rescueMax, amountAfter);
        check (liftRich > 3.0f, "harmonically rich source still gets a clarity lift in ADD mode");
        check (rescueMax > 0.3f, "falling harmonics are detected and topped up by the exciter");
    }

    //==========================================================================
    std::printf ("\n== SERAPH: SILK (tone & texture) ==\n");
    {
        using enh::dsp::Seraph;
        const auto n = (size_t) (6.0 * sr);

        auto runSeraph = [&] (std::vector<float> l, std::vector<float> r, const Seraph::Settings& st, Seraph* keep = nullptr)
        {
            juce::ScopedNoDenormals noDenormals;
            Seraph local;
            auto& unit = keep != nullptr ? *keep : local;
            unit.prepare (sr);
            for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
            {
                const int len = (int) std::min ((size_t) block, l.size() - pos);
                float* ch[2] { l.data() + pos, r.data() + pos };
                unit.process (ch, 2, len, st);
            }
            return std::make_pair (std::move (l), std::move (r));
        };
        auto pink = [&] (size_t len, float db, int seed)
        {
            std::vector<float> v (len);
            juce::Random rnd (seed);
            Pink pk;
            for (auto& x : v) x = dbfs (db) * pk.next (rnd);
            return v;
        };
        auto bandDb = [&] (const std::vector<float>& x, double hz, double q)
        {
            auto c = BiquadCoeffs::bandPass (sr, hz, q);
            BiquadState a, b;
            double e = 0.0;
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float y = b.process (c, a.process (c, x[i]));
                if (i > x.size() / 3) e += (double) y * y;
            }
            return (float) (10.0 * std::log10 (e / (double) (x.size() * 2 / 3) + 1e-20));
        };
        auto silkOnly = [] (float smooth, float air, float warmth, float body)
        {
            Seraph::Settings st;
            st.mode = Seraph::silkOnly;
            st.silk.smooth = smooth; st.silk.air = air; st.silk.warmth = warmth; st.silk.body = body;
            st.silk.protect = false; st.silk.autoGain = false;
            return st;
        };

        {
            Seraph::Settings st; st.mode = Seraph::off;
            const auto in = pink (n, -20.0f, 1);
            check (runSeraph (in, in, st).first == in, "OFF: bit-exact passthrough");
        }

        // SMOOTH dips a resonance, leaves the rest alone
        {
            auto in = pink (n, -24.0f, 2);
            auto res = BiquadCoeffs::bandPass (sr, 3150.0, 8.0);
            BiquadState r1;
            for (auto& x : in) x += 4.0f * r1.process (res, x);
            const auto out = runSeraph (in, in, silkOnly (10.0f, 0.0f, 0.0f, 0.0f)).first;
            const float dRes = bandDb (out, 3150.0, 8.0) - bandDb (in, 3150.0, 8.0);
            const float d1k = bandDb (out, 1000.0, 2.0) - bandDb (in, 1000.0, 2.0);
            std::printf ("  SMOOTH 10 on a +12 dB resonance at 3.15 kHz: resonance %+.1f dB, 1 kHz region %+.1f dB\n", dRes, d1k);
            check (dRes < -4.0f, "SMOOTH dips a ringing resonance (> 4 dB)");
            check (std::abs (d1k) < 1.5f, "...and leaves the neighbouring spectrum alone (< 1.5 dB)");

            const auto flatIn = pink (n, -24.0f, 3);
            const auto flatOut = runSeraph (flatIn, flatIn, silkOnly (10.0f, 0.0f, 0.0f, 0.0f)).first;
            float worst = 0.0f;
            for (double hz : { 250.0, 1000.0, 3000.0, 8000.0 })
                worst = std::max (worst, std::abs (bandDb (flatOut, hz, 2.0) - bandDb (flatIn, hz, 2.0)));
            std::printf ("  SMOOTH 10 on plain pink noise: largest regional change %.1f dB\n", worst);
            check (worst < 1.5f, "SMOOTH is not a fixed EQ: a smooth spectrum stays as it is");
        }

        // PROTECT keeps attacks while the steady resonance is still dipped
        {
            std::vector<float> in (n, 0.0f);
            juce::Random rnd (5);
            auto bp = BiquadCoeffs::bandPass (sr, 2500.0, 3.0);
            BiquadState b1, b2;
            std::vector<size_t> onsets;
            for (size_t i = 0; i < n; ++i)
            {
                in[i] = dbfs (-26.0f) * (float) std::sin (twoPi * 2500.0 * i / sr) + dbfs (-50.0f) * (rnd.nextFloat() * 2 - 1);
                const size_t k = i % (size_t) (0.5 * sr);
                if (k == 0) onsets.push_back (i);
                in[i] += dbfs (-24.0f) * 3.0f * b2.process (bp, b1.process (bp, rnd.nextFloat() * 2 - 1)) * (float) std::exp (-(double) k / (0.02 * sr));
            }
            auto onsetPeak = [&] (const std::vector<float>& x)
            {
                double sum = 0.0; int count = 0;
                for (auto o : onsets)
                    if (o > (size_t) (2.0 * sr))
                    {
                        float pk = 0.0f;
                        for (size_t i = o; i < o + (size_t) (0.008 * sr) && i < x.size(); ++i) pk = std::max (pk, std::abs (x[i]));
                        sum += 20.0 * std::log10 (pk + 1e-9); ++count;
                    }
                return (float) (sum / std::max (1, count));
            };
            auto st = silkOnly (10.0f, 0.0f, 0.0f, 0.0f);
            const float lossOff = onsetPeak (runSeraph (in, in, st).first) - onsetPeak (in);
            st.silk.protect = true;
            const float loss = onsetPeak (runSeraph (in, in, st).first) - onsetPeak (in);
            std::printf ("  footstep-like attacks over a whistling 2.5 kHz tone, SMOOTH 10: attack peak %+.1f dB (PROTECT off) vs %+.1f dB (on)\n", lossOff, loss);
            // Zero latency: the first ~1 ms of an attack passes an existing dip before any detector can react
            check (loss > -3.5f, "PROTECT keeps attack peaks under a dipped resonance (within 3.5 dB)");
            check (loss > lossOff + 4.0f, "PROTECT recovers most of what the dip would take");
        }

        // AIR adapts to how dull the source is
        {
            const auto bright = pink (n, -24.0f, 6);
            auto dull = bright;
            auto shelf = BiquadCoeffs::highShelf (sr, 4000.0, 0.7071, -14.0);
            BiquadState sh;
            for (auto& x : dull) x = sh.process (shelf, x);
            const float liftBright = bandDb (runSeraph (bright, bright, silkOnly (0.0f, 10.0f, 0.0f, 0.0f)).first, 12000.0, 2.0) - bandDb (bright, 12000.0, 2.0);
            const float liftDull = bandDb (runSeraph (dull, dull, silkOnly (0.0f, 10.0f, 0.0f, 0.0f)).first, 12000.0, 2.0) - bandDb (dull, 12000.0, 2.0);
            std::printf ("  AIR 10: 12 kHz lift on a bright source %+.1f dB, on a dull source %+.1f dB\n", liftBright, liftDull);
            check (liftDull > 1.5f, "AIR opens up a dull source");
            check (liftDull > liftBright + 1.0f, "AIR gives dull sources more than bright ones");
        }

        // WARMTH: harmonics in the low mids
        {
            auto tone = [&] (double hz) { std::vector<float> v (n); for (size_t i = 0; i < n; ++i) v[i] = dbfs (-18.0f) * (float) std::sin (twoPi * hz * i / sr); return v; };
            auto h2 = [&] (const std::vector<float>& x, double hz) { return toneDb (x, sr, 2 * hz, n / 2, n) - toneDb (x, sr, hz, n / 2, n); };
            const auto low = tone (250.0);
            const float cold = h2 (runSeraph (low, low, silkOnly (0.0f, 0.0f, 0.0f, 0.0f)).first, 250.0);
            const float warm = h2 (runSeraph (low, low, silkOnly (0.0f, 0.0f, 10.0f, 0.0f)).first, 250.0);
            std::printf ("  WARMTH on a 250 Hz tone: 2nd harmonic %.1f dB (0) -> %.1f dB (10)\n", cold, warm);
            check (warm > cold + 8.0f && warm < -12.0f, "WARMTH adds real low-mid harmonics, still tasteful");
        }

        // Live display: activity is measured per channel
        {
            Seraph unit;
            auto st = silkOnly (10.0f, 10.0f, 10.0f, 0.0f);
            auto l = pink (n, -20.0f, 11), r = pink (n, -20.0f, 12);
            auto res = BiquadCoeffs::bandPass (sr, 3150.0, 8.0);
            BiquadState rs;
            for (auto& x : r) x += 4.0f * rs.process (res, x);   // a resonance only on the right
            juce::ScopedNoDenormals noDenormals;
            unit.prepare (sr);
            for (size_t pos = 0; pos < n; pos += (size_t) block)
            {
                float* chp[2] { l.data() + pos, r.data() + pos };
                unit.process (chp, 2, (int) std::min ((size_t) block, n - pos), st);
            }
            const float sL = unit.getActivityDb (Seraph::smoothAct, 0), sR = unit.getActivityDb (Seraph::smoothAct, 1);
            std::printf ("  live display, resonance only on the right: SMOOTH activity L %.1f dB, R %.1f dB\n", sL, sR);
            check (sR > sL + 3.0f, "the display shows SMOOTH working harder on the channel that needs it");

            Seraph quiet;
            auto l2 = pink (n, -20.0f, 13);
            std::vector<float> r2 (n, 0.0f);
            quiet.prepare (sr);
            for (size_t pos = 0; pos < n; pos += (size_t) block)
            {
                float* chp[2] { l2.data() + pos, r2.data() + pos };
                quiet.process (chp, 2, (int) std::min ((size_t) block, n - pos), st);
            }
            const float wL = quiet.getActivityDb (Seraph::warmthAct, 0), wR = quiet.getActivityDb (Seraph::warmthAct, 1);
            std::printf ("  music only on the left: WARMTH activity L %.1f dB, R %.1f dB\n", wL, wR);
            check (wL > -50.0f && wR <= -59.0f, "a silent channel shows no activity");
        }

        // AUTO keeps loudness
        {
            const auto in = pink (n, -20.0f, 7);
            auto st = silkOnly (10.0f, 10.0f, 10.0f, 10.0f);
            st.silk.autoGain = true; st.silk.tape = true;
            const auto out = runSeraph (in, in, st).first;
            const float d = rmsDb (out, n / 2, n) - rmsDb (in, n / 2, n);
            std::printf ("  everything at 10 + TAPE, AUTO on: loudness %+.2f dB\n", d);
            check (std::abs (d) < 1.5f, "AUTO keeps the level matched (within 1.5 dB)");
        }
    }

    //==========================================================================
    std::printf ("\n== SERAPH: HALO (space & width) ==\n");
    {
        using enh::dsp::Seraph;

        auto haloOnly = [] ()
        {
            Seraph::Settings st;
            st.mode = Seraph::heaven;
            st.silk.smooth = st.silk.air = st.silk.warmth = st.silk.body = 0.0f;
            st.silk.autoGain = false;
            st.halo.space = 0.0f; st.halo.shimmer = 0.0f; st.halo.width = 1.0f;
            return st;
        };
        auto process = [&] (std::vector<float>& l, std::vector<float>& r, const Seraph::Settings& st, Seraph& unit)
        {
            juce::ScopedNoDenormals noDenormals;
            unit.prepare (sr);
            for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
            {
                const int len = (int) std::min ((size_t) block, l.size() - pos);
                float* ch[2] { l.data() + pos, r.data() + pos };
                unit.process (ch, 2, len, st);
            }
        };
        const auto n = (size_t) (6.0 * sr);

        // WIDTH on mono material: wider, and the mono sum is unchanged
        {
            std::vector<float> in (n);
            juce::Random rnd (8);
            Pink pk;
            for (auto& x : in) x = dbfs (-20.0f) * pk.next (rnd);
            auto l = in, r = in;
            auto st = haloOnly();
            st.halo.width = 2.0f;
            Seraph unit;
            process (l, r, st, unit);
            std::vector<float> mid (n), side (n);
            for (size_t i = 0; i < n; ++i) { mid[i] = 0.5f * (l[i] + r[i]); side[i] = 0.5f * (l[i] - r[i]); }
            const float monoChange = rmsDb (mid, n / 2, n) - rmsDb (in, n / 2, n);
            const float sideRel = rmsDb (side, n / 2, n) - rmsDb (mid, n / 2, n);
            std::printf ("  WIDTH 200%% on mono: mono sum %+.2f dB, side %.1f dB below mid\n", monoChange, -sideRel);
            check (std::abs (monoChange) < 0.5f, "widening never changes the mono sum");
            check (sideRel > -20.0f, "mono material gains real width");
        }

        // BASS MONO
        {
            std::vector<float> l (n), r (n);
            for (size_t i = 0; i < n; ++i) { l[i] = dbfs (-20.0f) * (float) std::sin (twoPi * 60.0 * i / sr); r[i] = -l[i]; }
            const auto inSide = l;
            auto st = haloOnly();
            Seraph unit;
            process (l, r, st, unit);
            std::vector<float> side (n);
            for (size_t i = 0; i < n; ++i) side[i] = 0.5f * (l[i] - r[i]);
            const float d = toneDb (side, sr, 60.0, n / 2, n) - toneDb (inSide, sr, 60.0, n / 2, n);
            std::printf ("  BASS MONO: 60 Hz side content %+.1f dB\n", d);
            check (d < -20.0f, "BASS MONO centres the low end");
        }

        // SPACE / DECAY: an impulse rings out close to the set decay time
        {
            for (float decay : { 1.0f, 4.0f })
            {
                const auto len = (size_t) ((decay * 2.0f + 1.0f) * sr);
                std::vector<float> l (len, 0.0f), r (len, 0.0f);
                l[100] = r[100] = 0.5f;
                auto st = haloOnly();
                st.halo.space = 1.0f; st.halo.decayS = decay; st.halo.duck = false; st.halo.mod = false; st.halo.tone = 1.0f;
                Seraph unit;
                process (l, r, st, unit);
                // Schroeder backward integration, T20 extrapolated to 60 dB
                std::vector<double> energy (len, 0.0);
                double acc = 0.0;
                for (size_t i = len; i-- > 0;) { acc += (double) l[i] * l[i] + (double) r[i] * r[i]; energy[i] = acc; }
                const double e0 = energy[(size_t) (0.1 * sr)];
                size_t t5 = 0, t25 = 0;
                for (size_t i = (size_t) (0.1 * sr); i < len; ++i)
                {
                    const double db = 10.0 * std::log10 (energy[i] / e0 + 1e-30);
                    if (t5 == 0 && db < -5.0) t5 = i;
                    if (t25 == 0 && db < -25.0) { t25 = i; break; }
                }
                const float rt60 = t25 > t5 ? (float) ((double) (t25 - t5) / sr * 3.0) : 0.0f;
                std::printf ("  DECAY %.1f s: measured RT60 %.2f s\n", decay, rt60);
                check (rt60 > decay * 0.6f && rt60 < decay * 1.5f, "tail decays close to DECAY (" + juce::String (decay, 1) + " s)");
            }
        }

        // SHIMMER + long DECAY stays stable and dies away after the music stops
        {
            const auto len = (size_t) (20.0 * sr);
            std::vector<float> l (len, 0.0f), r (len, 0.0f);
            juce::Random rnd (9);
            Pink pk;
            for (size_t i = 0; i < (size_t) (5.0 * sr); ++i) l[i] = r[i] = dbfs (-12.0f) * pk.next (rnd);
            auto st = haloOnly();
            st.halo.space = 1.0f; st.halo.decayS = 8.0f; st.halo.shimmer = 1.0f; st.halo.duck = false;
            Seraph unit;
            process (l, r, st, unit);
            bool finite = true; float peak = 0.0f;
            for (size_t i = 0; i < len; ++i) { finite = finite && std::isfinite (l[i]) && std::isfinite (r[i]); peak = std::max (peak, std::abs (l[i])); }
            const float early = rmsDb (l, (size_t) (6.0 * sr), (size_t) (8.0 * sr)), late = rmsDb (l, (size_t) (18.0 * sr), len);
            std::printf ("  SHIMMER 10, DECAY 8 s: peak %.2f, tail %.1f dB at 6-8 s -> %.1f dB at 18-20 s\n", peak, early, late);
            check (finite && peak < 2.0f, "shimmer feedback stays stable");
            check (late < early - 6.0f, "the tail dies away instead of building up");
        }

        // DUCK: the tail steps back while the music plays
        {
            std::vector<float> base (n);
            juce::Random rnd (10);
            Pink pk;
            for (size_t i = 0; i < n; ++i)
            {
                // Speech-like bursts: 300 ms on, 200 ms off
                const bool onPhase = std::fmod ((double) i / sr, 0.5) < 0.3;
                base[i] = onPhase ? dbfs (-18.0f) * pk.next (rnd) : 0.0f;
            }
            auto measure = [&] (bool duck)
            {
                auto l = base, r = base;
                auto st = haloOnly();
                st.halo.space = 1.0f; st.halo.decayS = 2.0f; st.halo.duck = duck;
                Seraph unit;
                juce::ScopedNoDenormals noDenormals;
                unit.prepare (sr);
                double sum = 0.0; int count = 0;
                for (size_t pos = 0; pos < n; pos += (size_t) block)
                {
                    float* ch[2] { l.data() + pos, r.data() + pos };
                    unit.process (ch, 2, (int) std::min ((size_t) block, n - pos), st);
                    const double t = (double) pos / sr;
                    if (t > 2.0 && std::fmod (t, 0.5) > 0.1 && std::fmod (t, 0.5) < 0.28)
                    {
                        sum += unit.getHalo().getHaloDb();
                        ++count;
                    }
                }
                return (float) (sum / std::max (1, count));
            };
            const float free = measure (false), ducked = measure (true);
            std::printf ("  DUCK: tail under the music %.1f dB (off) vs %.1f dB (on)\n", free, ducked);
            check (ducked < free - 4.0f, "DUCK keeps the tail out of the way while the music plays");
        }
    }

    //==========================================================================
    std::printf ("\n== MULTIPLY and STRENGTH ==\n");
    {
        using enh::dsp::KnobValues;
        using enh::dsp::Seraph;

        // Mapping: every knob scales, SERAPH OUTPUT does not, and everything is clamped
        {
            KnobValues k;
            k.clarityNorm = 20.0f; k.adaptPercent = 40.0f; k.subPercent = 50.0f; k.enhMultiply = 1.5f;
            k.space = 2.0f; k.outputDb = 3.0f; k.decayS = 2.0f; k.seraphMultiply = 1.5f;
            const auto p = enh::dsp::mapKnobs (k);
            std::printf ("  x1.5: CLARITY 20 -> %.2f of full, ADAPT 40%% -> %.2f, SUB 50%% -> %.2f, SPACE 2 -> %.2f, DECAY 2 s -> %.1f s, OUTPUT %+.1f dB\n",
                         p.normalize, p.adaptSpeed, p.sub, p.seraph.halo.space, p.seraph.halo.decayS, p.seraph.silk.outputDb);
            check (std::abs (p.normalize - 1.0f) < 1.0e-4f && std::abs (p.adaptSpeed - 0.6f) < 1.0e-4f && std::abs (p.sub - 0.75f) < 1.0e-4f,
                   "ENH MULTIPLY scales every ENH knob");
            check (std::abs (p.seraph.halo.space - 0.3f) < 1.0e-4f && std::abs (p.seraph.halo.decayS - 3.0f) < 1.0e-4f,
                   "SERAPH MULTIPLY scales SERAPH knobs");
            check (p.seraph.silk.outputDb == 3.0f, "SERAPH MULTIPLY leaves the OUTPUT gain alone");

            k.enhMultiply = k.seraphMultiply = 9.0f; k.enhStrength = k.seraphStrength = 9.0f;
            const auto q = enh::dsp::mapKnobs (k);
            check (q.strength == 5.0f && q.seraph.silk.strength == 5.0f && q.normalize == 2.0f, "MULTIPLY caps at 3x, STRENGTH at 5");
        }

        // ENH STRENGTH scales the applied EQ curve
        {
            const auto n = (size_t) (10.0 * sr);
            Scene sc;
            sc.left.resize (n); sc.right.resize (n);
            juce::Random rnd (21);
            Pink pk;
            auto res = BiquadCoeffs::bandPass (sr, 600.0, 4.0);
            BiquadState r1;
            for (size_t i = 0; i < n; ++i)
            {
                const float x = dbfs (-20.0f) * pk.next (rnd);
                sc.left[i] = sc.right[i] = x + 3.0f * r1.process (res, x);
            }
            auto curveSpan = [&] (float strength)
            {
                EnhEngine::Parameters p;
                p.normalize = 1.0f; p.strength = strength;
                const auto r = run (sc, sr, block, p);
                float lo = 0.0f, hi = 0.0f;
                for (auto v : r.curve) { lo = std::min (lo, (float) v); hi = std::max (hi, (float) v); }
                return hi - lo;
            };
            const float s0 = curveSpan (0.0f), s1 = curveSpan (1.0f), s2 = curveSpan (2.0f);
            std::printf ("  ENH EQ curve span: STRENGTH 0 %.2f dB, 1 %.2f dB, 2 %.2f dB\n", s0, s1, s2);
            check (s0 < 0.1f, "ENH STRENGTH 0: no EQ movement");
            check (s2 > 1.6f * s1 && s2 < 2.4f * s1, "ENH STRENGTH 2: about twice the EQ movement");
        }

        // SERAPH STRENGTH 0 leaves the audio alone; higher strength does more
        {
            const auto n = (size_t) (6.0 * sr);
            std::vector<float> in (n);
            juce::Random rnd (22);
            Pink pk;
            for (auto& x : in) x = dbfs (-20.0f) * pk.next (rnd);

            auto runWith = [&] (float strength, Seraph& unit)
            {
                Seraph::Settings st;
                st.mode = Seraph::heaven;
                st.silk.strength = st.halo.strength = strength;
                st.silk.autoGain = false; st.silk.tape = true;
                auto l = in, r = in;
                juce::ScopedNoDenormals noDenormals;
                unit.prepare (sr);
                for (size_t pos = 0; pos < n; pos += (size_t) block)
                {
                    float* chp[2] { l.data() + pos, r.data() + pos };
                    unit.process (chp, 2, (int) std::min ((size_t) block, n - pos), st);
                }
                return l;
            };

            Seraph u0, u1, u3;
            const auto out0 = runWith (0.0f, u0);
            std::vector<float> diff (n);
            for (size_t i = 0; i < n; ++i) diff[i] = out0[i] - in[i];
            const float residual = rmsDb (diff, n / 2, n) - rmsDb (in, n / 2, n);
            runWith (1.0f, u1);
            runWith (3.0f, u3);
            const float space1 = u1.getActivityDb (Seraph::spaceAct, 0), space3 = u3.getActivityDb (Seraph::spaceAct, 0);
            const float warm1 = u1.getActivityDb (Seraph::warmthAct, 0), warm3 = u3.getActivityDb (Seraph::warmthAct, 0);
            std::printf ("  SERAPH STRENGTH 0: change %.1f dB below the input; SPACE %.1f -> %.1f dB, WARMTH %.1f -> %.1f dB (x1 -> x3)\n",
                         -residual, space1, space3, warm1, warm3);
            check (residual < -40.0f, "SERAPH STRENGTH 0 leaves the audio untouched (< -40 dB change)");
            check (space3 > space1 + 7.0f && warm3 > warm1 + 5.0f, "SERAPH STRENGTH 3 hits much harder than 1");
        }

        // Early reflections arrive well before the dense tail
        {
            const auto len = (size_t) (1.0 * sr);
            std::vector<float> l (len, 0.0f), r (len, 0.0f);
            l[0] = r[0] = 0.5f;
            Seraph::Settings st;
            st.mode = Seraph::heaven;
            st.silk.smooth = st.silk.air = st.silk.warmth = st.silk.body = 0.0f; st.silk.autoGain = false;
            st.halo.space = 1.0f; st.halo.duck = false; st.halo.shimmer = 0.0f; st.halo.width = 1.0f;
            Seraph unit;
            juce::ScopedNoDenormals noDenormals;
            unit.prepare (sr);
            for (size_t pos = 0; pos < len; pos += (size_t) block)
            {
                float* chp[2] { l.data() + pos, r.data() + pos };
                unit.process (chp, 2, (int) std::min ((size_t) block, len - pos), st);
            }
            // Sparse reflections: judge them by their peaks, not an average over the gaps between them
            float peak = 0.0f;
            for (size_t i = (size_t) (0.020 * sr); i < (size_t) (0.050 * sr); ++i) peak = std::max (peak, std::abs (l[i]));
            const float early = 20.0f * std::log10 (peak + 1e-9f);
            std::printf ("  impulse (-6 dBFS): strongest reflection 20-50 ms at %.1f dBFS\n", early);
            check (early > -45.0f, "early reflections give the space size before the tail");
        }

        // Everything at maximum: the output limiter keeps it clean and below full scale
        {
            const auto scene = makeScene (sr, 8.0, true, true, 23);
            KnobValues k;
            k.clarityAddMode = true; k.clarityAdd = 10.0f; k.subPercent = 100.0f; k.subBoost = true; k.footstep = true;
            k.smooth = k.air = k.warmth = k.body = 10.0f; k.tape = true; k.outputDb = 12.0f;
            k.widthPercent = 200.0f; k.space = 10.0f; k.decayS = 8.0f; k.shimmer = 10.0f; k.tone = 10.0f;
            k.enhMultiply = k.seraphMultiply = 3.0f; k.enhStrength = k.seraphStrength = 5.0f;
            const auto r = run (scene, sr, 128, enh::dsp::mapKnobs (k));
            std::printf ("  everything at maximum (3x, strength 5): peak %.3f\n", r.peak);
            check (r.finite && r.peak <= 1.0f, "MULTIPLY 3x + STRENGTH 5 on both units: finite and below full scale");
        }
    }

    {
        std::printf ("\n== Full chain: ENH Master + SERAPH at extreme settings ==\n");
        const auto scene = makeScene (sr, 8.0, true, true, 5);
        EnhEngine::Parameters p;
        p.normalize = 1.0f; p.boost = 1.0f; p.footstep = true;
        p.seraph.mode = enh::dsp::Seraph::heaven;
        p.seraph.silk = { 10.0f, 10.0f, 10.0f, 10.0f, 12.0f, true, true, false };
        p.seraph.halo = { 2.0f, 1.0f, 8.0f, 1.0f, 1.0f, false, false, true };
        const auto r = run (scene, sr, 64, p);
        std::printf ("  peak %.3f\n", r.peak);
        check (r.finite, "full chain stays finite with everything at maximum");
    }

    //==========================================================================
    std::printf ("\n== TIDE (adaptive compressor) ==\n");
    {
        using enh::dsp::DynamicCompressor;

        auto runTide = [&] (float levelDb, float response, float mix, bool dense, double seconds)
        {
            DynamicCompressor comp;
            comp.prepare (sr, 2);
            DynamicCompressor::Settings s;
            s.active = true;
            s.response = response;
            s.mix = mix;

            const int n = (int) (seconds * sr);
            std::vector<float> l ((size_t) n), r ((size_t) n);
            juce::Random rnd (7);
            Pink pk;
            double inSq = 0.0, outSq = 0.0;

            for (int i = 0; i < n; ++i)
            {
                float x = dbfs (levelDb) * pk.next (rnd) * 4.0f;
                if (! dense)   // sparse hits: high crest factor
                    x *= (i % (int) (0.4 * sr) < (int) (0.02 * sr)) ? 3.0f : 0.05f;
                l[(size_t) i] = r[(size_t) i] = x;
                inSq += (double) x * x;
            }

            for (int pos = 0; pos < n; pos += block)
            {
                float* c[2] { l.data() + pos, r.data() + pos };
                comp.process (c, 2, std::min (block, n - pos), s);
            }

            bool finite = true;
            for (int i = n / 2; i < n; ++i)
            {
                outSq += (double) l[(size_t) i] * l[(size_t) i];
                finite = finite && std::isfinite (l[(size_t) i]);
            }

            struct R { float threshold, gr, ratio, inDb, outDb; bool finite; };
            const double half = (double) (n - n / 2);
            return R { comp.getReadout().thresholdDb, comp.getReadout().gainReductionDb, comp.getReadout().ratio,
                       (float) (10.0 * std::log10 (inSq / n + 1e-12)), (float) (10.0 * std::log10 (outSq / half + 1e-12)), finite };
        };

        const auto loud = runTide (-12.0f, 0.5f, 1.0f, true, 6.0);
        const auto quiet = runTide (-30.0f, 0.5f, 1.0f, true, 6.0);
        std::printf ("  dense -12 dBFS: threshold %.1f dB, GR %.1f dB, ratio %.2f\n", loud.threshold, loud.gr, loud.ratio);
        std::printf ("  dense -30 dBFS: threshold %.1f dB, GR %.1f dB, ratio %.2f\n", quiet.threshold, quiet.gr, quiet.ratio);
        check (loud.threshold > quiet.threshold + 8.0f, "the threshold follows the programme level, it is not fixed");
        check (loud.gr > 0.5f, "dense material is actually compressed");

        const auto peaky = runTide (-12.0f, 0.5f, 1.0f, false, 6.0);
        std::printf ("  peaky -12 dBFS: threshold %.1f dB, ratio %.2f\n", peaky.threshold, peaky.ratio);
        check (peaky.ratio < loud.ratio, "peaky material gets a gentler ratio than dense material");

        const auto fast = runTide (-12.0f, 1.0f, 1.0f, true, 6.0);
        const auto slow = runTide (-12.0f, 0.0f, 1.0f, true, 6.0);
        std::printf ("  RESPONSE 10: threshold %.1f dB / RESPONSE 0: %.1f dB\n", fast.threshold, slow.threshold);
        check (fast.threshold < slow.threshold, "RESPONSE digs the threshold deeper into the programme");

        std::printf ("  level match: in %.1f dB -> out %.1f dB (MIX 100%%)\n", loud.inDb, loud.outDb);
        check (std::abs (loud.outDb - loud.inDb) < 4.0f, "auto make-up keeps the level within 4 dB, so MIX is usable");
        check (loud.finite && quiet.finite && peaky.finite, "TIDE stays finite");

        {
            DynamicCompressor comp;
            comp.prepare (sr, 2);
            DynamicCompressor::Settings s;
            s.active = true;
            s.mix = 0.0f;
            std::vector<float> a (512), b (512);
            juce::Random rnd (3);
            for (size_t i = 0; i < a.size(); ++i) a[i] = b[i] = dbfs (-6.0f) * (rnd.nextFloat() * 2.0f - 1.0f);
            const std::vector<float> before = a;
            float* c[2] { a.data(), b.data() };
            comp.process (c, 2, 512, s);
            float worst = 0.0f;
            for (size_t i = 0; i < a.size(); ++i) worst = std::max (worst, std::abs (a[i] - before[i]));
            check (worst == 0.0f, "MIX 0 % passes the signal through untouched");
        }
    }

    //==========================================================================
    std::printf ("\n== LUMEN (spectral leveler) ==\n");
    {
        using enh::dsp::SpectralLeveler;

        auto runLumen = [&] (float levelDb, float targetDb, double seconds)
        {
            SpectralLeveler lev;
            lev.prepare (sr, 2);
            SpectralLeveler::Settings s;
            s.active = true;
            s.targetDb = targetDb;
            s.response = 0.6f;

            const int n = (int) (seconds * sr);
            std::vector<float> l ((size_t) n), r ((size_t) n);
            juce::Random rnd (11);
            Pink pk;
            for (int i = 0; i < n; ++i)
                l[(size_t) i] = r[(size_t) i] = dbfs (levelDb) * pk.next (rnd) * 4.0f;

            const std::vector<float> dry = l;

            for (int pos = 0; pos < n; pos += block)
            {
                float* c[2] { l.data() + pos, r.data() + pos };
                lev.process (c, 2, std::min (block, n - pos), s);
            }

            double inSq = 0.0, outSq = 0.0;
            bool finite = true;
            for (int i = n / 2; i < n; ++i)
            {
                inSq += (double) dry[(size_t) i] * dry[(size_t) i];
                outSq += (double) l[(size_t) i] * l[(size_t) i];
                finite = finite && std::isfinite (l[(size_t) i]);
            }
            const double half = (double) (n - n / 2);
            struct R { float inDb, outDb, liftDb, total; bool finite; };
            return R { (float) (10.0 * std::log10 (inSq / half + 1e-12)), (float) (10.0 * std::log10 (outSq / half + 1e-12)),
                       (float) (10.0 * std::log10 (outSq / std::max (1e-12, inSq))), lev.getReadout().totalGainDb, finite };
        };

        const auto quiet = runLumen (-34.0f, -18.0f, 8.0);
        std::printf ("  quiet source: %.1f dB -> %.1f dB (lift %+.1f dB)\n", quiet.inDb, quiet.outDb, quiet.liftDb);
        check (quiet.liftDb > 4.0f, "quiet material is lifted toward the target");
        check (quiet.outDb <= -18.0f + 8.0f, "the lift stops near the target instead of running away");

        const auto loud = runLumen (-8.0f, -18.0f, 8.0);
        std::printf ("  loud source : %.1f dB -> %.1f dB (lift %+.1f dB)\n", loud.inDb, loud.outDb, loud.liftDb);
        check (std::abs (loud.liftDb) < 1.0f, "material already above the target is left alone");

        const auto floorOnly = runLumen (-72.0f, -18.0f, 8.0);
        std::printf ("  noise floor : %.1f dB -> %.1f dB (lift %+.1f dB)\n", floorOnly.inDb, floorOnly.outDb, floorOnly.liftDb);
        check (floorOnly.liftDb < 4.0f, "the noise floor is not dragged up to the target");

        {
            SpectralLeveler lev;
            lev.prepare (sr, 2);
            SpectralLeveler::Settings s;
            s.active = false;
            std::vector<float> a (2048), b (2048);
            juce::Random rnd (5);
            for (size_t i = 0; i < a.size(); ++i) a[i] = b[i] = dbfs (-10.0f) * (rnd.nextFloat() * 2.0f - 1.0f);
            const std::vector<float> before = a;
            float* c[2] { a.data(), b.data() };
            lev.process (c, 2, (int) a.size(), s);
            float worst = 0.0f;
            for (size_t i = 0; i < a.size(); ++i) worst = std::max (worst, std::abs (a[i] - before[i]));
            check (worst == 0.0f, "OUT is a true bypass");
        }

        {
            SpectralLeveler lev;
            lev.prepare (sr, 2);
            SpectralLeveler::Settings s;
            s.active = true;
            s.targetDb = -60.0f;     // nothing is below this, so every band gain stays at 0 dB
            std::vector<float> a (8192), b (8192);
            juce::Random rnd (9);
            Pink pk;
            for (size_t i = 0; i < a.size(); ++i) a[i] = b[i] = dbfs (-10.0f) * pk.next (rnd) * 4.0f;
            const std::vector<float> before = a;
            for (int pos = 0; pos < (int) a.size(); pos += block)
            {
                float* c[2] { a.data() + pos, b.data() + pos };
                lev.process (c, 2, std::min (block, (int) a.size() - pos), s);
            }
            float worst = 0.0f;
            for (size_t i = 64; i < a.size(); ++i) worst = std::max (worst, std::abs (a[i] - before[i]));
            std::printf ("  band sum error: %.2e\n", worst);
            check (worst < 1.0e-5f, "the three bands sum back to the input (LR4 split is transparent)");
        }

        check (quiet.finite && loud.finite, "LUMEN stays finite");
    }

    runLimiterTests (sr);
    runPresetTests (sr);

    std::printf ("\n== CPU (this machine) ==\n");
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const auto scene = makeScene (rate, 20.0, true, true, 42);
        EnhEngine::Parameters p;
        p.normalize = 0.7f; p.adaptSpeed = 0.5f; p.sub = 0.6f; p.subBoost = true; p.footstep = true;
        p.seraph.mode = enh::dsp::Seraph::heaven;   // every unit running, as the plugin ships
        p.lumen.active = true;
        p.limiter.active = true;
        p.tide.active = true;
        const auto r = run (scene, rate, 256, p);
        const double realtime = 20.0 / r.seconds;
        std::printf ("  %6.0f Hz stereo : %.1fx realtime  (%.2f%% of one core)\n", rate, realtime, 100.0 / realtime);
        check (r.finite && r.peak <= 1.0f, "stable at " + juce::String ((int) rate) + " Hz");
    }

    //==========================================================================
    std::printf ("\n== Odd block sizes ==\n");
    {
        const auto scene = makeScene (sr, 4.0, true, false, 3);
        EnhEngine::Parameters p;
        p.normalize = 1.0f; p.sub = 1.0f; p.subBoost = true; p.footstep = true;
        bool ok = true;
        for (int bs : { 1, 7, 33, 480, 1024 })
        {
            const auto r = run (scene, sr, bs, p);
            ok = ok && r.finite && r.peak <= 1.0f;
        }
        check (ok, "stable for block sizes 1, 7, 33, 480, 1024");
    }

    std::printf ("\n%s (%d failure%s)\n\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
