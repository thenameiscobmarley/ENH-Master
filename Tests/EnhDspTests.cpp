/*  Offline tests / benchmark for the ENH Master DSP engine.

    Synthetic scenes only (no game recordings are bundled), so treat the detection
    numbers as a sanity check of the detector's logic, not as real-game accuracy.
*/
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/EnhEngine.h"

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
        std::vector<Event> steps, shots;
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
        float maxAbsBandGain = 0.0f;
        std::vector<float> outL, outR;
        double seconds = 0.0;
        bool finite = true;
        float peak = 0.0f;
    };

    RunResult run (const Scene& scene, double sr, int blockSize, const EnhEngine::Parameters& p)
    {
        EnhEngine engine;
        engine.prepare (sr, blockSize, 2);

        RunResult res;
        const int total = (int) scene.left.size();
        res.outL.resize ((size_t) total);
        res.outR.resize ((size_t) total);
        juce::AudioBuffer<float> buf (2, blockSize);

        const auto t0 = juce::Time::getHighResolutionTicks();

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
        }

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
}

int main (int argc, char** argv)
{
    const double sr = 48000.0;
    const int block = 128;

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
                        std::printf ("     +%2.0fms onset %.2f bal %.2f share %.2f hot %.2f bb %.2f mid %.2f sus %.2f raw %.2f conf %.2f rhy %.2f\n",
                                     (b * block / sr - st.time) * 1000.0, t.onset, t.balance, t.share, t.hot, t.broadband, t.midDominance, t.sustain, t.raw, t.confirmed, t.rhythm);
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
        p.clarity = 0.6f; p.adaptSpeed = 0.5f; p.sub = 0.3f; p.footstep = true;
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
        check (hitRate >= 0.85f, "detects >= 85% of footsteps");
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
        p.clarity = 0.6f; p.footstep = true;
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
        check (hitRate >= 0.80f, "detects >= 80% of varied-surface footsteps");
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
        p.clarity = 0.0f; p.sub = 0.0f; p.footstep = false;
        const auto r = run (s, sr, block, p);
        const float inDb = rmsDb (s.left, n / 2, n), outDb = rmsDb (r.outL, n / 2, n);
        std::printf ("  level change     : %+.2f dB\n", outDb - inDb);
        check (std::abs (outDb - inDb) < 1.5f, "near unity at neutral settings");
    }

    //==========================================================================
    std::printf ("\n== Loudness match at full CLARITY (pink noise + tones) ==\n");
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

        EnhEngine::Parameters p;
        p.clarity = 1.0f; p.adaptSpeed = 0.6f;
        const auto r = run (s, sr, block, p);
        const float inDb = rmsDb (s.left, n / 2, n), outDb = rmsDb (r.outL, n / 2, n);
        std::printf ("  RMS change       : %+.2f dB\n", outDb - inDb);
        check (std::abs (outDb - inDb) < 3.0f, "auto gain keeps loudness within 3 dB");
        check (r.finite && r.peak <= 1.0f, "output finite and below full scale");
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
            p.clarity = 0.0f; p.sub = mode == 0 ? 0.0f : 1.0f; p.subBoost = mode == 2;
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
        base.clarity = 0.0f; base.adaptSpeed = 0.5f;
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
            p.clarity = c;
            const auto r = run (music, sr, block, p);
            std::printf ("  CLARITY %3.0f%%: difference %+.1f dB rel.  mean |band gain| %.1f dB  max %.1f dB  loudness %+.1f dB\n",
                         c * 100.0f, diffDb (r), r.meanAbsBandGain, r.maxAbsBandGain, rmsDb (r.outL, from, to) - rmsDb (music.left, from, to));
            if (c > 0.9f)
            {
                check (diffDb (r) > -12.0f, "full CLARITY changes the sound clearly (difference > -12 dB)");
                check (r.meanAbsBandGain > 2.0, "adaptive EQ actively moving (mean |gain| > 2 dB)");
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

    std::printf ("\n== CPU (this machine) ==\n");
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        const auto scene = makeScene (rate, 20.0, true, true, 42);
        EnhEngine::Parameters p;
        p.clarity = 0.7f; p.adaptSpeed = 0.5f; p.sub = 0.6f; p.subBoost = true; p.footstep = true;
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
        p.clarity = 1.0f; p.sub = 1.0f; p.subBoost = true; p.footstep = true;
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
