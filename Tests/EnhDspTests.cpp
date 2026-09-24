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
#include "DSP/Character.h"
#include "DSP/ParameterMapping.h"
#include "Parameters/PresetLibrary.h"
#include "Parameters/KnobModifiers.h"
#include "DSP/MethodRegistry.h"

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

    /** Steps in a reverberant room: varied surfaces through a comb / allpass room (RT ~0.6 s),
        over ambience, optionally with speech. Game engines put steps in rooms like this. */
    Scene makeReverbScene (double sr, double seconds, int seed, bool withSpeech)
    {
        Scene dry = makeScene (sr, seconds, false, false, seed, true);
        Scene s;
        s.left.assign (dry.left.size(), 0.0f);
        s.right.assign (dry.right.size(), 0.0f);
        s.steps = dry.steps;

        // Remove the ambience from the dry scene's steps: rebuild them alone, then add both
        juce::Random r (seed + 1);
        Pink pinkL, pinkR;
        const int combs[4] { (int) (0.0297 * sr), (int) (0.0371 * sr), (int) (0.0411 * sr), (int) (0.0437 * sr) };
        std::vector<std::vector<float>> lines (4);
        std::vector<int> pos (4, 0);
        std::vector<float> damp (4, 0.0f);
        for (int c = 0; c < 4; ++c) lines[(size_t) c].assign ((size_t) combs[c], 0.0f);
        const float feedback = 0.80f;   // RT60 ~0.6 s at these delays

        for (size_t i = 0; i < dry.left.size(); ++i)
        {
            const float x = 0.5f * (dry.left[i] + dry.right[i]);
            float wet = 0.0f;
            for (int c = 0; c < 4; ++c)
            {
                auto& line = lines[(size_t) c];
                const float y = line[(size_t) pos[(size_t) c]];
                damp[(size_t) c] += (y - damp[(size_t) c]) * 0.45f;
                line[(size_t) pos[(size_t) c]] = x + damp[(size_t) c] * feedback;
                pos[(size_t) c] = (pos[(size_t) c] + 1) % combs[c];
                wet += y;
            }
            wet *= 0.30f;
            s.left[i] = dry.left[i] + wet + pinkL.next (r) * dbfs (-48.0f);
            s.right[i] = dry.right[i] + wet * 0.9f + pinkR.next (r) * dbfs (-48.0f);
        }

        if (withSpeech)
        {
            addSpeech (s, sr, 4.0, 7.0, r);
            addSpeech (s, sr, 13.0, 16.0, r);
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
        double meanAbsBandGain = 0.0;
        std::array<double, enh::dsp::numBands> curve {};   // mean displayed EQ curve over the last 40 %
        float treble = 0.0f, bass = 0.0f;                  // measured balance at the end
        float maxAbsBandGain = 0.0f;
        std::vector<float> outL, outR;
        double seconds = 0.0;
        bool finite = true;
        float peak = 0.0f;
        float earGuardMaxDb = 0.0f;   // how far the EAR GUARD held a jump down, at most
    };

    RunResult run (const Scene& scene, double sr, int blockSize, const EnhEngine::Parameters& p,
                   std::function<void (const EnhEngine&)> atEnd = {}, std::function<void (EnhEngine&)> atStart = {})
    {
        EnhEngine engine;
        engine.prepare (sr, blockSize, 2);
        if (atStart)
            atStart (engine);

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
            res.earGuardMaxDb = std::max (res.earGuardMaxDb, engine.getMeters().earGuardDb.load());

            float blockAbs = 0.0f;
            for (auto& g : engine.getMeters().bandGainDb)
            {
                const float v = std::abs (g.load());
                blockAbs += v;
                res.maxAbsBandGain = std::max (res.maxAbsBandGain, v);
            }
            res.meanAbsBandGain += blockAbs / (float) enh::dsp::numBands;

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
        int footstepEvents = 0;
        std::array<float, 3> levelerBefore {}, levelerDuring {};   // UPWARD LEVELER lift per band
        bool finite = true;
    };

    LimiterRun runLimiterScene (double sr, bool limiterIn, bool broadbandEvent, int blockSize, float eventDb = 0.0f, bool trebleEvent = false,
                                const EnhEngine::Parameters* whole = nullptr, std::function<void (EnhEngine&)> setup = {})
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
        if (setup)
            setup (engine);
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
            if (std::getenv ("LIMITER_TRACE") != nullptr && limiterIn && (blockSize == 128 || std::getenv ("PRESET_DIAG")) && broadbandEvent == (std::atoi (std::getenv ("LIMITER_TRACE")) == 2) && ((t > (std::getenv ("LIMITER_FROM") ? std::atof (std::getenv ("LIMITER_FROM")) : eventAt - 0.6) && t < eventAt + 0.6 && pos % (blockSize * (std::getenv ("LIMITER_FINE") ? 1 : 24)) == 0)))
            {
                const auto& v = lim.getBandView();
                std::printf ("   t=%.2f cut=%.1f bb=%.1f loc=%.2f tideGR=%.1f levH=%.1f hold=%d |", t, lim.getDeepestCutDb(), lim.getBroadbandDb(), lim.getLocalisation(), engine.getCompressor().getReadout().gainReductionDb,
                             engine.getLeveler().getReadout().gainDb[2], lim.isHandlingLocalisedEvent() ? 1 : 0);
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

        out.footstepEvents = engine.getFootstepEventCount();
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
    /** The presets under test: the local preset file (the one the plugin uses, so a re-tuned file is
        tested as it stands), or the factory presets when there is none. Not seeded from here. */
    const std::vector<pad::presets::Preset>& presetList()
    {
        static const auto list = pad::presets::library (false);
        return *list;
    }

    /** A preset by name from that list, else from the factory list (tests that need a particular one). */
    const pad::presets::Preset& presetNamed (const char* name)
    {
        for (auto& p : presetList())
            if (p.name == name)
                return p;
        for (auto& p : pad::presets::factory())
            if (p.name == name)
                return p;
        return pad::presets::factory().front();
    }

    /** A factory preset as the engine sees it: the panel values through the same mapping the plugin uses. */
    EnhEngine::Parameters presetParameters (const pad::presets::Preset& preset,
                                            std::function<void (enh::dsp::KnobValues&)> adjust = {})
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
            else if (i == id::levelGain) k.levelDb = v;
            else if (i == id::balAmount) k.balAmount = v;   else if (i == id::balSpeed) k.balSpeed = v;
            else if (i == id::balTilt) k.balTilt = v;       else if (i == id::balRange) k.balRangeDb = v;
            else if (i == id::balActive) k.balActive = on;
            else if (i == id::balResolution) k.balResolution = v;
            else if (i == id::seraphMode) k.seraphMode = juce::roundToInt (v);
            else if (i == id::seraphMultiply) k.seraphMultiply = v; else if (i == id::seraphStrength) k.seraphStrength = v;
            else if (i == id::silkSmooth) k.smooth = v;   else if (i == id::silkAir) k.air = v;
            else if (i == id::silkWarmth) k.warmth = v;   else if (i == id::silkBody) k.body = v;
            else if (i == id::silkOutput) k.outputDb = v; else if (i == id::silkProtect) k.protect = on;
            else if (i == id::silkTape) k.tape = on;      else if (i == id::silkAuto) k.autoGain = on;
            else if (i == id::heavenHold) k.heavenHold = v; else if (i == id::heavenLift) k.heavenLift = v;
            else if (i == id::heavenMode) k.heavenLiftMode = on;
            else if (i == id::silkSub) k.silkSub = v;
            else if (i == id::heavenAuto) k.heavenAuto = on; else if (i == id::heavenAutoAmount) k.heavenAutoAmount = v;
            else if (i == id::haloWidth) k.widthPercent = v; else if (i == id::haloSpace) k.space = v;
            else if (i == id::haloDecay) k.decayS = v;       else if (i == id::haloShimmer) k.shimmer = v;
            else if (i == id::haloTone) k.tone = v;          else if (i == id::haloDuck) k.duck = on;
            else if (i == id::haloBassMono) k.bassMono = on; else if (i == id::haloMod) k.mod = on;
            else if (i == id::deepDepth) k.deepDepth = v;   else if (i == id::deepHull) k.deepHull = v;
            else if (i == id::deepSize) k.deepSize = v;     else if (i == id::deepPressure) k.deepPressure = v;
            else if (i == id::deepActive) k.deepActive = on;
            else if (i == id::charModelA) k.charModelA = v; else if (i == id::charModelB) k.charModelB = v;
            else if (i == id::charBlend) k.charBlend = v;   else if (i == id::charDrive) k.charDrive = v;
            else if (i == id::charColour) k.charColour = v;
            else if (i == id::charActive) k.charActive = on;
            else if (i == id::abCompare) k.compare = on;
            else if (i == id::charGrit) k.charGrit = on;
        }
        if (adjust)
            adjust (k);
        return enh::dsp::mapKnobs (k);
    }

    double rmsDb (const std::vector<float>& l, const std::vector<float>& r, size_t from)
    {
        double sum = 0.0;
        for (size_t i = from; i < l.size(); ++i)
            sum += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]);
        return 10.0 * std::log10 (sum / (double) std::max<size_t> (1, l.size() - from) + 1e-15);
    }

    /** Sets (or, with an empty value, clears) an environment variable, portably. */
    void setEnv (const char* name, const juce::String& value)
    {
       #if JUCE_WINDOWS
        _putenv_s (name, value.toRawUTF8());
       #else
        if (value.isEmpty()) ::unsetenv (name);
        else ::setenv (name, value.toRawUTF8(), 1);
       #endif
    }

    /** The preset file format: the factory list survives a write and a read unchanged, and a broken
        file is refused rather than half-loaded. */
    void runPresetFileTests()
    {
        std::printf ("\n== Preset file ==\n");
        juce::String error;
        const auto back = pad::presets::fromJson (pad::presets::toJson (pad::presets::factory()), error);
        bool same = back.size() == pad::presets::factory().size();
        for (size_t i = 0; same && i < back.size(); ++i)
        {
            const auto& a = pad::presets::factory()[i];
            same = a.name == back[i].name && a.values.size() == back[i].values.size();
            for (size_t k = 0; same && k < a.values.size(); ++k)
                same = a.values[k].first == back[i].values[k].first && std::abs (a.values[k].second - back[i].values[k].second) < 1.0e-3f;
        }
        check (same, "factory presets survive a write and a read unchanged");

        juce::String brokenError;
        const bool refused = pad::presets::fromJson ("{ \"presets\": [ { \"name\": \"X\", ", brokenError).empty();
        check (refused && brokenError.isNotEmpty(), "a broken preset file is refused (" + brokenError + ")");

        const auto clamped = pad::presets::fromJson (R"({ "presets": [ { "name": "T", "values": { "haloDecay": 99, "noSuchParam": 1 } } ] })", error);
        check (clamped.size() == 1 && clamped[0].values.size() == 1 && clamped[0].values[0].second == 8.0f,
               "values are clamped to their range, unknown parameters skipped");

        // An edited file is picked up without a restart (what the plugin does on a PRESET press)
        {
            const auto previous = juce::SystemStats::getEnvironmentVariable ("ENH_MASTER_PRESETS", {});
            const auto temp = juce::File::createTempFile (".json");
            setEnv ("ENH_MASTER_PRESETS", temp.getFullPathName());
            temp.replaceWithText (R"({ "presets": [ { "name": "FIRST", "values": {} } ] })");
            const auto first = pad::presets::library (false);
            juce::Thread::sleep (600);
            temp.replaceWithText (R"({ "presets": [ { "name": "SECOND", "values": {} }, { "name": "THIRD", "values": {} } ] })");
            temp.setLastModificationTime (juce::Time::getCurrentTime() + juce::RelativeTime::seconds (2.0));
            const auto second = pad::presets::library (false);
            check (first->front().name == "FIRST" && second->size() == 2 && second->front().name == "SECOND",
                   "an edited preset file is re-read without restarting");
            temp.deleteFile();
            setEnv ("ENH_MASTER_PRESETS", previous);
        }
    }

    void runPresetTests (double sr)
    {
        runPresetFileTests();
        std::printf ("\n== Presets (whole rack) on the game scene and the bass-hit mix ==\n");
        const int presetCount = (int) presetList().size();   // loads the list first, so the source is known
        std::printf ("  %d presets from %s\n", presetCount, pad::presets::librarySource().toRawUTF8());
        std::printf ("  %-24s %8s %7s %6s %6s %10s %9s\n", "preset", "level", "peak", "events", "cut", "detail dip", "comp GR");
        const auto scene = makeScene (sr, 16.0, true, true, 42);
        const double inDb = rmsDb (scene.left, scene.right, (size_t) (4.0 * sr));

        std::map<juce::String, float> dips, dipsOut;
        int defaultSteps = 0, footstepSteps = 0;
        for (auto& preset : presetList())
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
            std::printf ("  %-24s %+6.1f dB %7.3f %6d %5.1f %+9.2f dB %6.1f dB   (limiter OUT: %+.2f dB)\n", preset.name.toRawUTF8(), outDb - inDb, r.peak, steps,
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
            // The limiter has to take ducking away wherever there is any worth taking away. Where the rack
            // already ducks less than the bound without it, there is nothing left for it to do - it must
            // only not make it worse. (3.6.5.1's smoother EQ and TONE left DEEP SUB: SUBMARINE at 2.1 dB
            // without the limiter, just over the 2.0 this used to allow.)
            const bool improves = std::abs (dip) < std::abs (dipsOut[name]) - 0.15f
                               || (std::abs (dipsOut[name]) < bound && std::abs (dip) <= std::abs (dipsOut[name]) + 0.05f);
            check (improves && std::abs (dip) < bound,
                   name + ": less ducking with the SPECTRAL LIMITER than without, and under " + juce::String (bound, 1) + " dB");
        }
        (void) defaultSteps;

        if (std::getenv ("PRESET_DIAG") != nullptr)
        {
            // Which stage ducks the detail under the bass hit, with the whole rack running (DEFAULT)
            const auto base = presetParameters (presetList()[(size_t) std::atoi (std::getenv ("PRESET_DIAG"))]);
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
            variant ("tone & space: SUB 0", [] (auto& p) { p.seraph.silk.sub = 0.0f; });
            variant ("tone & space: AUTO off", [] (auto& p) { p.seraph.heaven.autoHeaven = false; });
            variant ("only limiter + compressor", [] (auto& p) { p.strength = 0.0f; p.normalize = 0.0f; p.sub = 0.0f; p.lumen.active = false; p.seraph.mode = enh::dsp::Seraph::off; });
        }

        // The reference preset: level unchanged, and nothing processing the bass hit
        {
            const auto p = presetParameters (presetNamed ("TRANSPARENT (ALL OUT)"));
            const auto r = run (scene, sr, 256, p);
            const double outDb = rmsDb (r.outL, r.outR, (size_t) (4.0 * sr));
            const auto hit = runLimiterScene (sr, true, false, 256, 0.0f, false, &p);
            std::printf ("  TRANSPARENT: level %+.2f dB, limiter cut %.2f dB, compressor GR %.2f dB, EAR GUARD up to %.1f dB\n",
                         outDb - inDb, hit.cutDuringDb, hit.compressorGrDuringDb, r.earGuardMaxDb);
            // The units do nothing; the always-on safety stages still may: the EAR GUARD holds this scene's
            // gunfire volleys (25 dB over the footsteps around them) down, and only ever takes level away
            const bool levelOk = std::abs (outDb - inDb) < 0.5 || (outDb < inDb && r.earGuardMaxDb > 3.0f);
            check (levelOk && hit.cutDuringDb < 0.01f && hit.compressorGrDuringDb < 0.01f,
                   "TRANSPARENT (ALL OUT): no unit processing; the level unchanged but for what the EAR GUARD took off jumps");
        }
    }
    //==========================================================================
    /** Loud bass through the whole rack: harmonic distortion (2nd-5th) of a 45 Hz tone at -1 dBFS. */
    float bassThdPercent (double sr, const EnhEngine::Parameters& p, float levelDb, float* peakOut = nullptr)
    {
        const int n = (int) (6.0 * sr);
        Scene s;
        s.left.assign ((size_t) n, 0.0f);
        s.right.assign ((size_t) n, 0.0f);
        juce::Random r (3);
        Pink pl, pr;
        for (int i = 0; i < n; ++i)
        {
            const float tone = dbfs (levelDb) * (float) std::sin (twoPi * 45.0 * i / sr);
            s.left[(size_t) i] = tone + pl.next (r) * dbfs (-46.0f);
            s.right[(size_t) i] = tone + pr.next (r) * dbfs (-46.0f);
        }
        const auto res = run (s, sr, 256, p);
        const size_t from = (size_t) (3.0 * sr), to = (size_t) n;
        const double f = std::pow (10.0, toneDb (res.outL, sr, 45.0, from, to) / 20.0);
        double h = 0.0;
        for (int k = 2; k <= 5; ++k)
            h += std::pow (10.0, toneDb (res.outL, sr, 45.0 * k, from, to) / 10.0);
        if (peakOut != nullptr)
            *peakOut = res.peak;
        return (float) (100.0 * std::sqrt (h) / std::max (1e-9, f));
    }

    /** TONE & SPACE alone: a steady 2 kHz tone, with a 60 Hz bass note switching on and off every second.
        How far the tone's level moves between bass-on and bass-off stretches is the pumping. */
    float seraphPumpingDb (double sr, const enh::dsp::Seraph::Settings& st)
    {
        using enh::dsp::Seraph;
        Seraph unit;
        unit.prepare (sr);
        const int n = (int) (12.0 * sr);
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const bool bassOn = std::fmod (t, 2.0) < 1.0;
            const double env = std::min (1.0, std::min (std::fmod (t, 2.0), 1.0 - std::fmod (t, 2.0) + (bassOn ? 0.0 : 10.0)) / 0.02);
            const float bass = bassOn ? dbfs (-8.0f) * (float) (std::sin (twoPi * 60.0 * t) * std::max (0.0, env)) : 0.0f;
            const float tone = dbfs (-28.0f) * (float) std::sin (twoPi * 2000.0 * t);
            l[(size_t) i] = r[(size_t) i] = bass + tone;
        }
        for (int pos = 0; pos < n; pos += 256)
        {
            float* c[2] { l.data() + pos, r.data() + pos };
            unit.process (c, 2, std::min (256, n - pos), st);
            if (std::getenv ("SMOOTH_TRACE") != nullptr && pos % (int) (0.25 * sr) < 256 && pos > (int) (8.0 * sr))
            {
                std::printf ("    t=%.2f bass %s dips:", pos / sr, std::fmod (pos / sr, 2.0) < 1.0 ? "on " : "off");
                for (int k = 0; k < 28; ++k) std::printf (" %.0f", unit.getSilk().getDips()[(size_t) k]);
                std::printf ("\n");
            }
        }
        // Tone level in the middle of bass-on and bass-off stretches, over the last 8 s
        const auto bp = BiquadCoeffs::bandPass (sr, 2000.0, 6.0);
        BiquadState b1, b2;
        std::vector<float> y ((size_t) n);
        for (int i = 0; i < n; ++i) y[(size_t) i] = b1.process (bp, b2.process (bp, 0.5f * (l[(size_t) i] + r[(size_t) i])));
        double on = 0, off = 0; int nOn = 0, nOff = 0;
        for (int i = (int) (4.0 * sr); i < n; ++i)
        {
            const double ph = std::fmod (i / sr, 2.0);
            if (ph > 0.3 && ph < 0.9) { on += (double) y[(size_t) i] * y[(size_t) i]; ++nOn; }
            if (ph > 1.3 && ph < 1.9) { off += (double) y[(size_t) i] * y[(size_t) i]; ++nOff; }
        }
        return (float) (10.0 * std::log10 ((off / std::max (1, nOff)) / std::max (1e-15, on / std::max (1, nOn))));
    }

    /** The whole rack: the same 2 kHz tone with bass switching on and off every second. */
    float rackPumpingDb (double sr, const EnhEngine::Parameters& p)
    {
        const int n = (int) (12.0 * sr);
        Scene sc;
        sc.left.assign ((size_t) n, 0.0f);
        sc.right.assign ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr, ph = std::fmod (t, 2.0);
            const bool bassOn = ph < 1.0;
            const double env = bassOn ? std::min (1.0, std::min (ph, 1.0 - ph) / 0.02) : 0.0;
            const float v = (float) (dbfs (-8.0f) * std::sin (twoPi * 60.0 * t) * env + dbfs (-28.0f) * std::sin (twoPi * 2000.0 * t));
            sc.left[(size_t) i] = sc.right[(size_t) i] = v;
        }
        const auto res = run (sc, sr, 256, p);
        const auto bp = BiquadCoeffs::bandPass (sr, 2000.0, 6.0);
        BiquadState b1, b2;
        double on = 0, off = 0; int nOn = 0, nOff = 0;
        for (int i = 0; i < n; ++i)
        {
            const float y = b1.process (bp, b2.process (bp, res.outL[(size_t) i]));
            const double ph = std::fmod (i / sr, 2.0);
            if (i < (int) (4.0 * sr)) continue;
            if (ph > 0.3 && ph < 0.9) { on += (double) y * y; ++nOn; }
            if (ph > 1.3 && ph < 1.9) { off += (double) y * y; ++nOff; }
        }
        return (float) (10.0 * std::log10 ((off / std::max (1, nOff)) / std::max (1e-15, on / std::max (1, nOn))));
    }

    void runLoudBassTests (double sr)
    {
        if (std::getenv ("PUMP_DIAG") != nullptr)
        {
            const auto base = presetParameters (presetNamed ("DEFAULT"));
            auto show = [&] (const char* what, auto&& change)
            {
                auto p = base;
                change (p);
                std::printf ("    rack %-34s tone off-vs-on %+.2f dB\n", what, rackPumpingDb (sr, p));
            };
            auto iso = [&] (const char* what, auto&& change)
            {
                auto st = base.seraph;
                change (st);
                std::printf ("    T&S alone %-28s tone off-vs-on %+.2f dB\n", what, seraphPumpingDb (sr, st));
            };
            iso ("as is", [] (auto&) {});
            iso ("no loudness hold", [] (auto& st) { st.heaven.amount = 0.0f; });
            iso ("no AUTO gain", [] (auto& st) { st.silk.autoGain = false; });
            iso ("no DUCK", [] (auto& st) { st.halo.duck = false; });
            iso ("SPACE 0", [] (auto& st) { st.halo.space = 0.0f; });
            iso ("TONE knobs 0", [] (auto& st) { st.silk.warmth = st.silk.air = st.silk.body = st.silk.smooth = 0.0f; });
            iso ("SMOOTH 0", [] (auto& st) { st.silk.smooth = 0.0f; });
            iso ("WARMTH 0", [] (auto& st) { st.silk.warmth = 0.0f; });
            iso ("AIR 0", [] (auto& st) { st.silk.air = 0.0f; });
            iso ("BODY 0", [] (auto& st) { st.silk.body = 0.0f; });
            iso ("hold + auto off", [] (auto& st) { st.heaven.amount = 0.0f; st.silk.autoGain = false; });
            show ("DEFAULT", [] (auto&) {});
            show ("tone & space OFF", [] (auto& p) { p.seraph.mode = enh::dsp::Seraph::off; });
            show ("tone & space: no loudness hold", [] (auto& p) { p.seraph.heaven.amount = 0.0f; });
            show ("tone & space: no AUTO gain", [] (auto& p) { p.seraph.silk.autoGain = false; });
            show ("tone & space: no DUCK", [] (auto& p) { p.seraph.halo.duck = false; });
            show ("compressor OUT", [] (auto& p) { p.tide.active = false; });
            show ("leveler OUT", [] (auto& p) { p.lumen.active = false; });
            show ("limiter OUT", [] (auto& p) { p.limiter.active = false; });
            show ("enhancer STRENGTH 0", [] (auto& p) { p.strength = 0.0f; p.normalize = 0.0f; });
            show ("only enhancer", [] (auto& p) { p.seraph.mode = 0; p.tide.active = false; p.lumen.active = false; p.limiter.active = false; });
        }

        if (std::getenv ("THD_DIAG") != nullptr)
        {
            const auto base = presetParameters (presetList()[(size_t) std::atoi (std::getenv ("THD_DIAG"))]);
            auto show = [&] (const char* what, auto&& change)
            {
                auto p = base;
                change (p);
                float peak = 0.0f;
                const float thd = bassThdPercent (sr, p, -1.0f, &peak);
                std::printf ("    preset %-30s THD %6.2f %%  peak %.3f\n", what, thd, peak);
            };
            show ("as is", [] (auto&) {});
            show ("tone & space OFF", [] (auto& p) { p.seraph.mode = enh::dsp::Seraph::off; });
            show ("no TAPE, WARMTH 0", [] (auto& p) { p.seraph.silk.tape = false; p.seraph.silk.warmth = 0.0f; });
            show ("compressor OUT", [] (auto& p) { p.tide.active = false; });
            show ("limiter OUT", [] (auto& p) { p.limiter.active = false; });
            show ("sub 0", [] (auto& p) { p.sub = 0.0f; p.subBoost = false; });
            show ("enhancer STRENGTH 0", [] (auto& p) { p.strength = 0.0f; p.normalize = 0.0f; p.boost = 0.0f; p.sub = 0.0f; });
            show ("T&S: WARMTH 0", [] (auto& p) { p.seraph.silk.warmth = 0.0f; });
            show ("T&S: AIR 0", [] (auto& p) { p.seraph.silk.air = 0.0f; });
            show ("T&S: BODY 0", [] (auto& p) { p.seraph.silk.body = 0.0f; });
            show ("T&S: SMOOTH 0", [] (auto& p) { p.seraph.silk.smooth = 0.0f; });
            show ("T&S: TONE only (no SPACE)", [] (auto& p) { p.seraph.mode = enh::dsp::Seraph::silkOnly; });
            show ("T&S: all TONE knobs 0", [] (auto& p) { p.seraph.silk.warmth = p.seraph.silk.air = p.seraph.silk.body = p.seraph.silk.smooth = 0.0f; p.seraph.silk.tape = false; });
            show ("everything off", [] (auto& p) { p.strength = 0.0f; p.normalize = 0.0f; p.boost = 0.0f; p.sub = 0.0f; p.seraph.mode = 0; p.tide.active = false; p.lumen.active = false; p.limiter.active = false; });
        }

        std::printf ("\n== Loud bass through the whole rack: 45 Hz at -1 dBFS, harmonic distortion ==\n");
        for (auto* name : { "DEFAULT", "BASS HEAVY, PROTECTED" })
        {
            const auto& preset = presetNamed (name);
            float peak = 0.0f;
            const float thd = bassThdPercent (sr, presetParameters (preset), -1.0f, &peak);
            std::printf ("  %-24s THD %.2f %%  (peak %.3f)\n", preset.name.toRawUTF8(), thd, peak);
            check (thd < 3.0f && peak <= 1.0f, juce::String (preset.name) + ": loud bass stays clean (THD < 3 %)");
        }
        {
            auto p = presetParameters (presetNamed ("DEFAULT"));
            p.seraph.silk.sub = 10.0f;
            float peak = 0.0f;
            const float thd = bassThdPercent (sr, p, -1.0f, &peak);
            std::printf ("  %-24s THD %.2f %%  (peak %.3f)\n", "DEFAULT, SUB 10", thd, peak);
            check (thd < 4.0f && peak <= 1.0f, "SUB at full: loud bass stays clean (THD < 4 %, its glow is a 2nd harmonic)");
        }

        std::printf ("\n== TONE & SPACE: 2 kHz tone while bass switches on and off (pumping) ==\n");
        auto st = presetParameters (presetNamed ("DEFAULT")).seraph;
        const float pump = seraphPumpingDb (sr, st);
        std::printf ("  tone level with bass off vs on: %+.2f dB\n", pump);
        check (std::abs (pump) < 0.75f, "the tone does not pump with the bass (< 0.75 dB)");

        auto withSub = st;
        withSub.silk.sub = 10.0f;
        const float pumpSub = seraphPumpingDb (sr, withSub);
        std::printf ("  ... with SUB 10: %+.2f dB\n", pumpSub);
        check (std::abs (pumpSub) < 0.75f, "SUB 10: the tone does not pump with the bass");

        auto withAuto = st;
        withAuto.heaven.autoHeaven = true;
        withAuto.heaven.autoAmount = 1.0f;
        const float pumpAuto = seraphPumpingDb (sr, withAuto);
        std::printf ("  ... with AUTO, HEAVEN 10: %+.2f dB\n", pumpAuto);
        check (std::abs (pumpAuto) < 0.75f, "AUTO at full: the tone does not pump with the bass");
    }
}

#define ENH_HAS_OLD_FOOTSTEP_DETECTOR 0
#include "RadarTests.h"
#include "PrecisionTests.h"

namespace
{
    /** Golden output: the whole rack's exact output for fixed scenes, saved once and compared after an
        optimisation. An optimisation that changes nothing audible stays within float rounding
        (-120 dB); anything above that is a real change to the sound.
          --golden write <file>   save      --golden check <file>   compare */
    /** The LEVEL & LOUDNESS unit, the output limiter's 0 dBFS rule and the MIX BALANCER. */
    void runNewUnitTests (double sr)
    {
        using enh::dsp::LoudnessMeter;
        using enh::dsp::FinalLimiter;
        using enh::dsp::MixBalancer;
        const double twoPi = 6.283185307179586;

        std::printf ("\n== LOUDNESS METER (EBU Tech 3341 cases) ==\n");
        {
            LoudnessMeter m;
            m.prepare (sr);
            const float amp = std::pow (10.0f, -23.0f / 20.0f);
            std::vector<float> l ((size_t) (20.0 * sr)), r;
            for (size_t i = 0; i < l.size(); ++i)
                l[i] = amp * (float) std::sin (twoPi * 1000.0 * (double) i / sr);
            r = l;
            const float* ch[2] { l.data(), r.data() };
            m.process (ch, 2, (int) l.size());
            std::printf ("  stereo 1 kHz sine at -23 dBFS: M %.2f  S %.2f  I %.2f LUFS\n", m.getMomentaryLufs(), m.getShortTermLufs(), m.getIntegratedLufs());
            check (std::abs (m.getMomentaryLufs() + 23.0f) < 0.1f && std::abs (m.getShortTermLufs() + 23.0f) < 0.1f
                   && std::abs (m.getIntegratedLufs() + 23.0f) < 0.1f, "-23 dBFS stereo 1 kHz reads -23.0 LUFS (M, S, I within 0.1)");

            // Then 20 s of silence: gated out of the integrated reading
            std::vector<float> z ((size_t) (20.0 * sr), 0.0f);
            const float* zc[2] { z.data(), z.data() };
            m.process (zc, 2, (int) z.size());
            check (std::abs (m.getIntegratedLufs() + 23.0f) < 0.1f, "silence is gated out of the integrated loudness");
            m.resetIntegrated();
            check (m.getIntegratedLufs() <= -69.9f, "RESET clears the integrated loudness");
        }
        {
            // True peak: fs/4 at 45 degrees - every sample sits at 0.707 of the real peak
            LoudnessMeter m;
            m.prepare (48000.0);
            std::vector<float> l (48000);
            for (size_t i = 0; i < l.size(); ++i)
                l[i] = 0.5f * (float) std::sin (twoPi * 12000.0 * (double) i / 48000.0 + 0.25 * 3.14159265);
            const float* ch[2] { l.data(), l.data() };
            m.process (ch, 2, (int) l.size());
            std::printf ("  12 kHz at -6.02 dBFS true peak, samples at -9.03: true peak %.2f dBTP\n", m.getTruePeakDb());
            check (std::abs (m.getTruePeakDb() + 6.02f) < 0.5f, "true peak finds the peak between the samples (within 0.5 dB)");
        }

        std::printf ("\n== LEVEL ==\n");
        {
            const auto scene = makeScene (sr, 8.0, true, false, 5);
            // Both under full scale, so the output limiter (0 dBFS) is out of the picture
            auto flat = presetParameters (presetNamed ("TRANSPARENT (ALL OUT)"));
            flat.levelDb = -6.0f;
            auto down = flat;
            down.levelDb = -18.0f;
            const auto a = run (scene, sr, 256, flat), b = run (scene, sr, 256, down);
            const double da = rmsDb (a.outL, a.outR, (size_t) (2.0 * sr)), db = rmsDb (b.outL, b.outR, (size_t) (2.0 * sr));
            std::printf ("  TRANSPARENT, LEVEL -6 -> -18 dB: output %+.2f dB\n", db - da);
            check (std::abs ((db - da) + 12.0) < 0.1, "LEVEL -12 dB takes the rack's output down 12 dB");

            // The leveler's target moves with LEVEL: it does not lift the rack back up
            auto p0 = presetParameters (presetNamed ("DEFAULT")), p1 = p0;
            p1.levelDb = -12.0f;
            float lift0 = 0.0f, lift1 = 0.0f;
            run (scene, sr, 256, p0, [&] (const EnhEngine& e) { lift0 = e.getLeveler().getReadout().totalGainDb; });
            run (scene, sr, 256, p1, [&] (const EnhEngine& e) { lift1 = e.getLeveler().getReadout().totalGainDb; });
            std::printf ("  DEFAULT: leveler lift at LEVEL 0 dB %.2f dB, at -12 dB %.2f dB\n", lift0, lift1);
            check (std::abs (lift1 - lift0) < 1.0f, "the leveler does not fight LEVEL (same lift within 1 dB)");
        }

        std::printf ("\n== OUTPUT LIMITER: only real overs, and the region causing them ==\n");
        {
            FinalLimiter lim;
            lim.prepare (sr);
            const int n = (int) (2.0 * sr), lat = lim.getLatencySamples();
            std::vector<float> in ((size_t) n), l, r;
            for (int i = 0; i < n; ++i)
                in[(size_t) i] = 0.89f * (float) std::sin (twoPi * 60.0 * i / sr);   // -1 dBFS
            l = r = in;
            float* ch[2] { l.data(), r.data() };
            lim.process (ch, 2, n);
            double maxDiff = 0.0;
            for (int i = lat; i < n; ++i)
                maxDiff = std::max (maxDiff, (double) std::abs (l[(size_t) i] - in[(size_t) (i - lat)]));
            std::printf ("  -1 dBFS through it: largest change %.2g\n", maxDiff);
            check (maxDiff < 1.0e-5, "under 0 dBFS nothing is touched");
        }
        {
            // A clipping bass hit under a steady 2 kHz tone: the bass comes down, the tone stays
            FinalLimiter lim;
            lim.prepare (sr);
            const int n = (int) (3.0 * sr);
            std::vector<float> l ((size_t) n), bassOnly ((size_t) n), tone ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                const float bass = t > 1.0 && t < 2.0 ? 1.6f * (float) std::sin (twoPi * 55.0 * t) : 0.0f;
                tone[(size_t) i] = 0.1f * (float) std::sin (twoPi * 2000.0 * t);
                l[(size_t) i] = bass + tone[(size_t) i];
            }
            auto r = l;
            float* ch[2] { l.data(), r.data() };
            lim.process (ch, 2, n);
            enh::dsp::BiquadCoeffs bp = enh::dsp::BiquadCoeffs::bandPass (sr, 2000.0, 4.0);
            enh::dsp::BiquadState s1, s2;
            double before = 0.0, during = 0.0, peak = 0.0;
            int nb = 0, nd = 0;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                const float d = s2.process (bp, s1.process (bp, l[(size_t) i]));
                peak = std::max (peak, (double) std::abs (l[(size_t) i]));
                if (t > 0.5 && t < 0.95) { before += d * d; ++nb; }
                if (t > 1.2 && t < 1.9) { during += d * d; ++nd; }
            }
            const double dip = 10.0 * std::log10 ((during / nd) / (before / nb));
            std::printf ("  +4 dB bass over a 2 kHz tone: low region cut %.1f dB, tone %+.2f dB, peak %.3f\n",
                         lim.getRegionCutDb()[0], dip, peak);
            check (peak <= 1.0 && dip > -1.0, "a clipping bass hit is cut in the bass: the 2 kHz tone stays within 1 dB, output under 0 dBFS");
        }

        std::printf ("\n== MIX BALANCER ==\n");
        {
            auto balance = [&] (bool loudAll, bool burst, int& cutBand, float& cutDb, float& otherMax)
            {
                MixBalancer b;
                b.prepare (sr, 2);
                MixBalancer::Settings s;
                s.active = true; s.amount = 0.6f; s.speed = 0.5f; s.rangeDb = 8.0f;
                const int n = (int) (8.0 * sr);
                std::vector<float> l ((size_t) n);
                juce::Random rnd (3);
                enh::dsp::BiquadCoeffs pink = enh::dsp::BiquadCoeffs::lowPass (sr, 3000.0, 0.5);
                enh::dsp::BiquadState ps;
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr;
                    float x = ps.process (pink, rnd.nextFloat() * 2.0f - 1.0f) * 0.2f;
                    if (loudAll && t > 5.0) x *= 3.16f;                                   // everything +10 dB
                    if (burst && t > 5.0) x += 0.25f * (float) std::sin (twoPi * 500.0 * t);  // one region jumps
                    l[(size_t) i] = x;
                }
                auto r = l;
                // In blocks, noting each band's largest move over the second after the change
                std::array<float, MixBalancer::numBands> most {};
                for (int pos = 0; pos < n; pos += 256)
                {
                    const int m = std::min (256, n - pos);
                    float* ch[2] { l.data() + pos, r.data() + pos };
                    b.process (ch, 2, m, s);
                    const double t = pos / sr;
                    if (std::getenv ("BAL_TRACE") != nullptr && burst && t > 4.9 && t < 6.3 && pos % 4800 < 256)
                        std::printf ("   t %.2f  500 Hz band %+.2f dB\n", t, b.getGainDb (2));
                    if (t > 5.1 && t < 6.2)
                        for (int k = 0; k < MixBalancer::numBands; ++k)
                            if (std::abs (b.getGainDb (k)) > std::abs (most[(size_t) k]))
                                most[(size_t) k] = b.getGainDb (k);
                }
                if (std::getenv ("BAL_DIAG") != nullptr)
                {
                    std::printf ("   bands:");
                    for (int k = 0; k < MixBalancer::numBands; ++k) std::printf (" %.0f:%+.2f", MixBalancer::centreHz[(size_t) k], most[(size_t) k]);
                    std::printf ("\n");
                }
                cutBand = 0; cutDb = 0.0f; otherMax = 0.0f;
                for (int k = 0; k < MixBalancer::numBands; ++k)
                    if (most[(size_t) k] < cutDb) { cutDb = most[(size_t) k]; cutBand = k; }
                for (int k = 0; k < MixBalancer::numBands; ++k)
                    if (k != cutBand) otherMax = std::max (otherMax, std::abs (most[(size_t) k]));
            };
            int band = 0; float cut = 0.0f, other = 0.0f;
            balance (false, true, band, cut, other);
            std::printf ("  a 500 Hz region jumps out: band %.0f Hz cut %.1f dB, the others within %.1f dB\n", MixBalancer::centreHz[(size_t) band], cut, other);
            check (band == 2 && cut < -2.0f && other < std::abs (cut), "the region that jumps out is the one taken down, and the most");
            balance (true, false, band, cut, other);
            std::printf ("  everything +10 dB together: deepest move %.2f dB\n", std::min (cut, -other));
            check (cut > -0.5f && other < 0.5f, "a louder mix overall is not a reason to move (balance, not loudness)");
        }
        {
            // Loudness keeper: a 500 Hz burst (5-6 s) is taken down. Taking the burst away loses nothing, so
            // nothing is made up while it lasts; once it stops, the fader is still letting go and holds the
            // region below its usual level, and then the rest of the mix (a 4 kHz tone) is lifted so the
            // whole does not sound quieter - by part of the loss, never more than 3 dB
            auto run = [&] (bool burst, float& restDb, float& keepDb, float& cutDb, float& duringDb)
            {
                MixBalancer b;
                b.prepare (sr, 2);
                MixBalancer::Settings s;
                s.active = true; s.amount = 0.8f; s.speed = 0.5f; s.rangeDb = 10.0f;
                const int n = (int) (8.0 * sr);
                std::vector<float> l ((size_t) n), in;
                juce::Random rnd (3);
                enh::dsp::BiquadCoeffs pink = enh::dsp::BiquadCoeffs::lowPass (sr, 3000.0, 0.5);
                enh::dsp::BiquadState ps;
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr;
                    l[(size_t) i] = ps.process (pink, rnd.nextFloat() * 2.0f - 1.0f) * 0.2f
                                  + 0.03f * (float) std::sin (twoPi * 4000.0 * t)   // the rest of the mix, as a probe
                                  + (burst && t > 5.0 && t < 6.0 ? 0.3f * (float) std::sin (twoPi * 500.0 * t) : 0.0f);
                }
                in = l;
                auto r = l;
                keepDb = 0.0f; cutDb = 0.0f; duringDb = 0.0f;
                for (int pos = 0; pos < n; pos += 256)
                {
                    const int m = std::min (256, n - pos);
                    float* ch[2] { l.data() + pos, r.data() + pos };
                    b.process (ch, 2, m, s);
                    if (std::getenv ("KEEP_DIAG") != nullptr && pos % 12288 == 0)
                        std::printf ("   t %.2f keep %.2f gains %+.1f %+.1f %+.1f %+.1f %+.1f %+.1f\n", pos / sr, b.getMakeupDb(),
                                     b.getGainDb (0), b.getGainDb (1), b.getGainDb (2), b.getGainDb (3), b.getGainDb (4), b.getGainDb (5));
                    if (pos / sr > 5.3 && pos / sr < 5.95)
                        duringDb = std::max (duringDb, b.getMakeupDb());
                    if (pos / sr > 5.0)
                    {
                        keepDb = std::max (keepDb, b.getMakeupDb());
                        cutDb = std::min (cutDb, b.getGainDb (2));
                    }
                }
                // Output against input, in a band well away from the cut, over the second half of the burst
                auto band = [&] (const std::vector<float>& x)
                {
                    auto c = enh::dsp::BiquadCoeffs::bandPass (sr, 4000.0, 8.0);
                    enh::dsp::BiquadState a, a2;
                    double e = 0.0;
                    for (int i = 0; i < n; ++i)
                    {
                        const float y = a2.process (c, a.process (c, x[(size_t) i]));
                        if (i > (int) (6.05 * sr) && i < (int) (6.6 * sr)) e += (double) y * y;
                    }
                    return 10.0 * std::log10 (e + 1e-20);
                };
                restDb = (float) (band (l) - band (in));
            };
            float rest = 0.0f, keep = 0.0f, cut = 0.0f, during = 0.0f;
            run (true, rest, keep, cut, during);
            std::printf ("  loudness keeper: 500 Hz burst cut %.1f dB, keeper %.2f dB while it lasts; as the cut lets go the rest is lifted %+.2f dB (keeper up to %.2f dB)\n",
                         cut, during, rest, keep);
            check (cut < -2.0f && during < 0.3f, "taking a burst away is not made up (it was never part of the mix)");
            check (rest > 0.3f && rest < 3.05f && keep <= 3.0f,
                   "while a cut holds a region below its usual level the rest is lifted to hold the loudness, by at most 3 dB");
            run (false, rest, keep, cut, during);
            std::printf ("  loudness keeper, nothing cut: rest %+.2f dB, keeper %.2f dB\n", rest, keep);
            check (std::abs (rest) < 0.3f && keep < 0.3f, "with nothing cut the keeper does nothing");
        }
        {
            // RESOLUTION at spectral: a narrow 2.5 kHz whistle is cut in its own third-octave, and an octave
            // either side hardly moves
            MixBalancer b;
            b.prepare (sr, 2);
            MixBalancer::Settings s;
            s.active = true; s.amount = 0.6f; s.speed = 0.5f; s.rangeDb = 8.0f; s.resolution = 1.0f;
            const int n = (int) (8.0 * sr);
            std::vector<float> l ((size_t) n);
            juce::Random rnd (5);
            enh::dsp::BiquadCoeffs pink = enh::dsp::BiquadCoeffs::lowPass (sr, 3000.0, 0.5);
            enh::dsp::BiquadState ps;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr;
                l[(size_t) i] = ps.process (pink, rnd.nextFloat() * 2.0f - 1.0f) * 0.2f + (t > 5.0 ? 0.08f * (float) std::sin (twoPi * 2500.0 * t) : 0.0f);
            }
            auto r = l;
            std::array<float, MixBalancer::numFine> most {};
            for (int pos = 0; pos < n; pos += 256)
            {
                const int m = std::min (256, n - pos);
                float* ch[2] { l.data() + pos, r.data() + pos };
                b.process (ch, 2, m, s);
                if (pos / sr > 5.1 && pos / sr < 6.2)
                    for (int k = 0; k < MixBalancer::numFine; ++k)
                        most[(size_t) k] = std::min (most[(size_t) k], b.getFineGainDb (k));
            }
            int deepestBand = 0;
            for (int k = 1; k < MixBalancer::numFine; ++k)
                if (most[(size_t) k] < most[(size_t) deepestBand]) deepestBand = k;
            const int at = deepestBand;
            const float octaveAway = std::min (std::abs (most[(size_t) std::max (0, at - 3)]), std::abs (most[(size_t) std::min (MixBalancer::numFine - 1, at + 3)]));
            std::printf ("  spectral mode, a 2.5 kHz whistle: deepest cut %.1f dB at %.0f Hz, an octave either side %.1f dB\n",
                         most[(size_t) at], MixBalancer::fineHz (at), -octaveAway);
            check (std::abs (MixBalancer::fineHz (at) - 2500.0f) < 400.0f && most[(size_t) at] < -2.0f && octaveAway < 0.5f * std::abs (most[(size_t) at]),
                   "spectral mode cuts the whistle in its own third-octave, not the octave around it");
        }
    }

    //==========================================================================
    /** The method registry as the reference page in the Vault (EnhDspTests --methods-doc writes it). */
    juce::String methodsDoc()
    {
        namespace m = enh::dsp::methods;
        auto s = [] (std::string_view v) { return juce::String (v.data(), v.size()); };
        juce::String out;
        out << "# Every glass-panel setting\n\n"
            << juce::String::fromUTF8 ("> \xf0\x9f\x94\x8e **[Searchbar](../../Searchbar.md)** \xe2\x80\x94 find any doc, setting, function or GitHub page (Ctrl+F)\n\n")
            << "Click a unit to open its glass panel. Each setting below has two or three choices; the **first is the\n"
            << "default** and is the unit's original sound. Switching never clicks and never changes the plugin's delay.\n"
            << "Settings are saved with your session and presets leave them alone. **RESET TO DEFAULTS** puts a unit back.\n\n"
            << juce::String::fromUTF8 ("*Made from `Source/DSP/MethodRegistry.h` by `EnhDspTests --methods-doc` \xe2\x80\x94 don't edit by hand.* ")
            << "Back to [Start here](../00%20Start%20Here.md).\n";
        for (int unit : m::unitsInRackOrder)
        {
            const auto list = m::stagesForUnit (unit);
            if (list.count == 0)
                continue;
            out << "\n## " << s (list.stages[0].unit) << "\n";
            for (int i = 0; i < list.count; ++i)
            {
                const auto& st = list.stages[i];
                out << "\n### " << s (st.name) << (st.knobParam.empty() ? juce::String() : " (" + s (st.knobParam) + " knob)")
                    << " - " << s (st.question) << "\n\n";
                out << s (st.category) << ". Parameter `" << s (st.param) << "`.\n\n";
                out << "| Method | What it measures / does | How the sound changes | CPU / latency |\n|---|---|---|---|\n";
                for (int k = 0; k < st.numMethods; ++k)
                {
                    const auto& me = st.methods[k];
                    out << "| **" << s (me.shortName) << "** " << s (me.fullName) << (k == 0 ? " (default)" : "")
                        << " | " << s (me.measures) << " | " << s (me.sound) << " | " << s (me.cost) << " |\n";
                }
            }
        }

        out << "\n## Knob modifiers\n\n"
            << "Every continuous knob has these, between the knob and its processing (in the panel, under KNOBS). They\n"
            << "are stored in the session, not as parameters: the host always sees the knob's raw value. With all\n"
            << "three off the knob is exactly as it always was.\n\n"
            << "| Modifier | What it does | How the sound changes | Cost | Settings |\n|---|---|---|---|---|\n";
        for (auto& mo : m::modifiers)
        {
            juce::String choices;
            for (auto& l : mo.labels)
                choices << (choices.isEmpty() ? "**" + s (l) + "** (off)" : ", " + s (l));
            out << "| **" << s (mo.shortName) << "** " << s (mo.fullName) << " | " << s (mo.does) << " | " << s (mo.sound)
                << " | " << s (mo.cost) << " | " << choices << " |\n";
        }
        out << "\nKnobs with modifiers: ";
        for (size_t k = 0; k < enh::dsp::knobFields.size(); ++k)
            out << (k == 0 ? "" : ", ") << "`" << enh::dsp::knobFields[k].param << "`";
        out << ".\n";
        return out;
    }

    juce::File methodsDocFile()
    {
       #ifdef ENH_SOURCE_DIR
        if (const juce::File source (ENH_SOURCE_DIR); source.getChildFile ("Vault").isDirectory())
            return source.getChildFile ("Vault/Reference/Methods.md");
       #endif
        // Otherwise look for Vault/ above the working directory
        for (auto dir = juce::File::getCurrentWorkingDirectory(); dir.exists() && ! dir.isRoot(); dir = dir.getParentDirectory())
            if (dir.getChildFile ("Vault").isDirectory())
                return dir.getChildFile ("Vault/Reference/Methods.md");
        return {};
    }

    void runMethodTests (double sr)
    {
        namespace m = enh::dsp::methods;
        using enh::dsp::DynamicCompressor;
        std::printf ("\n== PROCESSING METHODS (the glass panel's stages) ==\n");

        // The registry and the parameters agree: every stage's parameter exists, is a choice, not
        // automatable, defaults to the first method, and lists the methods in the same order
        {
            bool ok = true;
            std::array<int, m::numMethodIds> seen {};
            for (int unit : m::unitsInRackOrder)
            for (int i = 0; i < m::stagesForUnit (unit).count; ++i)
            {
                const auto& st = m::stagesForUnit (unit).stages[i];
                ok = ok && st.unitIndex == unit && st.id >= 0 && st.id < m::numMethodIds;
                if (ok) ++seen[(size_t) st.id];
                const auto* spec = pad::params::findSpec (juce::String (st.param.data(), st.param.size()));
                ok = ok && spec != nullptr && spec->kind == pad::params::Kind::choice && ! spec->automatable
                        && spec->defaultValue == 0.0f && spec->texts.size() == st.numMethods;
                for (int k = 0; ok && k < st.numMethods; ++k)
                    ok = spec->texts[k] == juce::String (st.methods[k].shortName.data(), st.methods[k].shortName.size());
            }
            for (int c : seen)
                ok = ok && c == 1;
            check (ok, "every stage in the registry has its parameter: a choice, not automatable, default = first method, same order; every method id once");
        }
        {
            const auto file = methodsDocFile();
            // (line endings aside: git on Windows checks the page out with CRLF)
            const bool same = file.existsAsFile() && file.loadFileAsString().removeCharacters ("\r") == methodsDoc();
            if (! same) std::printf ("  %s is out of date: run EnhDspTests --methods-doc\n", file.getFullPathName().toRawUTF8());
            check (same, "Vault/Reference/Methods.md matches the registry");
        }

        // Every method of every stage through the whole engine, one at a time (everything else at its
        // default), at 48 kHz and three block sizes: finite, under the ceiling, the same latency, and it
        // changes the sound (a display setting must not); then every stage at its last method together,
        // at 44.1 and 96 kHz too
        {
            auto base = [] (enh::dsp::KnobValues& k)
            {
                k.tape = true; k.footstep = true; k.clarityAddMode = true; k.clarityAdd = 6.0f;   // everything in the path
                k.balAmount = 7.0f; k.balResolution = 5.0f;
                k.deepDepth = 6.0f; k.deepHull = 5.0f; k.deepPressure = 3.0f;
            };
            auto latencyAt = [] (double rate) { EnhEngine e; e.prepare (rate, 128, 2); return e.getLatencySamples(); };
            // Scenes that exercise each stage: the game scene; for the SPECTRAL LIMITER a long mix with kicks
            // and a huge rumble at 8 s (it needs a few seconds to learn what is normal); for the leveler's
            // LIFT quiet noise (quiet enough to reach its limits)
            const auto gameScene = makeScene (sr, 2.0, true, true, 11);
            // The FOOTSTEP RADAR's settings need steps to work on (and some it should not take)
            const auto radarScene = []
            {
                const auto a = radartests::methodScene();
                Scene sc;
                sc.left = a.l;
                sc.right = a.r;
                return sc;
            }();
            Scene jumpScene;    // the EAR GUARD's: 5 s quiet, then a blast 30 dB louder - held at 12, 15 or 18 dB over
            {
                juce::Random rnd (6);
                Pink pk;
                for (int i = 0; i < (int) (8.0 * sr); ++i)
                    jumpScene.left.push_back (dbfs (i < (int) (5.0 * sr) ? -46.0f : -16.0f) * pk.next (rnd) * 4.0f);
                jumpScene.right = jumpScene.left;
            }
            Scene quietScene;   // quiet pink noise, -34 dBFS, 8 s: the leveler lifts it well past LIFT's limits
            {
                juce::Random rnd (4);
                Pink pk;
                for (int i = 0; i < (int) (8.0 * sr); ++i)
                    quietScene.left.push_back (dbfs (-34.0f) * pk.next (rnd) * 4.0f);
                quietScene.right = quietScene.left;
            }
            Scene limiterScene;
            {
                const int n = (int) (10.5 * sr);
                limiterScene.left.resize ((size_t) n);
                limiterScene.right.resize ((size_t) n);
                juce::Random rnd (21);
                Pink pinkL, pinkR;
                BiquadState eA, eB;
                const auto rumbleLp = BiquadCoeffs::lowPass (sr, 110.0, 0.8);
                for (int i = 0; i < n; ++i)
                {
                    const double t = i / sr, kt = std::fmod (t, 0.5);
                    const float kick = dbfs (-10.0f) * (float) (std::sin (twoPi * (58.0 + 40.0 * std::exp (-kt / 0.02)) * kt) * std::exp (-kt / 0.11));
                    float ev = 0.0f;
                    if (t >= 8.0 && t < 8.5)
                        ev = (float) std::min (1.0, (t - 8.0) / 0.01) * (0.75f * (float) std::sin (twoPi * 72.0 * (t - 8.0))
                                                                       + 1.6f * eA.process (rumbleLp, eB.process (rumbleLp, rnd.nextFloat() * 2.0f - 1.0f)));
                    limiterScene.left[(size_t) i] = 0.10f * pinkL.next (rnd) + kick + ev;
                    limiterScene.right[(size_t) i] = 0.10f * pinkR.next (rnd) + kick + ev;
                }
            }
            Scene hotScene;   // the same mix pushed to the top: the only way a ceiling shows at all
            {
                hotScene = limiterScene;
                for (auto* v : { &hotScene.left, &hotScene.right })
                    for (auto& x : *v)
                        x = std::clamp (x * 4.0f, -0.999f, 0.999f);
            }
            auto sceneFor = [&] (const m::Stage& st) -> const Scene&
            {
                // A ceiling only shows on material that reaches it, so it gets the scene pushed to the top
                return st.id == m::outputCeiling ? hotScene
                     : st.unitIndex == 4 ? limiterScene
                     : st.id == m::levelerLift ? quietScene : st.id == m::earGuard ? jumpScene
                     : st.unitIndex == 10 ? radarScene : gameScene;
            };
            std::map<std::pair<const Scene*, bool>, RunResult> references;   // per scene, with CHARACTER in or out
            int runs = 0, failed = 0, placebo = 0;
            juce::StringArray problems;
            for (int unit : m::unitsInRackOrder)
                for (int i = 0; i < m::stagesForUnit (unit).count; ++i)
                {
                    const auto& st = m::stagesForUnit (unit).stages[i];
                    const bool display = st.category == "DISPLAY";
                    const float ceiling = st.id == m::outputCeiling ? 1.0f : 1.0f;
                    for (int k = 1; k < st.numMethods; ++k)
                    {
                        // A unit that is out by default (CHARACTER) is put in for its own methods
                        const auto p = presetParameters (presetNamed ("IMMERSIVE GAMES"), [&] (auto& kv) { base (kv); kv.methods[(size_t) st.id] = k; if (st.unitIndex == 9) kv.charActive = true; if (st.unitIndex == 10) kv.footstep = true; });
                        const float limit = st.id == m::outputCeiling ? (k == 1 ? 0.96605088f : 0.89125094f) + 1.0e-6f : ceiling;
                        bool ok = true;
                        float diff = 0.0f;
                        const auto& scene = sceneFor (st);
                        const auto key = std::make_pair (&scene, st.unitIndex == 9);
                        if (references.count (key) == 0)
                            references[key] = run (scene, sr, 128, presetParameters (presetNamed ("IMMERSIVE GAMES"),
                                                                                      [&] (auto& kv) { base (kv); if (st.unitIndex == 9) kv.charActive = true; if (st.unitIndex == 10) kv.footstep = true; }));
                        const auto& reference = references[key];
                        for (int bs : { 7, 128, 1024 })
                        {
                            bool sameLatency = true;
                            const auto r = run (scene, sr, bs, p, [&] (const EnhEngine& e) { sameLatency = e.getLatencySamples() == latencyAt (sr); });
                            ok = ok && r.finite && r.peak <= limit && sameLatency;
                            if (bs == 128)
                                for (size_t n = 0; n < r.outL.size(); ++n)
                                    diff = std::max (diff, std::abs (r.outL[n] - reference.outL[n]));
                            ++runs;
                        }
                        const auto name = juce::String (st.unit.data(), st.unit.size()) + " " + juce::String (st.name.data(), st.name.size())
                                        + " " + juce::String (st.methods[k].shortName.data(), st.methods[k].shortName.size());
                        if (! ok) { ++failed; problems.add (name + " (unstable / over / latency)"); }
                        // GLIDE only shows when LEVEL moves (tested below); a display setting must leave the audio alone
                        if (display ? diff != 0.0f : (diff < 1.0e-5f && st.id != m::levelGlide)) { ++placebo; problems.add (name + (display ? " (changes the audio)" : " (changes nothing)")); }
                    }
                }
            std::printf ("  every method of every stage, one at a time: %d runs (block sizes 7, 128, 1024)\n", runs);
            for (auto& pr : problems)
                std::printf ("    %s\n", pr.toRawUTF8());
            check (failed == 0, "every method is finite, under the ceiling and keeps the latency, at block sizes 7, 128 and 1024");
            check (placebo == 0, "every processing method changes the sound on the test scene; display settings change nothing");

            // Everything at its last method at once, at every rate
            bool allOk = true;
            for (double rate : { 44100.0, 48000.0, 96000.0 })
            {
                const auto p = presetParameters (presetNamed ("IMMERSIVE GAMES"), [&] (auto& kv)
                {
                    base (kv);
                    for (int unit : m::unitsInRackOrder)
                        for (int i = 0; i < m::stagesForUnit (unit).count; ++i)
                            kv.methods[(size_t) m::stagesForUnit (unit).stages[i].id] = m::stagesForUnit (unit).stages[i].numMethods - 1;
                });
                bool sameLatency = true;
                const auto r = run (makeScene (rate, 2.0, true, true, 12), rate, 128, p,
                                    [&] (const EnhEngine& e) { sameLatency = e.getLatencySamples() == latencyAt (rate); });
                allOk = allOk && r.finite && r.peak <= 1.0f && sameLatency;
            }
            check (allOk, "every stage at its last method together: stable, under full scale, same latency at 44.1, 48 and 96 kHz");
        }

        // Switching while the audio runs never steps it: a smooth test signal, the method switched half
        // way; the biggest sample-to-sample change right after the switch is no bigger than it already was
        {
            juce::StringArray stepped;
            auto switchTest = [&] (int id, int to, std::function<void (enh::dsp::KnobValues&)> setup)
            {
                const int n = (int) (1.6 * sr), block = 64;
                std::vector<float> l ((size_t) n);
                for (int i = 0; i < n; ++i)
                    l[(size_t) i] = 0.25f * (float) std::sin (twoPi * 110.0 * i / sr) + 0.12f * (float) std::sin (twoPi * 330.0 * i / sr)
                                  + (i % (int) (0.25 * sr) < (int) (0.01 * sr) ? 0.2f * (float) std::sin (twoPi * 1000.0 * i / sr) : 0.0f);
                auto r = l;
                EnhEngine e;
                e.prepare (sr, block, 2);
                auto before = presetParameters (presetNamed ("IMMERSIVE GAMES"), setup);
                auto after = presetParameters (presetNamed ("IMMERSIVE GAMES"), [&] (auto& kv) { setup (kv); kv.methods[(size_t) id] = to; });
                juce::AudioBuffer<float> buf (2, block);
                float prev = 0.0f, stepBefore = 0.0f, stepAfter = 0.0f;
                for (int pos = 0; pos < n; pos += block)
                {
                    const int len = std::min (block, n - pos);
                    buf.setSize (2, len, false, false, true);
                    std::copy_n (l.data() + pos, len, buf.getWritePointer (0));
                    std::copy_n (r.data() + pos, len, buf.getWritePointer (1));
                    e.process (buf, pos >= n / 2 ? after : before);
                    for (int i = 0; i < len; ++i)
                    {
                        const float y = buf.getSample (0, i);
                        const float step = std::abs (y - prev);
                        prev = y;
                        const int at = pos + i;
                        if (at > n / 2 - (int) (0.5 * sr) && at < n / 2) stepBefore = std::max (stepBefore, step);
                        if (at >= n / 2 && at < n / 2 + (int) (0.05 * sr)) stepAfter = std::max (stepAfter, step);
                    }
                }
                if (stepAfter > 1.25f * stepBefore + 1.0e-3f)
                    stepped.add (juce::String (id) + ": " + juce::String (stepAfter, 4) + " after vs " + juce::String (stepBefore, 4));
            };
            auto tape = [] (enh::dsp::KnobValues& k) { k.tape = true; };
            auto add = [] (enh::dsp::KnobValues& k) { k.clarityAddMode = true; k.clarityAdd = 10.0f; };
            auto none = [] (enh::dsp::KnobValues&) {};
            switchTest (m::seraphTape, 1, tape);      switchTest (m::seraphTape, 2, tape);
            switchTest (m::seraphPreDelay, 1, none);  switchTest (m::seraphPreDelay, 2, none);
            switchTest (m::enhancerHarmonics, 1, add); switchTest (m::enhancerHarmonics, 2, add);
            switchTest (m::tideDetector, 2, none);    switchTest (m::tideSideChain, 2, none);
            switchTest (m::tideGain, 2, none);        switchTest (m::tideSmoothing, 2, none);
            switchTest (m::outputCeiling, 2, none);   switchTest (m::levelerLift, 2, none);
            auto deepOn = [] (enh::dsp::KnobValues& k) { k.deepDepth = 8.0f; k.deepHull = 6.0f; };
            switchTest (m::deepShape, 2, deepOn);     switchTest (m::deepMaterial, 2, deepOn);
            for (auto& s : stepped)
                std::printf ("    stepped: %s\n", s.toRawUTF8());
            check (stepped.isEmpty(), "switching a method while audio runs never steps it (tape, pre-delay, harmonics, compressor, ceiling, leveler, deep sub)");
        }

        // GLIDE: LEVEL moved 12 dB; how long until it has covered 90 % of the way
        {
            auto settle = [&] (int glide)
            {
                EnhEngine e;
                e.prepare (sr, 32, 2);
                auto p = presetParameters (presetNamed ("TRANSPARENT (ALL OUT)"), [&] (auto& kv) { kv.methods[(size_t) m::levelGlide] = glide; });
                juce::AudioBuffer<float> buf (2, 32);
                const int n = (int) (0.6 * sr);
                int at90 = -1;
                for (int pos = 0; pos < n; pos += 32)
                {
                    buf.setSize (2, 32, false, false, true);
                    for (int c = 0; c < 2; ++c) for (int i = 0; i < 32; ++i) buf.setSample (c, i, 0.1f);
                    p.levelDb = pos >= (int) (0.1 * sr) ? -12.0f : 0.0f;
                    e.process (buf, p);
                    if (at90 < 0 && pos >= (int) (0.1 * sr) && e.getMeters().levelDb.load() < -10.8f)
                        at90 = pos - (int) (0.1 * sr);
                }
                return 1000.0 * at90 / sr;
            };
            const double stdMs = settle (0), fastMs = settle (1), slowMs = settle (2);
            std::printf ("  GLIDE, LEVEL -12 dB: 90 %% of the way in %.0f ms (STD) / %.0f ms (FST) / %.0f ms (SLW)\n", stdMs, fastMs, slowMs);
            check (fastMs < stdMs && stdMs < slowMs && slowMs > 200.0, "GLIDE: FST is quicker and SLW slower than the 20 ms default");
        }

        // The compressor alone gives the same output whatever the block size, for every combination
        {
            bool same = true;
            for (int det = 0; det < DynamicCompressor::numDetectors; ++det)
                for (int smo = 0; smo < DynamicCompressor::numSmoothings; ++smo)
                {
                    auto once = [&] (int bs)
                    {
                        DynamicCompressor c;
                        c.prepare (sr, 2);
                        DynamicCompressor::Settings s;
                        s.active = true; s.mix = 1.0f; s.response = 0.7f; s.detector = det; s.smoothing = smo;
                        const auto scene = makeScene (sr, 2.0, true, true, 4);
                        auto l = scene.left, r = scene.right;
                        for (int pos = 0; pos < (int) l.size(); pos += bs)
                        {
                            float* ch[2] { l.data() + pos, r.data() + pos };
                            c.process (ch, 2, std::min (bs, (int) l.size() - pos), s);
                        }
                        return l;
                    };
                    same = same && once (7) == once (1024);
                }
            check (same, "the compressor's output is identical at block sizes 7 and 1024, for every method");
        }

        // Switching methods while it compresses: the 30 ms crossfade, and the incoming method starting from
        // the gain being applied, keep the gain moving no faster than it does anyway
        {
            DynamicCompressor c;
            c.prepare (sr, 2);
            DynamicCompressor::Settings s;
            s.active = true; s.mix = 1.0f; s.response = 0.7f;
            // Dense pink noise at -12 dBFS: it is compressing, steadily, when the switch comes
            const int len = (int) (6.0 * sr);
            std::vector<float> l ((size_t) len), r;
            {
                juce::Random rnd (12);
                Pink pk;
                for (auto& x : l) x = dbfs (-12.0f) * pk.next (rnd) * 4.0f;
                r = l;
            }
            float prev = 0.0f, stepBefore = 0.0f, stepAfter = 0.0f, grAtSwitch = 0.0f;
            for (int i = 0; i < (int) l.size(); ++i)
            {
                const double t = i / sr;
                if (t >= 3.0) { s.detector = DynamicCompressor::detectorRms; s.smoothing = DynamicCompressor::smoothingSrl; }
                float* ch[2] { l.data() + i, r.data() + i };
                c.process (ch, 2, 1, s);
                const float gr = c.getReadout().gainReductionDb;
                const float step = std::abs (gr - prev);
                prev = gr;
                if (t > 2.0 && t < 3.0) stepBefore = std::max (stepBefore, step);
                if (t >= 3.0 && t < 3.05) stepAfter = std::max (stepAfter, step);
                if (i == (int) (3.0 * sr) - 1) grAtSwitch = gr;
            }
            std::printf ("  switching PKR/DRL -> RMS/SRL while compressing %.1f dB: largest gain step %.4f dB/sample (%.4f in the second before)\n",
                         grAtSwitch, stepAfter, stepBefore);
            check (grAtSwitch > 1.0f && stepAfter <= std::max (0.01f, 1.5f * stepBefore), "a method switch does not step the gain (crossfaded)");
            check (c.getDetector() == DynamicCompressor::detectorRms && c.getSmoothing() == DynamicCompressor::smoothingSrl, "the switch lands on the new methods");
        }

        // The methods sound different (otherwise the panel would be a placebo)
        {
            auto grWith = [&] (int det, int smo)
            {
                DynamicCompressor c;
                c.prepare (sr, 2);
                DynamicCompressor::Settings s;
                s.active = true; s.mix = 1.0f; s.response = 0.6f; s.detector = det; s.smoothing = smo;
                // Sparse loud hits over a quiet bed (high crest factor), as in the TIDE tests
                const int len = (int) (5.0 * sr);
                std::vector<float> l ((size_t) len), r;
                {
                    juce::Random rnd (13);
                    Pink pk;
                    for (int i = 0; i < len; ++i)
                        l[(size_t) i] = dbfs (-12.0f) * pk.next (rnd) * 4.0f * ((i % (int) (0.4 * sr) < (int) (0.02 * sr)) ? 3.0f : 0.05f);
                    r = l;
                }
                double sum = 0.0; int n = 0;
                float most = 0.0f;
                for (int pos = 0; pos < (int) l.size(); pos += 128)
                {
                    float* ch[2] { l.data() + pos, r.data() + pos };
                    c.process (ch, 2, std::min (128, (int) l.size() - pos), s);
                    if (pos > 2.0 * sr) { sum += c.getReadout().gainReductionDb; ++n; }
                    if (pos > 2.0 * sr) most = std::max (most, c.getReadout().gainReductionDb);
                }
                return std::make_pair ((float) (sum / std::max (1, n)), most);
            };
            const auto pkr = grWith (0, 0), rms = grWith (1, 0), srl = grWith (0, 1);
            std::printf ("  sparse hits, gain reduction on the hits / on average: PKR/DRL %.2f / %.2f dB, RMS/DRL %.2f / %.2f dB, PKR/SRL %.2f / %.2f dB\n",
                         pkr.second, pkr.first, rms.second, rms.first, srl.second, srl.first);
            check (rms.second < pkr.second - 0.5f, "RMS takes less off the hits than PKR (transients pass through fuller)");
            check (std::abs (srl.first - pkr.first) > 0.05f, "SRL moves differently from DRL");
        }

        // RESPONSE's law: EXP spends more of the travel at the slow end, same ends
        {
            auto resp = [] (float knob, int law) { enh::dsp::KnobValues k; k.tideResponse = knob; k.methods[(size_t) m::tideResponseLaw] = law; return enh::dsp::mapKnobs (k).tide.response; };
            std::printf ("  RESPONSE 5: LIN %.2f, EXP %.2f, LOG %.2f\n", resp (5.0f, 0), resp (5.0f, 1), resp (5.0f, 2));
            check (resp (0.0f, 1) == 0.0f && std::abs (resp (10.0f, 1) - 1.0f) < 1.0e-6f && resp (5.0f, 1) < 0.35f && resp (5.0f, 0) == 0.5f
                   && std::abs (resp (0.0f, 2)) < 1.0e-6f && std::abs (resp (10.0f, 2) - 1.0f) < 1.0e-6f && resp (5.0f, 2) > 0.65f,
                   "RESPONSE LAW: LIN is the travel, EXP and LOG have the same ends and more travel at the slow / fast end");
        }

        // SMO: the same glide at every sample rate and block size (it is worked out in seconds)
        {
            // Wherever it has got to, it is where a 250 ms one-pole would be after that much time in seconds
            float worst = 0.0f;
            for (double rate : { 44100.0, 48000.0, 96000.0 })
                for (int bs : { 1, 32, 512 })
                {
                    pad::KnobSmoother smo;
                    smo.process (0.0f, 250.0f, bs, rate);          // primes at 0
                    const int blocks = (int) std::lround (0.25 * rate / bs);
                    float v = 0.0f;
                    for (int b = 0; b < blocks; ++b)
                        v = smo.process (10.0f, 250.0f, bs, rate);
                    const double seconds = blocks * bs / rate;
                    worst = std::max (worst, std::abs (v - (float) (10.0 * (1.0 - std::exp (-seconds / 0.25)))));
                }
            std::printf ("  SMO 250 ms: largest error against the one-pole in seconds %.2e (3 rates x 3 block sizes)\n", worst);
            check (worst < 1.0e-3f, "SMO glides the same at every sample rate and block size");
            pad::KnobSmoother off;
            off.process (2.0f, 0.0f, 64, sr);
            check (off.process (7.0f, 0.0f, 64, sr) == 7.0f, "SMO off passes the knob straight through");
        }

        // CRV and LIM on a knob's travel; all three off leaves the value exactly as it was
        {
            juce::NormalisableRange<float> range (30.0f, 600.0f);
            range.setSkewForCentre (150.0f);   // like RELEASE: skewed
            bool identity = true, ends = true;
            for (float v : { 30.0f, 77.7f, 150.0f, 333.3f, 600.0f })
            {
                pad::KnobSmoother s;
                identity = identity && pad::applyKnobModifiers (v, range, s, 0.0f, 0, 100.0f, 64, sr) == v;
            }
            for (int curve = 0; curve < 4; ++curve)
            {
                pad::KnobSmoother a, b;
                ends = ends && std::abs (pad::applyKnobModifiers (30.0f, range, a, 0.0f, curve, 100.0f, 64, sr) - 30.0f) < 1.0e-3f
                            && std::abs (pad::applyKnobModifiers (600.0f, range, b, 0.0f, curve, 100.0f, 64, sr) - 600.0f) < 1.0e-2f;
            }
            pad::KnobSmoother c, d;
            const float half = pad::applyKnobModifiers (600.0f, range, c, 0.0f, 0, 50.0f, 64, sr);    // full travel -> half way
            const float low = pad::applyKnobModifiers (150.0f, range, d, 0.0f, 1, 100.0f, 64, sr);    // LOW: the middle comes down
            std::printf ("  modifiers on a skewed knob: LIM 50 %% at full travel %.1f (the travel's middle is 150), CRV LOW at the middle %.1f\n", half, low);
            check (identity, "a knob with every modifier off is passed through unchanged, to the bit");
            check (ends && std::abs (half - 150.0f) < 0.5f && low < 100.0f, "CRV keeps the ends; LIM 50 % stops the knob half way along its travel");
        }
    }

    //==========================================================================
    void runDeepSubTests (double sr)
    {
        using enh::dsp::DeepSub;
        std::printf ("\n== DEEP SUB ==\n");
        // A bass line (80 and 98 Hz notes, half a second each) at -12 dBFS, then silence
        auto bassLine = [&] (double seconds, double silenceFrom)
        {
            std::vector<float> x ((size_t) (seconds * sr));
            for (size_t i = 0; i < x.size(); ++i)
            {
                const double t = i / sr;
                const double f = std::fmod (t, 1.0) < 0.5 ? 80.0 : 98.0;
                x[i] = t < silenceFrom ? dbfs (-12.0f) * (float) std::sin (twoPi * f * t) : 0.0f;
            }
            return x;
        };
        auto runDeep = [&] (DeepSub::Settings s, std::vector<float> l, int block)
        {
            DeepSub d;
            d.prepare (sr);
            auto r = l;
            for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
            {
                const int len = (int) std::min ((size_t) block, l.size() - pos);
                float* ch[2] { l.data() + pos, r.data() + pos };
                d.process (ch, 2, len, s);
            }
            return l;
        };
        auto bandDb = [&] (const std::vector<float>& x, double hz, double from, double to)
        {
            auto c = BiquadCoeffs::bandPass (sr, hz, 6.0);
            BiquadState a, b;
            double e = 0.0; int n = 0;
            for (size_t i = 0; i < x.size(); ++i)
            {
                const float y = b.process (c, a.process (c, x[i]));
                if (i >= (size_t) (from * sr) && i < (size_t) (to * sr)) { e += (double) y * y; ++n; }
            }
            return (float) (10.0 * std::log10 (e / std::max (1, n) + 1e-20));
        };

        const auto input = bassLine (6.0, 4.0);
        {
            DeepSub::Settings off;
            off.active = true;   // IN, every amount at 0
            const auto out = runDeep (off, input, 128);
            check (out == input, "IN with DEPTH, HULL and PRESSURE at 0 leaves the audio exactly as it was");
        }
        DeepSub::Settings sub;
        sub.active = true; sub.depth = 0.8f;
        const auto withSub = runDeep (sub, input, 128);
        const float at40 = bandDb (withSub, 40.0, 1.0, 3.9) - bandDb (input, 40.0, 1.0, 3.9);
        const float at49 = bandDb (withSub, 49.0, 1.0, 3.9) - bandDb (input, 49.0, 1.0, 3.9);
        std::printf ("  DEPTH 8 under an 80 / 98 Hz bass line: +%.1f dB at 40 Hz, +%.1f dB at 49 Hz (an octave below each note)\n", at40, at49);
        check (at40 > 15.0f && at49 > 15.0f, "DEPTH adds a sub an octave below the bass notes");

        DeepSub::Settings hull;
        hull.active = true; hull.hull = 0.8f; hull.size = 0.7f;
        const auto withHull = runDeep (hull, input, 128);
        auto tailDb = [&] (const std::vector<float>& x)
        {
            double e = 0.0; int n = 0;
            for (size_t i = (size_t) (4.3 * sr); i < (size_t) (5.0 * sr); ++i) { e += (double) x[i] * x[i]; ++n; }
            return (float) (10.0 * std::log10 (e / std::max (1, n) + 1e-20));
        };
        std::printf ("  HULL 8: 0.3 - 1.0 s after the bass stops, the hull still rings at %.1f dBFS (dry: silence)\n", tailDb (withHull));
        check (tailDb (withHull) > -60.0f && tailDb (input) < -150.0f, "the hull rings on after the bass stops");

        // Everything up, loud bass near full scale: finite, and the same at any block size
        DeepSub::Settings all;
        all.active = true; all.depth = 1.0f; all.hull = 1.0f; all.size = 1.0f; all.pressure = 1.0f; all.shape = 2;
        auto loud = input;
        for (auto& x : loud) x *= 3.5f;
        const auto a = runDeep (all, loud, 7), b = runDeep (all, loud, 1024);
        float peak = 0.0f, diff = 0.0f;
        bool finite = true;
        for (size_t i = 0; i < a.size(); ++i)
        {
            finite = finite && std::isfinite (a[i]);
            peak = std::max (peak, std::abs (a[i]));
            diff = std::max (diff, std::abs (a[i] - b[i]));
        }
        std::printf ("  everything at 10, bass at -1 dBFS: output peak %.2f (the output limiter looks after full scale), block 7 vs 1024 differ by %.1e\n", peak, diff);
        check (finite && peak < 4.0f, "stays finite and bounded with everything at full on a near-full-scale bass");
        check (diff < 1.0e-5f, "the same at any block size");
    }

    int runNewUnitsMode (double sr)
    {
        runNewUnitTests (sr);
        runDeepSubTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    int runGolden (const juce::String& mode, const juce::File& file)
    {
        std::vector<float> out;
        auto add = [&] (const EnhEngine::Parameters& p, int blockSize)
        {
            const auto scene = makeScene (48000.0, 6.0, true, true, 7);
            const auto r = run (scene, 48000.0, blockSize, p);
            out.insert (out.end(), r.outL.begin(), r.outL.end());
            out.insert (out.end(), r.outR.begin(), r.outR.end());
        };
        {
            auto p = presetParameters (presetNamed ("DEFAULT"));
            add (p, 256);
        }
        {
            auto p = presetParameters (presetNamed ("IMMERSIVE GAMES"));   // AUTO heaven, SUB, space
            p.seraph.silk.tape = true;
            p.footstep = true;
            add (p, 100);
        }
        {
            auto p = presetParameters (presetNamed ("BASS HEAVY, PROTECTED"));
            add (p, 512);
        }

        if (mode == "write")
        {
            file.replaceWithData (out.data(), out.size() * sizeof (float));
            std::printf ("golden: wrote %d samples to %s\n", (int) out.size(), file.getFullPathName().toRawUTF8());
            return 0;
        }

        juce::MemoryBlock mb;
        if (! file.loadFileAsData (mb) || mb.getSize() != out.size() * sizeof (float))
        {
            std::printf ("golden: no matching file at %s\n", file.getFullPathName().toRawUTF8());
            return 1;
        }
        const auto* ref = static_cast<const float*> (mb.getData());
        double maxDiff = 0.0, maxRef = 0.0;
        for (size_t i = 0; i < out.size(); ++i)
        {
            maxDiff = std::max (maxDiff, (double) std::abs (out[i] - ref[i]));
            maxRef = std::max (maxRef, (double) std::abs (ref[i]));
        }
        const double db = 20.0 * std::log10 (std::max (1.0e-12, maxDiff));
        std::printf ("golden: largest difference %.3g (%.1f dBFS) over %d samples\n", maxDiff, db, (int) out.size());
        check (db < -120.0, "output unchanged (within float rounding, -120 dBFS)");
        return failures == 0 ? 0 : 1;
    }

    void runCpuBenchmark()
    {
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

        {
            // CHARACTER in, two models blended (its most expensive state), 48 kHz
            const auto scene = makeScene (48000.0, 20.0, true, true, 42);
            EnhEngine::Parameters p;
            p.normalize = 0.7f; p.adaptSpeed = 0.5f; p.sub = 0.6f; p.subBoost = true; p.footstep = true;
            p.seraph.mode = enh::dsp::Seraph::heaven; p.lumen.active = true; p.limiter.active = true; p.tide.active = true;
            p.character.active = true; p.character.modelA = enh::dsp::Character::vintage; p.character.modelB = enh::dsp::Character::tape15;
            p.character.blend = 0.5f;
            const auto r = run (scene, 48000.0, 256, p);
            std::printf ("   48000 Hz, CHARACTER in (A + B blended): %.2f%% of one core\n", 100.0 * r.seconds / 20.0);
            check (r.finite && r.peak <= 1.0f, "stable with CHARACTER in");
            p.character.blend = 0.0f;
            const auto one = run (scene, 48000.0, 256, p);
            std::printf ("   48000 Hz, CHARACTER in (one model):        %.2f%% of one core\n", 100.0 * one.seconds / 20.0);
        }

        if (std::getenv ("CPU_BREAKDOWN") != nullptr)
        {
            // What each unit costs: the whole rack, then with one unit taken out at a time (48 kHz)
            const auto scene = makeScene (48000.0, 20.0, true, true, 42);
            auto full = EnhEngine::Parameters {};
            full.normalize = 0.7f; full.adaptSpeed = 0.5f; full.sub = 0.6f; full.subBoost = true; full.footstep = true;
            full.seraph.mode = enh::dsp::Seraph::heaven; full.lumen.active = true; full.limiter.active = true; full.tide.active = true;
            auto pct = [&] (const EnhEngine::Parameters& p)   // best of 3: this CPU's clock wanders
            {
                double best = 1.0e9;
                for (int k = 0; k < 3; ++k)
                    best = std::min (best, 100.0 * run (scene, 48000.0, 256, p).seconds / 20.0);
                return best;
            };
            const double all = pct (full);
            std::printf ("  whole rack %.2f %%\n", all);
            auto without = [&] (const char* what, auto&& change)
            {
                auto p = full;
                change (p);
                std::printf ("  %-28s saves %5.2f %%\n", what, all - pct (p));
            };
            {
                auto spectral = full;
                spectral.balancer.active = true;
                spectral.balancer.resolution = 1.0f;
                auto bands = full;
                bands.balancer.active = true;
                std::printf ("  MIX BALANCER spectral costs %5.2f %% over six bands\n", pct (spectral) - pct (bands));
            }
            without ("TONE & SPACE SPACE (tone only)", [] (auto& p) { p.seraph.mode = enh::dsp::Seraph::silkOnly; });
            without ("TONE & SPACE off", [] (auto& p) { p.seraph.mode = enh::dsp::Seraph::off; });
            without ("compressor", [] (auto& p) { p.tide.active = false; });
            without ("spectral limiter", [] (auto& p) { p.limiter.active = false; });
            without ("leveler", [] (auto& p) { p.lumen.active = false; });
            without ("sub", [] (auto& p) { p.sub = 0.0f; p.subBoost = false; });
            without ("footstep", [] (auto& p) { p.footstep = false; });
            without ("enhancer strength 0", [] (auto& p) { p.strength = 0.0f; p.normalize = 0.0f; p.boost = 0.0f; });
            without ("everything off", [] (auto& p) { p.seraph.mode = 0; p.tide.active = false; p.limiter.active = false; p.lumen.active = false;
                                                      p.sub = 0.0f; p.subBoost = false; p.footstep = false; p.strength = 0.0f; p.normalize = 0.0f; p.boost = 0.0f; });
        }
    }

}

#include "CharacterTests.h"
#include "MasteringTests.h"

int main (int argc, char** argv)
{
    const double sr = 48000.0;
    const int block = 128;

    if (argc > 1 && juce::String (argv[1]) == "--bass")
    {
        runLoudBassTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 3 && juce::String (argv[1]) == "--golden")
        return runGolden (argv[2], juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (argv[3])));

    if (argc > 1 && juce::String (argv[1]) == "--radar")
    {
        bool table = false, old = false;
        for (int k = 2; k < argc; ++k)
        {
            table = table || juce::String (argv[k]) == "table";
            old = old || juce::String (argv[k]) == "old";
        }
        radartests::runRadarTests (sr, table, old);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--units")
        return runNewUnitsMode (sr);

    if (argc > 1 && juce::String (argv[1]) == "--methods")
    {
        runMethodTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--methods-doc")
    {
        const auto file = methodsDocFile();
        if (file == juce::File() || ! file.replaceWithText (methodsDoc(), false, false, "\n"))
            return 2;
        std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--str0")
    {
        runStrengthZeroCheck (sr);
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--alias")
    {
        runAliasTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--zipper")
    {
        runZipperTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--fuzz")
    {
        runKnobFuzz (sr, argc > 2 ? juce::String (argv[2]).getDoubleValue() : 60.0, argc > 3 ? juce::String (argv[3]).getIntValue() : 1);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--mastering")
    {
        runMasteringTests (sr);
        runSafetyTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--character")
    {
        runCharacterTests (argc > 2 && juce::String (argv[2]) == "table");
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--cpu")
    {
        runCpuBenchmark();
        return failures == 0 ? 0 : 1;
    }

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
        std::vector<enh::dsp::FootstepRadar::Decision> log;
        log.reserve (200000);
        engine.setRadarLog (&log);
        size_t shown = 0;
        double nextReport = 5.0;
        std::printf ("time    event     P     base  walker attack  decay  over   tonal  level     far\n");

        for (int pos = 0; pos < len; pos += block)
        {
            const int n = std::min (block, len - pos);
            buf.setSize (2, n, false, false, true);
            buf.copyFrom (0, 0, file, 0, pos, n);
            buf.copyFrom (1, 0, file, file.getNumChannels() > 1 ? 1 : 0, pos, n);
            engine.process (buf, p);

            for (; shown < log.size(); ++shown)
            {
                const auto& d = log[shown];
                std::printf ("%7.3f %-8s  %.2f  %.2f  %.2f  %5.1f ms %5.1f dB %5.1f dB  %.2f %6.1f dBFS %.2f\n", d.time,
                             d.accepted ? "FOOTSTEP" : "rejected", d.probability, d.base, d.match, d.attackMs, d.decayDb, d.excessDb,
                             d.tonal, d.levelDb, d.distance);
            }

            const double now = (pos + n) / fileRate;
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

        engine.setRadarLog (nullptr);
        std::printf ("footsteps found %d, events %d\n", engine.getRadar().getStepsTotal(), engine.getRadar().getEventCount());
        return 0;
    }

    // Per-event decisions with ground truth: EnhDspTests --events quiet|crates|varied|reverb|game [seed]
    if (argc > 2 && juce::String (argv[1]) == "--events")
    {
        const juce::String which (argv[2]);
        const auto scene = which == "quiet" ? makeScene (sr, 12.0, false, false, 77)
                         : which == "crates" ? makeCrateScene (sr, 40.0, argc > 3 ? std::atoi (argv[3]) : 11)
                         : which == "varied" ? makeScene (sr, 30.0, true, true, argc > 3 ? std::atoi (argv[3]) : 7, true)
                         : which == "reverb" ? makeReverbScene (sr, 24.0, argc > 3 ? std::atoi (argv[3]) : 5, true)
                         : makeScene (sr, 24.0, true, true, argc > 3 ? std::atoi (argv[3]) : 1234);
        std::vector<enh::dsp::FootstepRadar::Decision> log;
        log.reserve (20000);
        enh::dsp::FootstepRadar::Settings s;
        radartests::runRadar (radartests::fromScene (scene, sr), s, nullptr, &log);
        auto label = [&] (double t)
        {
            for (auto& st : scene.steps) if (t >= st.time - 0.02 && t < st.time + 0.2) return juce::String ("STEP ") + juce::String ((t - st.time) * 1000.0, 0) + "ms";
            for (auto& c : scene.crates) if (t >= c.time - 0.01 && t < c.time + 1.2) return juce::String ("crate+") + juce::String ((t - c.time) * 1000.0, 0) + "ms";
            for (auto& sh : scene.shots) if (t >= sh.time - 0.01 && t < sh.time + 0.5) return juce::String ("shot");
            return juce::String ("-");
        };
        for (auto& d : log)
            std::printf ("%7.3f %-6s %-14s P %.2f base %.2f walker %.2f  attack %4.1f ms  decay %4.1f dB  over %4.1f dB  tonal %.2f  %6.1f dBFS  far %.2f\n",
                         d.time, d.accepted ? "ACCEPT" : "reject", label (d.time).toRawUTF8(), d.probability, d.base, d.match, d.attackMs,
                         d.decayDb, d.excessDb, d.tonal, d.levelDb, d.distance);
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--precision")
    {
        precisiontests::runPrecisionTests (sr);
        std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
        return failures == 0 ? 0 : 1;
    }

    if (argc > 1 && juce::String (argv[1]) == "--diagnose")
    {
        radartests::runRadarTests (sr, true, false);   // every scene, what it found (RADAR_DUMP=<scene> for its every decision)
        return 0;
    }

    //==========================================================================
    // Footsteps: the FOOTSTEP RADAR on every surface and distance, under game audio, against look-alikes,
    // and on the scenes the old detector was tuned on (the same ground truth)
    radartests::runRadarTests (sr, false, false);
    precisiontests::runPrecisionTests (sr);

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
            // TONE is oversampled, so the unit delays by a few samples: run that far past the end and give
            // the output back lined up with the input
            const size_t n = l.size(), lat = (size_t) unit.getLatencySamples();
            l.resize (n + lat, 0.0f);
            r.resize (n + lat, 0.0f);
            for (size_t pos = 0; pos < l.size(); pos += (size_t) block)
            {
                const int len = (int) std::min ((size_t) block, l.size() - pos);
                float* ch[2] { l.data() + pos, r.data() + pos };
                unit.process (ch, 2, len, st);
            }
            l.erase (l.begin(), l.begin() + (std::ptrdiff_t) lat);
            r.erase (r.begin(), r.begin() + (std::ptrdiff_t) lat);
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

        // BASS MONO on a 60 Hz note: in phase (left = right) and out of phase (right upside down). Out of
        // phase, the whole note is side: taken away it was gone (-16 dB on out-of-phase footsteps); folded
        // into the middle it stays, as mono.
        {
            Seraph::Settings st;
            st.mode = Seraph::heaven;
            st.silk = silkOnly (0.0f, 0.0f, 0.0f, 0.0f).silk;
            st.halo.width = 1.0f; st.halo.space = 0.0f; st.halo.shimmer = 0.0f; st.halo.bassMono = true; st.halo.strength = 1.0f;
            std::vector<float> l (n), r (n), rInv (n);
            for (size_t i = 0; i < n; ++i)
            {
                l[i] = r[i] = dbfs (-12.0f) * (float) std::sin (twoPi * 60.0 * (double) i / sr);
                rInv[i] = -l[i];
            }
            const auto inPhase = runSeraph (l, r, st), outOfPhase = runSeraph (l, rInv, st);
            const float same = 0.5f * (bandDb (inPhase.first, 60.0, 2.0) + bandDb (inPhase.second, 60.0, 2.0));
            const float anti = 0.5f * (bandDb (outOfPhase.first, 60.0, 2.0) + bandDb (outOfPhase.second, 60.0, 2.0));
            double lr = 0.0, ll = 0.0, rr = 0.0;
            for (size_t i = n / 2; i < n; ++i)
            {
                lr += (double) outOfPhase.first[i] * outOfPhase.second[i];
                ll += (double) outOfPhase.first[i] * outOfPhase.first[i];
                rr += (double) outOfPhase.second[i] * outOfPhase.second[i];
            }
            const double corr = lr / std::sqrt (ll * rr + 1e-30);
            std::printf ("  BASS MONO, 60 Hz: in phase %.1f dB, out of phase %.1f dB (left/right correlation after: %.2f)\n", same, anti, corr);
            check (std::abs (anti - same) < 1.5f && corr > 0.9,
                   "BASS MONO keeps out-of-phase bass (folded to mono, within 1.5 dB of the same note in phase)");
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
            // Zero latency: the first ~1 ms of an attack passes an existing dip before any detector can react,
            // and the dip then fades out over 1 ms (dropping it in one sample clicked): about 4 dB at the peak
            check (loss > -4.0f && loss > lossOff + 4.0f, "PROTECT keeps attack peaks under a dipped resonance (within 4 dB, and 4 dB better than without it)");
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
            // TONE is oversampled (linear phase): the output is the input delayed by its latency, and
            // untouched below 18 kHz - the oversampler's anti-alias filter works only above that
            const size_t lat0 = (size_t) u0.getLatencySamples();
            const auto lp18 = enh::dsp::SvfCoeffs::make (sr, 18000.0, 0.7071);
            enh::dsp::SvfState o1, o2, i1, i2;
            std::vector<float> inBand (n);
            for (size_t i = 0; i < n; ++i)
            {
                const float o = o2.process (lp18, o1.process (lp18, out0[i]).low).low;
                const float x = i2.process (lp18, i1.process (lp18, i >= lat0 ? in[i - lat0] : 0.0f).low).low;
                diff[i] = o - x;
                inBand[i] = x;
            }
            const float residual = rmsDb (diff, n / 2, n) - rmsDb (inBand, n / 2, n);
            runWith (1.0f, u1);
            runWith (3.0f, u3);
            const float space1 = u1.getActivityDb (Seraph::spaceAct, 0), space3 = u3.getActivityDb (Seraph::spaceAct, 0);
            const float warm1 = u1.getActivityDb (Seraph::warmthAct, 0), warm3 = u3.getActivityDb (Seraph::warmthAct, 0);
            std::printf ("  SERAPH STRENGTH 0: change %.1f dB below the input; SPACE %.1f -> %.1f dB, WARMTH %.1f -> %.1f dB (x1 -> x3)\n",
                         -residual, space1, space3, warm1, warm3);
            check (residual < -40.0f, "SERAPH STRENGTH 0 leaves the audio untouched below 18 kHz (< -40 dB change)");
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
    std::printf ("\n== ADAPTIVE COMPRESSOR: pumping (steady 2 kHz tone under a kick every 0.5 s), dual vs single release ==\n");
    {
        using enh::dsp::DynamicCompressor;
        auto pumping = [&] (bool dual)
        {
            DynamicCompressor comp;
            comp.prepare (sr, 2);
            comp.setDualRelease (dual);
            DynamicCompressor::Settings s;
            s.active = true; s.response = 0.5f; s.mix = 1.0f;
            const int n = (int) (8.0 * sr);
            std::vector<float> l ((size_t) n), r ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const double t = i / sr, kt = std::fmod (t, 0.5);
                const float tone = dbfs (-20.0f) * (float) std::sin (twoPi * 2000.0 * t);
                const float kick = dbfs (-6.0f) * (float) (std::sin (twoPi * (55.0 + 60.0 * std::exp (-kt / 0.02)) * kt) * std::exp (-kt / 0.09));
                l[(size_t) i] = r[(size_t) i] = tone + kick;
            }
            double grSum = 0.0;
            int grCount = 0;
            for (int pos = 0; pos < n; pos += block)
            {
                float* c[2] { l.data() + pos, r.data() + pos };
                comp.process (c, 2, std::min (block, n - pos), s);
                if (pos > 4.0 * sr) { grSum += comp.getReadout().gainReductionDb; ++grCount; }
            }
            // The tone's level in 10 ms windows over the last 4 s: how much it swings is the pumping
            const auto bp = BiquadCoeffs::bandPass (sr, 2000.0, 4.0);
            BiquadState b1, b2;
            std::vector<float> levels;
            const int win = (int) (0.010 * sr);
            double acc = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const float y = b1.process (bp, b2.process (bp, l[(size_t) i]));
                acc += (double) y * y;
                if ((i + 1) % win == 0)
                {
                    if (i > 4.0 * sr)
                    {
                        levels.push_back ((float) (10.0 * std::log10 (acc / win + 1e-15)));
                    }
                    acc = 0.0;
                }
            }
            // Pumping: how far the tone sits from its typical level, on average over time
            auto sorted = levels;
            std::nth_element (sorted.begin(), sorted.begin() + (long) sorted.size() / 2, sorted.end());
            const float median = sorted[sorted.size() / 2];
            double dev = 0.0;
            for (float v : levels) dev += std::abs (v - median);
            return std::make_pair ((float) (dev / std::max<size_t> (1, levels.size())), (float) (grSum / std::max (1, grCount)));
        };
        const auto single = pumping (false), dual = pumping (true);
        std::printf ("  single release: tone off its level by %.2f dB on average (mean GR %.1f dB)\n", single.first, single.second);
        std::printf ("  dual release  : tone off its level by %.2f dB on average (mean GR %.1f dB)\n", dual.first, dual.second);
        check (dual.first < single.first * 0.75f, "dual release pumps a steady tone at least 25% less");
    }

    std::printf ("\n== ADAPTIVE COMPRESSOR: a wide mix is compressed as the same mix in mono ==\n");
    {
        using enh::dsp::DynamicCompressor;
        // Pink noise at the same level in each channel: the same in both (mono), unrelated (wide), or
        // one the other upside down (out of phase). The mean gain reduction over the last 3 s.
        auto meanGr = [&] (int detector, int stereo)
        {
            DynamicCompressor comp;
            comp.prepare (sr, 2);
            DynamicCompressor::Settings s;
            s.active = true; s.response = 0.5f; s.mix = 1.0f; s.detector = detector;
            const int n = (int) (6.0 * sr);
            std::vector<float> l ((size_t) n), r ((size_t) n);
            juce::Random rnd (31);
            Pink pl, pr;
            for (int i = 0; i < n; ++i)
            {
                const float a = dbfs (-10.0f) * pl.next (rnd) * 4.0f, b = dbfs (-10.0f) * pr.next (rnd) * 4.0f;
                l[(size_t) i] = a;
                r[(size_t) i] = stereo == 0 ? a : stereo == 1 ? b : -a;
            }
            double sum = 0.0;
            int count = 0;
            for (int pos = 0; pos < n; pos += block)
            {
                float* c[2] { l.data() + pos, r.data() + pos };
                comp.process (c, 2, std::min (block, n - pos), s);
                if (pos > 3.0 * sr) { sum += comp.getReadout().gainReductionDb; ++count; }
            }
            return (float) (sum / std::max (1, count));
        };
        for (int det = 0; det < DynamicCompressor::numDetectors; ++det)
        {
            const float mono = meanGr (det, 0), wide = meanGr (det, 1), anti = meanGr (det, 2);
            std::printf ("  detector %d: gain reduction mono %.1f, wide %.1f, out of phase %.1f dB\n", det, mono, wide, anti);
            check (std::abs (wide - mono) < 1.0f && std::abs (anti - mono) < 1.0f,
                   "detector " + juce::String (det) + ": wide and out-of-phase mixes compressed as mono (within 1 dB)");
        }
    }

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

        // The same pink noise as mono, as two unrelated channels (a wide ambience), and out of phase:
        // the lift it settles on, and the most it lifted on the way there (the start of the sound)
        auto runWidth = [&] (int stereo, float levelDb, double seconds)
        {
            SpectralLeveler lev;
            lev.prepare (sr, 2);
            SpectralLeveler::Settings s;
            s.active = true;
            s.targetDb = -18.0f;
            s.response = 0.6f;

            const int n = (int) (seconds * sr);
            std::vector<float> l ((size_t) n), r ((size_t) n);
            juce::Random rnd (12);
            Pink pl, pr;
            for (int i = 0; i < n; ++i)
            {
                const float a = dbfs (levelDb) * pl.next (rnd) * 4.0f;
                const float b = dbfs (levelDb) * pr.next (rnd) * 4.0f;
                l[(size_t) i] = a;
                r[(size_t) i] = stereo == 0 ? a : stereo == 1 ? b : -a;
            }
            float most = 0.0f;
            for (int pos = 0; pos < n; pos += block)
            {
                float* c[2] { l.data() + pos, r.data() + pos };
                lev.process (c, 2, std::min (block, n - pos), s);
                most = std::max (most, lev.getReadout().totalGainDb);
            }
            return std::pair<float, float> { lev.getReadout().totalGainDb, most };
        };
        const auto mono = runWidth (0, -24.0f, 20.0), wide = runWidth (1, -24.0f, 20.0), anti = runWidth (2, -24.0f, 20.0);
        std::printf ("  pink at -24 dB, lift settled on (most on the way): mono %+.1f (%+.1f), wide %+.1f (%+.1f), out of phase %+.1f (%+.1f) dB\n",
                     mono.first, mono.second, wide.first, wide.second, anti.first, anti.second);
        check (std::abs (wide.first - mono.first) < 1.0f && std::abs (anti.first - mono.first) < 1.0f,
               "a wide or out-of-phase sound is lifted as the same sound in mono is (within 1 dB)");
        check (mono.second < mono.first + 1.0f && wide.second < wide.first + 1.0f,
               "no surge when a steady sound starts: the lift never goes more than 1 dB past where it settles");

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
            // A real LR4 3-way sums flat in level, not sample for sample: each split adds an allpass, so
            // the waveform is phase-shifted while every frequency comes through at its own level.
            auto rmsDb = [] (const std::vector<float>& v, size_t from) {
                double acc = 0.0;
                for (size_t i = from; i < v.size(); ++i) acc += (double) v[i] * v[i];
                return 10.0 * std::log10 (acc / (double) (v.size() - from) + 1e-20);
            };
            const double whole = rmsDb (a, 2048) - rmsDb (before, 2048);
            double worstBand = 0.0;
            for (double hz : { 60.0, 200.0, 800.0, 2200.0, 8000.0 })
            {
                BiquadState f1, f2, g1, g2;
                const auto bp = BiquadCoeffs::bandPass (sr, hz, 1.0);
                std::vector<float> fa (a.size()), fb (a.size());
                for (size_t i = 0; i < a.size(); ++i)
                {
                    fa[i] = f2.process (bp, f1.process (bp, a[i]));
                    fb[i] = g2.process (bp, g1.process (bp, before[i]));
                }
                worstBand = std::max (worstBand, std::abs (rmsDb (fa, 2048) - rmsDb (fb, 2048)));
            }
            std::printf ("  band sum: level error %+.3f dB overall, worst band %.3f dB\n", whole, worstBand);
            check (std::abs (whole) < 0.1 && worstBand < 0.5, "the three bands sum back to the input at every frequency (the LR4 split is flat)");
        }

        {
            // The split has to be a split: a bass note must not appear in the midrange band. It used to
            // (the low band was subtracted with the wrong phase), so every bass note read as a loud
            // midrange and the lift on detail was pulled down for seconds afterwards.
            SpectralLeveler lev;
            lev.prepare (sr, 2);
            SpectralLeveler::Settings s;
            s.active = true;
            s.targetDb = -60.0f;
            std::vector<float> a ((size_t) (sr * 2.0)), b (a.size());
            for (size_t i = 0; i < a.size(); ++i)
                a[i] = b[i] = dbfs (-10.0f) * (float) std::sin (twoPi * 50.0 * (double) i / sr);
            for (int pos = 0; pos < (int) a.size(); pos += block)
            {
                float* c[2] { a.data() + pos, b.data() + pos };
                lev.process (c, 2, std::min (block, (int) a.size() - pos), s);
            }
            const auto& r = lev.getReadout();
            std::printf ("  a 50 Hz note reads: low %.1f dB, mid %.1f dB, high %.1f dB\n", r.levelDb[0], r.levelDb[1], r.levelDb[2]);
            check (r.levelDb[1] < r.levelDb[0] - 25.0f && r.levelDb[2] < r.levelDb[0] - 25.0f,
                   "a bass note stays in the low band (25 dB or more under it in the other two)");
        }

        check (quiet.finite && loud.finite, "LUMEN stays finite");
    }

    runLimiterTests (sr);
    runPresetTests (sr);
    runLoudBassTests (sr);
    runNewUnitTests (sr);
    runDeepSubTests (sr);
    runMethodTests (sr);

    runCpuBenchmark();

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
