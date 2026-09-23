/*  EnhAudioLab - listen to ENH Master with your eyes.

    Runs audio (built-in test scenes or any WAV file) through the real engine with any preset, knob or
    processing method, and writes what it did as audio, a text report and pictures:

      in.wav / out.wav    the input and the rack's output (latency removed), to listen to
      report.txt          levels, loudness (BS.1770), true peak, dynamics, stereo, third-octave bands in
                          and out, the low end in detail, pumping per band, new clicks, THD / spurious
                          tones on the tone scenes, latency
      spectrogram.png     input, output and the difference (out minus in, red = added, blue = taken away)
      spectrum.png        the long-term spectrum in and out, and the change
      waveform.png        the waveforms in and out, momentary loudness, and the gain the rack applied over time
      lowend.png          10 - 250 Hz at high resolution, in and out (the sub region: DEEP SUB, SUB, limiters)
      contrib.png         (contrib) what each unit does to each band: the rack with that one unit out, compared
      ducks.png           (ducks) who ducks what, when: each unit's effect on low / mid / high over time
      trace.png / .txt    (trace) every unit's own gain read from the engine as it runs (10 ms): who moves,
                          how far, and when - the decisions rather than the result

    Commands:
      EnhAudioLab scenes
      EnhAudioLab render  (--scene NAME | --in FILE.wav) [--preset NAME] [--set param=value]... [--seconds S] --out DIR
      EnhAudioLab contrib (--scene NAME | --in FILE.wav) [--preset NAME] [--set param=value]... --out DIR
      EnhAudioLab ducks   (--scene NAME | --in FILE.wav) [--preset NAME] [--set param=value]... --out DIR
      EnhAudioLab trace   (--scene NAME | --in FILE.wav) [--preset NAME] [--set param=value]... --out DIR
      EnhAudioLab compare A.wav B.wav --out DIR
      EnhAudioLab suite   --out DIR         every scene through a set of presets, with a summary table
      EnhAudioLab check   [--update] [--only TEXT] [--threads N] [--baseline FILE] [--out DIR]
                          the self-test: every factory preset through the scenes, held to hard rules and compared with
                          Tests/lab-baseline.json; exit code 0 only when everything holds (--update: accept as the new baseline)

    --set takes any parameter ID with a value in its own units (knobs), 0 / 1 (switches) or the method's index
    (a processing method, e.g. --set tideDetector=1). Everything runs at 48 kHz stereo, offline.
*/

#include <atomic>
#include <thread>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/EnhEngine.h"
#include "DSP/ParameterMapping.h"
#include "DSP/LoudnessMeter.h"
#include "DSP/MethodRegistry.h"
#include "Parameters/ParameterSpecs.h"
#include "Parameters/PresetLibrary.h"

namespace lab
{
    using Buffer = juce::AudioBuffer<float>;
    constexpr double sr = 48000.0;
    constexpr double twoPi = 6.283185307179586;

    //==============================================================================
    // Test scenes
    struct Rng
    {
        juce::uint32 s = 1;
        float next() noexcept { s = s * 1664525u + 1013904223u; return (float) (s >> 8) / 8388608.0f - 1.0f; }
    };

    struct Pink   // Paul Kellet's refined filter
    {
        float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
        float next (Rng& r) noexcept
        {
            const float w = r.next();
            b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f;
            b2 = 0.96900f * b2 + w * 0.1538520f; b3 = 0.86650f * b3 + w * 0.3104856f;
            b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
            const float out = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
            b6 = w * 0.115926f;
            return out * 0.11f;
        }
    };

    struct Biquad
    {
        enh::dsp::BiquadCoeffs c;
        enh::dsp::BiquadState s;
        float operator() (float x) noexcept { return s.process (c, x); }
    };

    inline float db (float d) { return std::pow (10.0f, d / 20.0f); }

    Buffer makeBuffer (double seconds) { Buffer b (2, (int) (seconds * sr)); b.clear(); return b; }

    /** Footsteps: filtered noise bursts (1 - 4 kHz body, a heel thump), alternating left / right. */
    void addFootsteps (Buffer& b, double from, double to, double every, float levelDb, Rng& rng)
    {
        Biquad bp { enh::dsp::BiquadCoeffs::bandPass (sr, 2200.0, 0.9) }, thump { enh::dsp::BiquadCoeffs::lowPass (sr, 180.0, 0.7) };
        int k = 0;
        for (double t0 = from; t0 < to; t0 += every, ++k)
        {
            const float pan = (k % 2 == 0) ? 0.35f : 0.65f;
            const int start = (int) (t0 * sr), len = (int) (0.12 * sr);
            for (int i = 0; i < len && start + i < b.getNumSamples(); ++i)
            {
                const float t = (float) i / (float) sr;
                const float env = std::exp (-t / 0.035f) * std::min (1.0f, t / 0.0015f);
                const float x = db (levelDb) * env * (2.5f * bp (rng.next()) + 0.8f * thump (rng.next()) * std::exp (-t / 0.02f));
                b.addSample (0, start + i, x * (1.0f - pan) * 1.4f);
                b.addSample (1, start + i, x * pan * 1.4f);
            }
        }
    }

    void addNoiseBed (Buffer& b, float levelDb, Rng& rng)
    {
        Pink l, r;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            b.addSample (0, i, db (levelDb) * l.next (rng));
            b.addSample (1, i, db (levelDb) * r.next (rng));
        }
    }

    /** A gunshot: a broadband crack and a low thump, decaying in ~150 ms. */
    void addShot (Buffer& b, double at, float levelDb, Rng& rng)
    {
        Biquad lp { enh::dsp::BiquadCoeffs::lowPass (sr, 120.0, 0.8) };
        const int start = (int) (at * sr);
        for (int i = 0; i < (int) (0.4 * sr) && start + i < b.getNumSamples(); ++i)
        {
            const float t = (float) i / (float) sr;
            const float crack = rng.next() * std::exp (-t / 0.025f);
            const float boom = lp (rng.next()) * 6.0f * std::exp (-t / 0.15f) + 0.6f * (float) std::sin (twoPi * 55.0 * t) * std::exp (-t / 0.12f);
            const float x = db (levelDb) * (crack + boom);
            b.addSample (0, start + i, x);
            b.addSample (1, start + i, x * 0.9f);
        }
    }

    /** An explosion: a huge low rumble with a long decay and a broadband crack. */
    void addExplosion (Buffer& b, double at, float levelDb, Rng& rng)
    {
        Biquad lp1 { enh::dsp::BiquadCoeffs::lowPass (sr, 90.0, 0.7) }, lp2 { enh::dsp::BiquadCoeffs::lowPass (sr, 90.0, 0.7) };
        const int start = (int) (at * sr);
        for (int i = 0; i < (int) (2.5 * sr) && start + i < b.getNumSamples(); ++i)
        {
            const float t = (float) i / (float) sr;
            const float rumble = lp2 (lp1 (rng.next())) * 14.0f * std::exp (-t / 0.9f) * std::min (1.0f, t / 0.01f);
            const float crack = rng.next() * std::exp (-t / 0.06f) * 0.8f;
            const float x = db (levelDb) * (rumble + crack);
            b.addSample (0, start + i, x);
            b.addSample (1, start + i, x);
        }
    }

    /** Speech stand-in: noise through three formants, syllables at ~4 Hz. */
    void addVoice (Buffer& b, double from, double to, float levelDb, Rng& rng)
    {
        Biquad f1 { enh::dsp::BiquadCoeffs::bandPass (sr, 550.0, 5.0) }, f2 { enh::dsp::BiquadCoeffs::bandPass (sr, 1600.0, 6.0) },
               f3 { enh::dsp::BiquadCoeffs::bandPass (sr, 2600.0, 7.0) };
        for (int i = (int) (from * sr); i < (int) (to * sr) && i < b.getNumSamples(); ++i)
        {
            const double t = i / sr;
            const float buzz = (float) (std::fmod (t * 120.0, 1.0) - 0.5) + 0.3f * rng.next();
            const float syll = (float) std::pow (std::max (0.0, std::sin (twoPi * 3.8 * t)), 1.5);
            const float x = db (levelDb) * syll * (3.0f * f1 (buzz) + 2.2f * f2 (buzz) + 1.5f * f3 (buzz));
            b.addSample (0, i, x);
            b.addSample (1, i, x);
        }
    }

    /** Drums, bass, a pad and a lead at 120 bpm. */
    void addMusic (Buffer& b, Rng& rng, bool withPad)
    {
        const double beat = 0.5;
        const std::array<double, 8> bassNotes { 41.2, 41.2, 49.0, 55.0, 41.2, 61.7, 55.0, 73.4 };
        Biquad bassLp { enh::dsp::BiquadCoeffs::lowPass (sr, 400.0, 0.9) }, hatHp { enh::dsp::BiquadCoeffs::highPass (sr, 7000.0, 0.7) },
               snareBp { enh::dsp::BiquadCoeffs::bandPass (sr, 1800.0, 0.8) }, padLp { enh::dsp::BiquadCoeffs::lowPass (sr, 2200.0, 0.7) },
               padLpR { enh::dsp::BiquadCoeffs::lowPass (sr, 2200.0, 0.7) };
        double bassPhase = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const double t = i / sr;
            const double inBeat = std::fmod (t, beat);
            const int beatIndex = (int) (t / beat);
            float l = 0.0f, r = 0.0f;
            // Kick on every beat
            const float kick = 0.55f * (float) (std::sin (twoPi * (50.0 + 90.0 * std::exp (-inBeat / 0.03)) * inBeat) * std::exp (-inBeat / 0.18));
            // Snare on 2 and 4
            float snare = 0.0f;
            if (beatIndex % 2 == 1)
                snare = 0.35f * std::exp (-(float) inBeat / 0.09f) * (snareBp (rng.next()) * 2.5f + 0.4f * (float) std::sin (twoPi * 190.0 * inBeat));
            // Hats on eighths
            const double inEighth = std::fmod (t, beat * 0.5);
            const float hat = 0.12f * std::exp (-(float) inEighth / 0.025f) * hatHp (rng.next());
            // Bass: a saw, low-passed, one note per beat
            const double f = bassNotes[(size_t) (beatIndex % 8)];
            bassPhase = std::fmod (bassPhase + f / sr, 1.0);
            const float bass = 0.32f * bassLp ((float) (2.0 * bassPhase - 1.0)) * (float) std::min (1.0, inBeat / 0.005) * (float) std::exp (-inBeat / 0.9);
            l += kick + snare + bass + hat * 0.7f;
            r += kick + snare + bass + hat * 1.3f;
            if (withPad)
            {
                // Pad: an A minor chord of slightly detuned saws, and a sine lead
                float pad = 0.0f;
                for (double pf : { 220.0, 261.6, 329.6 })
                    pad += (float) (2.0 * std::fmod (t * pf * 1.003, 1.0) - 1.0) + (float) (2.0 * std::fmod (t * pf * 0.997, 1.0) - 1.0);
                const float lead = 0.10f * (float) std::sin (twoPi * (std::fmod (t, 4.0) < 2.0 ? 659.3 : 587.3) * t);
                l += 0.035f * padLp (pad) + lead;
                r += 0.035f * padLpR (pad) + lead * 0.8f;
            }
            b.addSample (0, i, l);
            b.addSample (1, i, r);
        }
    }

    struct SceneInfo { const char* name; const char* what; };
    const std::array<SceneInfo, 18> sceneList {{
        { "gaps",      "music for 3 s, near silence for 3 s, music again: what the auto gains do in the gaps" },
        { "bassduck",  "unchanging quiet detail and footsteps throughout, loud bass only from 3 - 5 s: what the bass does to the rest" },
        { "steps",     "ambience and footsteps only, left/right, every 0.45 s: what the rack does to footsteps over time" },
        { "game",      "ambience, footsteps left/right, gunshots, an explosion, a voice" },
        { "music",     "drums, bass line, pad chord and lead at 120 bpm" },
        { "drumsbass", "drums and a low bass line only (41 - 73 Hz)" },
        { "bassline",  "a sine bass line 41 - 110 Hz under a kick: the sub region" },
        { "explosion", "steady 2 kHz detail and quiet steps, then a huge low explosion: ducking and pumping" },
        { "quiet",     "a -60 dBFS noise floor with quiet footsteps: upward levelling and the gate" },
        { "sweep",     "a -12 dBFS log sine sweep, 20 Hz - 20 kHz: response, harmonics, aliasing" },
        { "tones",     "1 kHz, 100 Hz and 40 Hz at -6 dBFS, then 60 Hz + 7 kHz (4:1): THD and IMD" },
        { "impulses",  "short 2 kHz bursts every 0.5 s over silence: transients, ringing, pre-echo" },
        { "pink",      "-18 dBFS pink noise: the rack's static tone" },
        { "voice",     "a voice over a low bed: intelligibility, the leveler" },
        { "start",     "1.5 s of digital silence, then steady wide pink noise at -20 dBFS: does anything surge when sound starts" },
        { "wide",      "a wide ambience (unrelated left and right) with quiet footsteps: is wide material treated as mono would be" },
        { "antiphase", "footsteps and ambience with the right channel upside down: out-of-phase material" },
        { "silence",   "music for 2 s, then digital silence: does the output fall silent (no hiss, no endless tails, no drift)" },
    }};

    Buffer makeScene (const juce::String& name, double seconds)
    {
        Rng rng;
        auto b = makeBuffer (seconds);
        if (name == "game")
        {
            addNoiseBed (b, -40.0f, rng);
            addFootsteps (b, 0.3, seconds, 0.45, -24.0f, rng);
            addShot (b, 2.1, -6.0f, rng);
            addShot (b, 5.3, -8.0f, rng);
            addExplosion (b, 3.6, -4.0f, rng);
            addVoice (b, 6.0, seconds, -20.0f, rng);
        }
        else if (name == "steps")
        {
            addNoiseBed (b, -40.0f, rng);
            addFootsteps (b, 0.3, seconds, 0.45, -24.0f, rng);
        }
        else if (name == "music")      addMusic (b, rng, true);
        else if (name == "drumsbass")  addMusic (b, rng, false);
        else if (name == "bassline")
        {
            const std::array<double, 8> notes { 41.2, 55.0, 49.0, 73.4, 82.4, 61.7, 110.0, 55.0 };
            double phase = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / sr, inNote = std::fmod (t, 0.75);
                const double f = notes[(size_t) ((int) (t / 0.75) % 8)];
                phase = std::fmod (phase + f / sr, 1.0);
                const float env = (float) (std::min (1.0, inNote / 0.01) * std::exp (-inNote / 1.2));
                const float bass = 0.35f * env * (float) (std::sin (twoPi * phase) + 0.25 * std::sin (2.0 * twoPi * phase));
                const double kt = std::fmod (t, 1.5);
                const float kick = 0.4f * (float) (std::sin (twoPi * (48.0 + 80.0 * std::exp (-kt / 0.03)) * kt) * std::exp (-kt / 0.15));
                b.addSample (0, i, bass + kick);
                b.addSample (1, i, bass + kick);
            }
        }
        else if (name == "gaps")
        {
            auto music = makeBuffer (seconds);
            addMusic (music, rng, true);
            addNoiseBed (b, -54.0f, rng);   // a room tone that never stops, so nothing is ever digital silence
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / sr;
                const double cycle = std::fmod (t, 6.0);
                const float on = (float) (cycle < 3.0 ? std::min (1.0, cycle / 0.05) * std::min (1.0, (3.0 - cycle) / 0.05) : 0.0);
                for (int c = 0; c < 2; ++c)
                    b.addSample (c, i, on * music.getSample (c, i));
            }
        }
        else if (name == "bassduck")
        {
            // The whole point: everything except the bass is the same from start to finish, so any change in
            // the detail's output level is the rack reacting to the bass and nothing else.
            addNoiseBed (b, -40.0f, rng);
            addFootsteps (b, 0.3, seconds, 0.5, -24.0f, rng);
            Biquad detailL { enh::dsp::BiquadCoeffs::bandPass (sr, 2500.0, 1.2) }, detailR { enh::dsp::BiquadCoeffs::bandPass (sr, 2500.0, 1.2) };
            const double bassFrom = 3.0, bassTo = 5.0;
            double phase = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / sr;
                b.addSample (0, i, db (-34.0f) * 2.0f * detailL (rng.next()));
                b.addSample (1, i, db (-34.0f) * 2.0f * detailR (rng.next()));
                if (t >= bassFrom && t < bassTo)
                {
                    const double inNote = std::fmod (t - bassFrom, 0.5);
                    phase = std::fmod (phase + 50.0 / sr, 1.0);
                    const float env = (float) (std::min (1.0, inNote / 0.008) * std::exp (-inNote / 0.25));
                    const float bass = db (-5.0f) * env * (float) std::sin (twoPi * phase);
                    b.addSample (0, i, bass);
                    b.addSample (1, i, bass);
                }
            }
        }
        else if (name == "explosion")
        {
            Biquad bpL { enh::dsp::BiquadCoeffs::bandPass (sr, 2000.0, 2.0) }, bpR { enh::dsp::BiquadCoeffs::bandPass (sr, 2000.0, 2.0) };
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                b.addSample (0, i, db (-20.0f) * 2.0f * bpL (rng.next()));
                b.addSample (1, i, db (-20.0f) * 2.0f * bpR (rng.next()));
            }
            addFootsteps (b, 0.5, seconds, 0.5, -26.0f, rng);
            addExplosion (b, seconds * 0.45, -1.0f, rng);
        }
        else if (name == "quiet")
        {
            addNoiseBed (b, -60.0f, rng);
            addFootsteps (b, 0.4, seconds, 0.55, -46.0f, rng);
        }
        else if (name == "sweep")
        {
            const double f0 = 20.0, f1 = 20000.0, len = seconds - 0.5;
            const double k = std::log (f1 / f0);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / sr;
                if (t > len) break;
                const float x = db (-12.0f) * (float) std::sin (twoPi * f0 * len / k * (std::exp (t / len * k) - 1.0));
                b.setSample (0, i, x);
                b.setSample (1, i, x);
            }
        }
        else if (name == "tones")
        {
            const double seg = seconds / 4.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = i / sr;
                const int part = std::min (3, (int) (t / seg));
                const double fade = std::min (1.0, std::min (std::fmod (t, seg), seg - std::fmod (t, seg)) / 0.02);
                float x = 0.0f;
                if (part == 0) x = db (-6.0f) * (float) std::sin (twoPi * 1000.0 * t);
                if (part == 1) x = db (-6.0f) * (float) std::sin (twoPi * 100.0 * t);
                if (part == 2) x = db (-6.0f) * (float) std::sin (twoPi * 40.0 * t);
                if (part == 3) x = db (-6.0f) * (0.8f * (float) std::sin (twoPi * 60.0 * t) + 0.2f * (float) std::sin (twoPi * 7000.0 * t));
                x *= (float) fade;
                b.setSample (0, i, x);
                b.setSample (1, i, x);
            }
        }
        else if (name == "impulses")
        {
            for (double t0 = 0.25; t0 < seconds; t0 += 0.5)
                for (int i = 0; i < (int) (0.005 * sr); ++i)
                {
                    const double t = i / sr;
                    const float x = db (-6.0f) * (float) (std::sin (twoPi * 2000.0 * t) * 0.5 * (1.0 - std::cos (twoPi * t / 0.005)));
                    b.setSample (0, (int) (t0 * sr) + i, x);
                    b.setSample (1, (int) (t0 * sr) + i, x);
                }
        }
        else if (name == "pink")  addNoiseBed (b, -18.0f + 12.0f, rng);   // the pink generator sits ~12 dB low
        else if (name == "start")
        {
            addNoiseBed (b, -20.0f + 12.0f, rng);
            const int quiet = (int) (1.5 * sr);
            for (int c = 0; c < 2; ++c)
            {
                b.clear (c, 0, std::min (quiet, b.getNumSamples()));
                for (int i = 0; i < (int) (0.02 * sr) && quiet + i < b.getNumSamples(); ++i)   // a 20 ms fade in
                    b.setSample (c, quiet + i, b.getSample (c, quiet + i) * (float) i / (float) (0.02 * sr));
            }
        }
        else if (name == "wide")
        {
            addNoiseBed (b, -34.0f + 12.0f, rng);   // unrelated left and right
            addFootsteps (b, 0.3, seconds, 0.5, -30.0f, rng);
        }
        else if (name == "antiphase")
        {
            addNoiseBed (b, -40.0f, rng);
            addFootsteps (b, 0.3, seconds, 0.45, -24.0f, rng);
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (1, i, -b.getSample (0, i));
        }
        else if (name == "silence")
        {
            addMusic (b, rng, true);
            const int from = (int) (2.0 * sr), fade = (int) (0.05 * sr);
            for (int c = 0; c < 2; ++c)
                for (int i = from; i < b.getNumSamples(); ++i)
                    b.setSample (c, i, i < from + fade ? b.getSample (c, i) * (float) (from + fade - i) / (float) fade : 0.0f);
        }
        else if (name == "voice")
        {
            addNoiseBed (b, -48.0f, rng);
            addVoice (b, 0.2, seconds, -22.0f, rng);
        }
        else
            return {};
        return b;
    }

    //==============================================================================
    // The engine, from a preset plus overrides
    struct Setup
    {
        juce::String preset = "DEFAULT";
        std::vector<std::pair<juce::String, float>> sets;
    };

    bool factoryPresetsOnly = false;   // check: the presets as shipped, not the local presets file

    const pad::presets::Preset* findPreset (const juce::String& name)
    {
        if (factoryPresetsOnly)
        {
            for (auto& p : pad::presets::factory())
                if (p.name.equalsIgnoreCase (name))
                    return &p;
            return nullptr;
        }
        static const auto list = pad::presets::library (false);
        for (auto& p : *list)
            if (p.name.equalsIgnoreCase (name))
                return &p;
        for (auto& p : pad::presets::factory())
            if (p.name.equalsIgnoreCase (name))
                return &p;
        return nullptr;
    }

    enh::dsp::EnhEngine::Parameters parametersFor (const Setup& setup)
    {
        const auto* preset = findPreset (setup.preset);
        enh::dsp::KnobValues k;
        namespace id = pad::params::id;
        for (auto& spec : pad::params::allSpecs())
        {
            float v = preset != nullptr ? pad::presets::valueFor (*preset, spec) : spec.defaultValue;
            for (auto& s : setup.sets)
                if (s.first == spec.id) v = s.second;
            const bool on = v > 0.5f;
            // Continuous knobs
            bool done = false;
            for (auto& f : enh::dsp::knobFields)
                if (spec.id == f.param) { k.*(f.field) = v; done = true; }
            if (done) continue;
            // Processing methods
            for (int unit : enh::dsp::methods::unitsInRackOrder)
                for (int i = 0; i < enh::dsp::methods::stagesForUnit (unit).count; ++i)
                {
                    const auto& st = enh::dsp::methods::stagesForUnit (unit).stages[i];
                    if (st.id >= 0 && spec.id == juce::String (st.param.data(), st.param.size()))
                    { k.methods[(size_t) st.id] = juce::roundToInt (v); done = true; }
                }
            if (done) continue;
            // Switches and choices
            const auto& i = spec.id;
            if (i == id::clarityMode) k.clarityAddMode = on;   else if (i == id::subBoost) k.subBoost = on;
            else if (i == id::footstep) k.footstep = on;       else if (i == id::tideActive) k.tideActive = on;
            else if (i == id::lumenActive) k.lumenActive = on; else if (i == id::spectralActive) k.spectralActive = on;
            else if (i == id::balActive) k.balActive = on;     else if (i == id::deepActive) k.deepActive = on;
            else if (i == id::charActive) k.charActive = on;   else if (i == id::abCompare) k.compare = on;
            else if (i == id::charGrit) k.charGrit = on;
            else if (i == id::charModelA) k.charModelA = v;    else if (i == id::charModelB) k.charModelB = v;
            else if (i == id::seraphMode) k.seraphMode = juce::roundToInt (v);
            else if (i == id::silkProtect) k.protect = on;     else if (i == id::silkTape) k.tape = on;
            else if (i == id::silkAuto) k.autoGain = on;       else if (i == id::heavenMode) k.heavenLiftMode = on;
            else if (i == id::heavenAuto) k.heavenAuto = on;   else if (i == id::haloDuck) k.duck = on;
            else if (i == id::haloBassMono) k.bassMono = on;   else if (i == id::haloMod) k.mod = on;
        }
        return enh::dsp::mapKnobs (k);
    }

    /** Through the engine, in blocks; the output shifted back by the reported latency. */
    Buffer run (const Buffer& input, const enh::dsp::EnhEngine::Parameters& p, int& latency, int block = 256)
    {
        enh::dsp::EnhEngine engine;
        engine.prepare (sr, block, 2);
        latency = engine.getLatencySamples();
        const int n = input.getNumSamples();
        Buffer out (2, n + latency);
        out.clear();
        Buffer chunk (2, block);
        for (int pos = 0; pos < n + latency; pos += block)
        {
            const int len = std::min (block, n + latency - pos);
            chunk.setSize (2, len, false, false, true);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < len; ++i)
                    chunk.setSample (c, i, pos + i < n ? input.getSample (c, pos + i) : 0.0f);
            engine.process (chunk, p);
            for (int c = 0; c < 2; ++c)
                out.copyFrom (c, pos, chunk, c, 0, len);
            if (std::getenv ("LAB_FOOTSTEP_TRACE") != nullptr && (pos / block) % 8 == 0)
                std::printf ("   t %.2f  footstep confidence %.2f\n", pos / sr, engine.getMeters().footstepConfidence.load());
        }
        Buffer aligned (2, n);
        for (int c = 0; c < 2; ++c)
            aligned.copyFrom (c, 0, out, c, latency, n);
        return aligned;
    }

    //==============================================================================
    // Analysis
    struct Spectrum { std::vector<float> power; int fftOrder = 13; float binHz = 0.0f; };

    /** Long-term average power spectrum of the mid signal (Welch, Hann, 50 % overlap). */
    Spectrum averageSpectrum (const Buffer& b, int order = 13, int from = 0, int to = -1)
    {
        const int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> window ((size_t) size), work ((size_t) size * 2);
        for (int i = 0; i < size; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos ((float) twoPi * (float) i / (float) size);
        Spectrum s;
        s.fftOrder = order;
        s.binHz = (float) sr / (float) size;
        s.power.assign ((size_t) size / 2, 0.0f);
        if (to < 0) to = b.getNumSamples();
        int frames = 0;
        for (int pos = from; pos + size <= to; pos += size / 2, ++frames)
        {
            std::fill (work.begin(), work.end(), 0.0f);
            for (int i = 0; i < size; ++i)
                work[(size_t) i] = 0.5f * (b.getSample (0, pos + i) + b.getSample (1, pos + i)) * window[(size_t) i];
            fft.performFrequencyOnlyForwardTransform (work.data());
            for (int k = 0; k < size / 2; ++k)
                s.power[(size_t) k] += work[(size_t) k] * work[(size_t) k];
        }
        // Scaled so a full-scale sine reads ~0 dB at its bin (Hann: coherent gain 0.5)
        const float norm = frames > 0 ? 1.0f / ((float) frames * (0.25f * (float) size * (float) size)) : 0.0f;
        for (auto& p : s.power) p *= norm * 4.0f;
        return s;
    }

    float bandDb (const Spectrum& s, float lo, float hi)
    {
        double e = 0.0;
        for (size_t k = 1; k < s.power.size(); ++k)
        {
            const float f = (float) k * s.binHz;
            if (f >= lo && f < hi) e += s.power[k];
        }
        return (float) (10.0 * std::log10 (e + 1.0e-20));
    }

    float dbOf (double power) { return (float) (10.0 * std::log10 (power + 1.0e-20)); }

    struct Stats
    {
        float peakDb = -200, truePeakDb = -200, rmsDb = -200, lufsI = -70, maxMomentary = -120, maxShortTerm = -120, lra = 0;
        float crestDb = 0, dc = 0, correlation = 1, sideToMidDb = -100;
        int clipped = 0;
        std::vector<float> momentary, shortTerm;   // every 100 ms
    };

    Stats stats (const Buffer& b)
    {
        Stats st;
        const int n = b.getNumSamples();
        double sum = 0, sumL = 0, sumR = 0, lr = 0, ll = 0, rr = 0, mid = 0, side = 0;
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float l = b.getSample (0, i), r = b.getSample (1, i);
            peak = std::max ({ peak, std::abs (l), std::abs (r) });
            st.clipped += (std::abs (l) > 1.0f) + (std::abs (r) > 1.0f);
            sum += 0.5 * ((double) l * l + (double) r * r);
            sumL += l; sumR += r;
            lr += (double) l * r; ll += (double) l * l; rr += (double) r * r;
            mid += 0.25 * (double) (l + r) * (l + r); side += 0.25 * (double) (l - r) * (l - r);
        }
        st.peakDb = 20.0f * std::log10 (std::max (1.0e-10f, peak));
        st.rmsDb = dbOf (sum / std::max (1, n));
        st.crestDb = st.peakDb - st.rmsDb;
        st.dc = (float) (0.5 * (sumL + sumR) / std::max (1, n));
        st.correlation = (float) (lr / std::sqrt (ll * rr + 1.0e-20));
        st.sideToMidDb = dbOf (side) - dbOf (mid);

        enh::dsp::LoudnessMeter meter;
        meter.prepare (sr);
        const int hop = (int) (0.1 * sr);
        for (int pos = 0; pos < n; pos += hop)
        {
            const int len = std::min (hop, n - pos);
            const float* ch[2] { b.getReadPointer (0) + pos, b.getReadPointer (1) + pos };
            meter.process (ch, 2, len);
            st.momentary.push_back (meter.getMomentaryLufs());
            st.shortTerm.push_back (meter.getShortTermLufs());
        }
        st.lufsI = meter.getIntegratedLufs();
        st.truePeakDb = meter.getTruePeakDb();
        for (float m : st.momentary) st.maxMomentary = std::max (st.maxMomentary, m);
        std::vector<float> sts;
        for (size_t i = 30; i < st.shortTerm.size(); ++i)   // once the 3 s window is full
        {
            st.maxShortTerm = std::max (st.maxShortTerm, st.shortTerm[i]);
            if (st.shortTerm[i] > -70.0f) sts.push_back (st.shortTerm[i]);
        }
        if (sts.size() > 4)
        {
            std::sort (sts.begin(), sts.end());
            st.lra = sts[(size_t) (0.95 * (sts.size() - 1))] - sts[(size_t) (0.10 * (sts.size() - 1))];
        }
        return st;
    }

    /** Three bands of the mid signal, RMS every 50 ms (dB): low < 150 Hz, mid 150 Hz - 2 kHz, high > 2 kHz. */
    std::array<std::vector<float>, 3> bandLevels (const Buffer& b)
    {
        Biquad lo1 { enh::dsp::BiquadCoeffs::lowPass (sr, 150.0, 0.7071) }, lo2 { enh::dsp::BiquadCoeffs::lowPass (sr, 150.0, 0.7071) },
               hi1 { enh::dsp::BiquadCoeffs::highPass (sr, 2000.0, 0.7071) }, hi2 { enh::dsp::BiquadCoeffs::highPass (sr, 2000.0, 0.7071) };
        std::array<std::vector<float>, 3> out;
        const int hop = (int) (0.05 * sr);
        std::array<double, 3> acc {};
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float m = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
            const float low = lo2 (lo1 (m)), high = hi2 (hi1 (m)), midBand = m - low - high;
            acc[0] += (double) low * low; acc[1] += (double) midBand * midBand; acc[2] += (double) high * high;
            if ((i + 1) % hop == 0)
            {
                for (size_t k = 0; k < 3; ++k) { out[k].push_back (dbOf (acc[k] / hop)); acc[k] = 0.0; }
            }
        }
        return out;
    }

    /** Clicks: sample-to-sample curvature far above what the signal itself has. Returns the times (s) of
        the output's clicks that have no counterpart in the input within 3 ms. */
    std::vector<double> newClicks (const Buffer& in, const Buffer& out)
    {
        auto curvature = [] (const Buffer& b)
        {
            std::vector<float> d ((size_t) b.getNumSamples(), 0.0f);
            for (int i = 2; i < b.getNumSamples(); ++i)
                for (int c = 0; c < 2; ++c)
                    d[(size_t) i] = std::max (d[(size_t) i], std::abs (b.getSample (c, i) - 2.0f * b.getSample (c, i - 1) + b.getSample (c, i - 2)));
            return d;
        };
        const auto di = curvature (in), dout = curvature (out);
        // Local reference: the 99th percentile of the output's curvature over the surrounding 50 ms
        const int win = (int) (0.05 * sr);
        std::vector<double> clicks;
        std::vector<float> scratch;
        for (int i = win; i + win < (int) dout.size(); i += win / 4)
        {
            scratch.assign (dout.begin() + i - win, dout.begin() + i + win);
            std::nth_element (scratch.begin(), scratch.begin() + (long) (scratch.size() * 0.99), scratch.end());
            const float ref = scratch[(size_t) (scratch.size() * 0.99)];
            for (int j = i; j < i + win / 4; ++j)
            {
                if (dout[(size_t) j] < std::max (0.02f, 4.0f * ref))
                    continue;
                // The input's own discontinuity, scaled by the gain the rack applies here (a step the rack
                // only made bigger is not a new click)
                double eIn = 0.0, eOut = 0.0;
                for (int k = std::max (2, j - (int) (0.02 * sr)); k < std::min (in.getNumSamples(), j + (int) (0.02 * sr)); ++k)
                {
                    eIn += (double) in.getSample (0, k) * in.getSample (0, k);
                    eOut += (double) out.getSample (0, k) * out.getSample (0, k);
                }
                const float gain = (float) std::min (10.0, std::sqrt ((eOut + 1e-12) / (eIn + 1e-12)));
                bool inInput = false;
                for (int k = std::max (0, j - (int) (0.003 * sr)); k < std::min ((int) di.size(), j + (int) (0.003 * sr)); ++k)
                    inInput = inInput || di[(size_t) k] * std::max (1.0f, gain) * 5.0f > dout[(size_t) j];   // the input's own, made louder
                if (! inInput && (clicks.empty() || j / sr - clicks.back() > 0.01))
                    clicks.push_back (j / sr);
            }
        }
        return clicks;
    }

    /** Harmonic distortion of a steady tone between t0 and t1: THD (%), and the strongest non-harmonic
        component (dB below the tone) - aliasing or intermodulation shows up there. */
    struct Thd { float fundamentalHz = 0, thdPercent = 0, spuriousDb = -200, spuriousHz = 0; };
    Thd thd (const Buffer& b, double t0, double t1, float expectHz)
    {
        const auto s = averageSpectrum (b, 15, (int) (t0 * sr), (int) (t1 * sr));
        auto power = [&] (float hz, float halfWidth)
        {
            double e = 0.0;
            for (size_t k = 1; k < s.power.size(); ++k)
                if (std::abs ((float) k * s.binHz - hz) <= halfWidth) e += s.power[k];
            return e;
        };
        Thd r;
        r.fundamentalHz = expectHz;
        const double f = power (expectHz, 4.0f * s.binHz);
        double h = 0.0;
        for (int n = 2; n <= 10; ++n)
            if (expectHz * n < 20000.0f) h += power (expectHz * n, 4.0f * s.binHz);
        r.thdPercent = (float) (100.0 * std::sqrt (h / (f + 1.0e-30)));
        for (size_t k = 4; k < s.power.size(); ++k)
        {
            const float hz = (float) k * s.binHz;
            if (hz < 20.0f || hz > 20000.0f) continue;
            const float nearest = std::round (hz / expectHz) * expectHz;
            if (std::abs (hz - nearest) < 6.0f * s.binHz) continue;   // a harmonic (or the tone)
            const float d = dbOf (s.power[k]) - dbOf (f);
            if (d > r.spuriousDb) { r.spuriousDb = d; r.spuriousHz = hz; }
        }
        return r;
    }

    //==============================================================================
    // Pictures
    juce::Colour heat (float t)   // 0..1, black - purple - orange - yellow - white (inferno-like)
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        const std::array<juce::Colour, 6> stops { juce::Colour (0xff000004), juce::Colour (0xff320a5e), juce::Colour (0xff781c6d),
                                                   juce::Colour (0xffbb3754), juce::Colour (0xfff98c0a), juce::Colour (0xfffcffa4) };
        const float x = t * (float) (stops.size() - 1);
        const int i = std::min ((int) x, (int) stops.size() - 2);
        return stops[(size_t) i].interpolatedWith (stops[(size_t) i + 1], x - (float) i);
    }

    juce::Colour diverging (float d, float range)   // blue (taken away) - dark - red (added)
    {
        const float t = juce::jlimit (-1.0f, 1.0f, d / range);
        return t >= 0.0f ? juce::Colour (0xff101014).interpolatedWith (juce::Colour (0xffff4a3a), t)
                         : juce::Colour (0xff101014).interpolatedWith (juce::Colour (0xff3a9bff), -t);
    }

    juce::Font font (float h, bool bold = false)
    {
        auto o = juce::FontOptions().withHeight (h);
        return juce::Font (bold ? o.withStyle ("Bold") : o);
    }

    void save (const juce::Image& img, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }

    /** STFT magnitudes (dB) of the mid signal on a log-frequency grid: [column][row]. */
    std::vector<std::vector<float>> stft (const Buffer& b, int order, int columns, int rows, float loHz, float hiHz)
    {
        const int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> window ((size_t) size), work ((size_t) size * 2);
        for (int i = 0; i < size; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos ((float) twoPi * (float) i / (float) size);
        const int n = b.getNumSamples();
        std::vector<std::vector<float>> out ((size_t) columns, std::vector<float> ((size_t) rows, -200.0f));
        const float binHz = (float) sr / (float) size;
        for (int c = 0; c < columns; ++c)
        {
            const int centre = (int) ((double) c / (columns - 1) * (n - 1));
            std::fill (work.begin(), work.end(), 0.0f);
            for (int i = 0; i < size; ++i)
            {
                const int at = centre - size / 2 + i;
                if (at >= 0 && at < n)
                    work[(size_t) i] = 0.5f * (b.getSample (0, at) + b.getSample (1, at)) * window[(size_t) i];
            }
            fft.performFrequencyOnlyForwardTransform (work.data());
            for (int r = 0; r < rows; ++r)
            {
                const float f0 = loHz * std::pow (hiHz / loHz, (float) r / (float) rows);
                const float f1 = loHz * std::pow (hiHz / loHz, (float) (r + 1) / (float) rows);
                const int k0 = std::max (1, (int) std::floor (f0 / binHz)), k1 = std::max (k0, (int) std::ceil (f1 / binHz));
                float m = 0.0f;
                for (int k = k0; k <= k1 && k < size / 2; ++k) m = std::max (m, work[(size_t) k]);
                out[(size_t) c][(size_t) r] = 20.0f * std::log10 (m / (0.25f * (float) size) + 1.0e-10f);
            }
        }
        return out;
    }

    void freqAxis (juce::Graphics& g, juce::Rectangle<int> area, float loHz, float hiHz)
    {
        g.setFont (font (11.0f));
        for (float hz : { 10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 80.f, 100.f, 150.f, 200.f, 300.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
        {
            if (hz < loHz || hz > hiHz) continue;
            const float y = (float) area.getBottom() - (float) area.getHeight() * std::log (hz / loHz) / std::log (hiHz / loHz);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawHorizontalLine ((int) y, (float) area.getX(), (float) area.getRight());
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText (hz >= 1000.0f ? juce::String (hz / 1000.0f, 0) + "k" : juce::String ((int) hz), area.getX() - 44, (int) y - 7, 40, 14,
                        juce::Justification::right);
        }
    }

    void timeAxis (juce::Graphics& g, juce::Rectangle<int> area, double seconds)
    {
        g.setFont (font (11.0f));
        const double step = seconds > 20 ? 5.0 : seconds > 8 ? 2.0 : 1.0;
        for (double t = 0; t <= seconds + 1e-6; t += step)
        {
            const float x = (float) area.getX() + (float) area.getWidth() * (float) (t / seconds);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.drawText (juce::String (t, 0) + " s", (int) x - 20, area.getBottom() + 2, 40, 14, juce::Justification::centred);
        }
    }

    /** Input, output and the difference, log frequency. */
    void spectrogramImage (const Buffer& in, const Buffer& out, const juce::File& file, const juce::String& title,
                           float loHz = 20.0f, float hiHz = 20000.0f, int order = 12, float floorDb = -110.0f)
    {
        const int cols = 1100, rows = 240, left = 60, top = 34, gap = 30;
        juce::Image img (juce::Image::RGB, left + cols + 20, top + 3 * (rows + gap) + 10, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff0b0b0e));
        g.setColour (juce::Colours::white);
        g.setFont (font (15.0f, true));
        g.drawText (title, left, 6, cols, 20, juce::Justification::left);
        const auto a = stft (in, order, cols, rows, loHz, hiHz), b = stft (out, order, cols, rows, loHz, hiHz);
        const double seconds = in.getNumSamples() / sr;
        for (int panel = 0; panel < 3; ++panel)
        {
            const juce::Rectangle<int> area (left, top + panel * (rows + gap), cols, rows);
            for (int c = 0; c < cols; ++c)
                for (int r = 0; r < rows; ++r)
                {
                    const float va = a[(size_t) c][(size_t) r], vb = b[(size_t) c][(size_t) r];
                    const juce::Colour col = panel == 0 ? heat ((va - floorDb) / -floorDb)
                                           : panel == 1 ? heat ((vb - floorDb) / -floorDb)
                                                        : (std::max (va, vb) < floorDb + 20.0f ? juce::Colour (0xff101014) : diverging (vb - va, 12.0f));
                    img.setPixelAt (area.getX() + c, area.getBottom() - 1 - r, col);
                }
            freqAxis (g, area, loHz, hiHz);
            g.setColour (juce::Colours::white);
            g.setFont (font (12.0f, true));
            g.drawText (panel == 0 ? "INPUT" : panel == 1 ? "OUTPUT" : "CHANGE  (red: added, blue: taken away, +-12 dB)",
                        area.getX() + 6, area.getY() + 4, 500, 14, juce::Justification::left);
            timeAxis (g, area, seconds);
        }
        save (img, file);
    }

    /** Long-term spectrum, in and out, and the change. */
    void spectrumImage (const Spectrum& si, const Spectrum& so, const juce::File& file, const juce::String& title)
    {
        const int w = 1100, h = 560, left = 60, top = 34, plotH = 330, diffH = 150;
        juce::Image img (juce::Image::RGB, w + left + 20, h, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff0b0b0e));
        g.setColour (juce::Colours::white);
        g.setFont (font (15.0f, true));
        g.drawText (title, left, 6, w, 20, juce::Justification::left);
        auto xOf = [&] (float hz) { return (float) left + (float) w * std::log (hz / 20.0f) / std::log (1000.0f); };
        // Smoothed to 1/12 octave
        auto smooth = [&] (const Spectrum& s, float hz)
        {
            const float lo = hz * std::pow (2.0f, -1.0f / 24.0f), hi = hz * std::pow (2.0f, 1.0f / 24.0f);
            double e = 0; int n = 0;
            for (size_t k = 1; k < s.power.size(); ++k)
                if ((float) k * s.binHz >= lo && (float) k * s.binHz < hi) { e += s.power[k]; ++n; }
            if (n == 0) { const size_t k = std::min (s.power.size() - 1, (size_t) std::round (hz / s.binHz)); e = s.power[k]; n = 1; }
            return dbOf (e / n);
        };
        juce::Path pin, pout, pdiff;
        const juce::Rectangle<int> plot (left, top, w, plotH), diff (left, top + plotH + 30, w, diffH);
        for (float db = 0; db >= -120; db -= 20)
        {
            const float y = (float) plot.getY() + (float) plot.getHeight() * (-db / 120.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawHorizontalLine ((int) y, (float) left, (float) (left + w));
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (font (11.0f));
            g.drawText (juce::String ((int) db), 4, (int) y - 7, 50, 14, juce::Justification::right);
        }
        for (float db : { -12.f, -6.f, 0.f, 6.f, 12.f })
        {
            const float y = (float) diff.getCentreY() - (float) diff.getHeight() * 0.5f * db / 12.0f;
            g.setColour (juce::Colours::white.withAlpha (db == 0.0f ? 0.45f : 0.15f));
            g.drawHorizontalLine ((int) y, (float) left, (float) (left + w));
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), 4, (int) y - 7, 50, 14, juce::Justification::right);
        }
        for (float hz : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
        {
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawVerticalLine ((int) xOf (hz), (float) top, (float) diff.getBottom());
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawText (hz >= 1000 ? juce::String (hz / 1000, 0) + "k" : juce::String ((int) hz), (int) xOf (hz) - 20, diff.getBottom() + 4, 40, 14,
                        juce::Justification::centred);
        }
        bool first = true;
        for (int i = 0; i <= 400; ++i)
        {
            const float hz = 20.0f * std::pow (1000.0f, (float) i / 400.0f);
            const float a = smooth (si, hz), b = smooth (so, hz);
            const float x = xOf (hz);
            const float ya = (float) plot.getY() + (float) plot.getHeight() * juce::jlimit (0.0f, 1.0f, -a / 120.0f);
            const float yb = (float) plot.getY() + (float) plot.getHeight() * juce::jlimit (0.0f, 1.0f, -b / 120.0f);
            const float yd = (float) diff.getCentreY() - (float) diff.getHeight() * 0.5f * juce::jlimit (-1.0f, 1.0f, (b - a) / 12.0f);
            if (first) { pin.startNewSubPath (x, ya); pout.startNewSubPath (x, yb); pdiff.startNewSubPath (x, yd); first = false; }
            else { pin.lineTo (x, ya); pout.lineTo (x, yb); pdiff.lineTo (x, yd); }
        }
        g.setColour (juce::Colour (0xff8a8a96)); g.strokePath (pin, juce::PathStrokeType (1.6f));
        g.setColour (juce::Colour (0xfff2f2f2)); g.strokePath (pout, juce::PathStrokeType (1.6f));
        g.setColour (juce::Colour (0xffff5a4a)); g.strokePath (pdiff, juce::PathStrokeType (1.8f));
        g.setFont (font (12.0f, true));
        g.setColour (juce::Colour (0xff8a8a96)); g.drawText ("IN", left + 8, top + 4, 60, 14, juce::Justification::left);
        g.setColour (juce::Colour (0xfff2f2f2)); g.drawText ("OUT", left + 40, top + 4, 60, 14, juce::Justification::left);
        g.setColour (juce::Colour (0xffff5a4a)); g.drawText ("CHANGE (out - in, dB)", left + 8, diff.getY() + 4, 200, 14, juce::Justification::left);
        save (img, file);
    }

    /** Waveforms in and out, momentary loudness, and the gain per band over time. */
    void waveformImage (const Buffer& in, const Buffer& out, const Stats& si, const Stats& so, const juce::File& file, const juce::String& title)
    {
        const int w = 1100, left = 60, top = 34, waveH = 200, loudH = 150, gainH = 170;
        juce::Image img (juce::Image::RGB, w + left + 20, top + waveH + loudH + gainH + 110, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff0b0b0e));
        g.setColour (juce::Colours::white);
        g.setFont (font (15.0f, true));
        g.drawText (title, left, 6, w, 20, juce::Justification::left);
        const int n = in.getNumSamples();
        const double seconds = n / sr;
        const juce::Rectangle<int> wave (left, top, w, waveH), loud (left, top + waveH + 30, w, loudH), gain (left, top + waveH + loudH + 60, w, gainH);

        auto drawWave = [&] (const Buffer& b, juce::Colour col)
        {
            for (int x = 0; x < w; ++x)
            {
                const int i0 = (int) ((double) x / w * n), i1 = std::max (i0 + 1, (int) ((double) (x + 1) / w * n));
                float lo = 0, hi = 0;
                for (int i = i0; i < i1 && i < n; ++i)
                {
                    const float m = 0.5f * (b.getSample (0, i) + b.getSample (1, i));
                    lo = std::min (lo, m); hi = std::max (hi, m);
                }
                g.setColour (col);
                g.drawVerticalLine (left + x, (float) wave.getCentreY() - hi * (float) waveH * 0.5f, (float) wave.getCentreY() - lo * (float) waveH * 0.5f + 1.0f);
            }
        };
        drawWave (in, juce::Colour (0xff5a5a66));
        drawWave (out, juce::Colour (0xe0f0f0f0));
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawRect (wave);
        g.setFont (font (12.0f, true));
        g.drawText ("WAVEFORM  (grey in, white out)", left + 6, top + 4, 400, 14, juce::Justification::left);

        // Momentary loudness, -60 .. 0 LUFS
        auto curve = [&] (const std::vector<float>& v, juce::Rectangle<int> area, float lo, float hi, juce::Colour col)
        {
            juce::Path p;
            for (size_t i = 0; i < v.size(); ++i)
            {
                const float x = (float) area.getX() + (float) area.getWidth() * (float) i / (float) std::max<size_t> (1, v.size() - 1);
                const float y = (float) area.getBottom() - (float) area.getHeight() * juce::jlimit (0.0f, 1.0f, (v[i] - lo) / (hi - lo));
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (1.5f));
        };
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawRect (loud);
        for (float l : { -50.f, -40.f, -30.f, -20.f, -10.f })
        {
            const float y = (float) loud.getBottom() - (float) loud.getHeight() * (l + 60.0f) / 60.0f;
            g.setColour (juce::Colours::white.withAlpha (0.15f)); g.drawHorizontalLine ((int) y, (float) left, (float) (left + w));
            g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (font (11.0f));
            g.drawText (juce::String ((int) l), 4, (int) y - 7, 50, 14, juce::Justification::right);
        }
        curve (si.momentary, loud, -60.0f, 0.0f, juce::Colour (0xff8a8a96));
        curve (so.momentary, loud, -60.0f, 0.0f, juce::Colour (0xfff2f2f2));
        g.setColour (juce::Colours::white); g.setFont (font (12.0f, true));
        g.drawText ("MOMENTARY LOUDNESS (LUFS)", left + 6, loud.getY() + 4, 400, 14, juce::Justification::left);

        // The gain the rack applied, per band: out minus in, every 50 ms (+-18 dB)
        const auto bi = bandLevels (in), bo = bandLevels (out);
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawRect (gain);
        for (float d : { -12.f, -6.f, 0.f, 6.f, 12.f })
        {
            const float y = (float) gain.getCentreY() - (float) gain.getHeight() * 0.5f * d / 18.0f;
            g.setColour (juce::Colours::white.withAlpha (d == 0 ? 0.4f : 0.15f)); g.drawHorizontalLine ((int) y, (float) left, (float) (left + w));
            g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (font (11.0f));
            g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), 4, (int) y - 7, 50, 14, juce::Justification::right);
        }
        const std::array<juce::Colour, 3> bandCol { juce::Colour (0xffff8c3a), juce::Colour (0xff5ad07a), juce::Colour (0xff4aa8ff) };
        for (size_t k = 0; k < 3; ++k)
        {
            std::vector<float> d;
            for (size_t i = 0; i < bi[k].size(); ++i)
                d.push_back (std::max (bi[k][i], bo[k][i]) < -70.0f ? 0.0f : bo[k][i] - bi[k][i]);
            curve (d, gain, -18.0f, 18.0f, bandCol[k]);
        }
        g.setFont (font (12.0f, true));
        g.setColour (bandCol[0]); g.drawText ("LOW <150", left + 6, gain.getY() + 4, 90, 14, juce::Justification::left);
        g.setColour (bandCol[1]); g.drawText ("MID", left + 96, gain.getY() + 4, 60, 14, juce::Justification::left);
        g.setColour (bandCol[2]); g.drawText ("HIGH >2k", left + 146, gain.getY() + 4, 90, 14, juce::Justification::left);
        g.setColour (juce::Colours::white); g.drawText ("GAIN PER BAND (out - in, dB)", left + 250, gain.getY() + 4, 300, 14, juce::Justification::left);
        timeAxis (g, gain, seconds);
        save (img, file);
    }

    //==============================================================================
    // Report
    const std::array<float, 31> thirdOctaves = []
    {
        std::array<float, 31> f {};
        for (size_t k = 0; k < f.size(); ++k) f[k] = 20.0f * std::pow (2.0f, (float) k / 3.0f);
        return f;
    }();

    juce::String fmt (float v, int d = 1) { return juce::String (v, d); }

    juce::String report (const juce::String& title, const Buffer& in, const Buffer& out, int latency, const Stats& si, const Stats& so,
                         const Spectrum& spi, const Spectrum& spo, const juce::String& scene)
    {
        juce::String r;
        r << title << "\n" << juce::String::repeatedString ("=", title.length()) << "\n\n";
        r << "Latency (reported, removed from out.wav): " << latency << " samples (" << fmt ((float) (latency / sr * 1000.0), 2) << " ms)\n\n";
        r << "                    IN          OUT       CHANGE\n";
        auto row = [&] (const char* name, float a, float b, const char* unit)
        {
            r << juce::String (name).paddedRight (' ', 18) << fmt (a).paddedLeft (' ', 8) << "  " << fmt (b).paddedLeft (' ', 8) << "  "
              << ((b - a >= 0 ? "+" : "") + fmt (b - a)).paddedLeft (' ', 8) << "  " << unit << "\n";
        };
        row ("sample peak", si.peakDb, so.peakDb, "dBFS");
        row ("true peak", si.truePeakDb, so.truePeakDb, "dBTP");
        row ("RMS", si.rmsDb, so.rmsDb, "dBFS");
        row ("integrated", si.lufsI, so.lufsI, "LUFS");
        row ("max momentary", si.maxMomentary, so.maxMomentary, "LUFS");
        row ("max short-term", si.maxShortTerm, so.maxShortTerm, "LUFS");
        row ("loudness range", si.lra, so.lra, "LU (10th - 95th pct of short-term)");
        row ("crest factor", si.crestDb, so.crestDb, "dB (peak / RMS)");
        row ("side / mid", si.sideToMidDb, so.sideToMidDb, "dB (stereo width)");
        r << "correlation L/R     " << fmt (si.correlation, 3).paddedLeft (' ', 8) << "  " << fmt (so.correlation, 3).paddedLeft (' ', 8) << "\n";
        r << "DC offset           " << juce::String (si.dc, 6).paddedLeft (' ', 8) << "  " << juce::String (so.dc, 6).paddedLeft (' ', 8) << "\n";
        r << "samples over 0 dBFS " << juce::String (si.clipped).paddedLeft (' ', 8) << "  " << juce::String (so.clipped).paddedLeft (' ', 8) << "\n\n";

        r << "Third-octave bands (mid signal, long-term average, dB):\n";
        r << "      Hz      IN     OUT   CHANGE\n";
        for (float f : thirdOctaves)
        {
            const float lo = f * std::pow (2.0f, -1.0f / 6.0f), hi = f * std::pow (2.0f, 1.0f / 6.0f);
            const float a = bandDb (spi, lo, hi), b = bandDb (spo, lo, hi);
            if (std::max (a, b) < -140.0f) continue;
            r << (f >= 1000 ? fmt (f / 1000.0f, 1) + "k" : juce::String ((int) std::round (f))).paddedLeft (' ', 8)
              << fmt (a).paddedLeft (' ', 8) << fmt (b).paddedLeft (' ', 8) << ((b - a >= 0 ? " +" : " ") + fmt (b - a)).paddedLeft (' ', 9)
              << "  " << juce::String::repeatedString (b - a >= 0 ? "+" : "-", juce::jlimit (0, 30, (int) std::round (std::abs (b - a) * 2.0f))) << "\n";
        }
        r << "\nThe low end in detail (dB):\n";
        for (auto band : { std::pair { 10.f, 20.f }, std::pair { 20.f, 30.f }, std::pair { 30.f, 45.f }, std::pair { 45.f, 60.f },
                           std::pair { 60.f, 90.f }, std::pair { 90.f, 130.f }, std::pair { 130.f, 200.f } })
        {
            const float a = bandDb (spi, band.first, band.second), b = bandDb (spo, band.first, band.second);
            r << ("  " + juce::String ((int) band.first) + "-" + juce::String ((int) band.second) + " Hz").paddedRight (' ', 14)
              << fmt (a).paddedLeft (' ', 8) << fmt (b).paddedLeft (' ', 8) << ((b - a >= 0 ? " +" : " ") + fmt (b - a)).paddedLeft (' ', 9) << "\n";
        }

        // Dynamics per band: how the gain the rack applied moves (pumping), over the loud part
        const auto bi = bandLevels (in), bo = bandLevels (out);
        r << "\nGain per band over time (out - in, 50 ms steps, where the input is above -60 dB):\n";
        const char* names[3] { "low < 150 Hz", "mid", "high > 2 kHz" };
        for (size_t k = 0; k < 3; ++k)
        {
            std::vector<float> d;
            for (size_t i = 0; i < bi[k].size(); ++i)
                if (bi[k][i] > -60.0f) d.push_back (bo[k][i] - bi[k][i]);
            if (d.size() < 4) { r << "  " << names[k] << ": (no content)\n"; continue; }
            float mean = 0; for (float x : d) mean += x; mean /= (float) d.size();
            float var = 0; for (float x : d) var += (x - mean) * (x - mean); var /= (float) d.size();
            auto sorted = d; std::sort (sorted.begin(), sorted.end());
            r << "  " << juce::String (names[k]).paddedRight (' ', 14) << " mean " << fmt (mean).paddedLeft (' ', 6) << " dB   spread (sd) "
              << fmt (std::sqrt (var)).paddedLeft (' ', 5) << " dB   5th..95th pct " << fmt (sorted[sorted.size() / 20]) << " .. "
              << fmt (sorted[sorted.size() * 19 / 20]) << " dB\n";
        }

        const auto clicks = newClicks (in, out);
        r << "\nNew clicks / discontinuities in the output (not in the input): " << (int) clicks.size();
        if (! clicks.empty())
        {
            r << "  at";
            for (size_t i = 0; i < std::min<size_t> (12, clicks.size()); ++i) r << " " << fmt ((float) clicks[i], 3) << "s";
        }
        r << "\n";

        if (scene == "tones")
        {
            const double seg = in.getNumSamples() / sr / 4.0;
            r << "\nDistortion on the tones (steady middle of each):\n";
            for (auto [part, hz] : { std::pair { 0, 1000.0f }, std::pair { 1, 100.0f }, std::pair { 2, 40.0f } })
            {
                const auto a = thd (in, part * seg + 0.3, (part + 1) * seg - 0.3, hz), b = thd (out, part * seg + 0.3, (part + 1) * seg - 0.3, hz);
                r << "  " << juce::String ((int) hz).paddedLeft (' ', 5) << " Hz: THD in " << fmt (a.thdPercent, 3) << " %, out " << fmt (b.thdPercent, 3)
                  << " %;  strongest non-harmonic out " << fmt (b.spuriousDb) << " dB at " << juce::String ((int) b.spuriousHz) << " Hz\n";
            }
            // IMD: 60 Hz + 7 kHz; sidebands at 7 kHz +- 60 / 120 Hz
            const auto s = averageSpectrum (out, 15, (int) ((3 * seg + 0.3) * sr), (int) ((4 * seg - 0.3) * sr));
            const float carrier = bandDb (s, 6990.0f, 7010.0f);
            const float side = std::max ({ bandDb (s, 6930.0f, 6950.0f), bandDb (s, 7050.0f, 7070.0f), bandDb (s, 6870.0f, 6890.0f), bandDb (s, 7110.0f, 7130.0f) });
            r << "  IMD (60 Hz + 7 kHz): strongest sideband " << fmt (side - carrier) << " dB below the 7 kHz tone\n";
        }
        return r;
    }

    void writeWav (const Buffer& b, const juce::File& file)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (file.createOutputStream());
        if (os == nullptr) return;
        auto opts = juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24);
        if (auto writer = wav.createWriterFor (os, opts))
            writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }

    Buffer readWav (const juce::File& file)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr) return {};
        Buffer raw ((int) std::max (2u, reader->numChannels), (int) reader->lengthInSamples);
        reader->read (&raw, 0, (int) reader->lengthInSamples, 0, true, true);
        Buffer b (2, raw.getNumSamples());
        b.copyFrom (0, 0, raw, 0, 0, raw.getNumSamples());
        b.copyFrom (1, 0, raw, reader->numChannels > 1 ? 1 : 0, 0, raw.getNumSamples());
        if (std::abs (reader->sampleRate - sr) > 1.0)
        {
            // Resample to 48 kHz (Lagrange), so every analysis speaks the same rate
            const double ratio = reader->sampleRate / sr;
            Buffer rs (2, (int) (b.getNumSamples() / ratio));
            for (int c = 0; c < 2; ++c)
            {
                juce::LagrangeInterpolator li;
                li.process (ratio, b.getReadPointer (c), rs.getWritePointer (c), rs.getNumSamples());
            }
            return rs;
        }
        return b;
    }

    //==============================================================================
    struct Job
    {
        juce::String scene, inFile, name;
        Setup setup;
        double seconds = 8.0;
        juce::File out;
    };

    struct Result
    {
        Stats in, out;
        int clicks = 0;
        float lowDelta = 0, highSpread = 0, lowSpread = 0;
        float thd1k = 0;
        bool finite = true;
        float surgeDb = 0;          // "start": how far the output rises past where it settles, more than the input does
        float tailDb = -200;        // "silence": the output's level over its last 2 s (the input is digital silence there)
        float balanceDb = 0;        // left minus right, out against in: does the rack lean to one side
    };

    Buffer inputFor (const Job& job)
    {
        return job.inFile.isNotEmpty() ? readWav (juce::File::getCurrentWorkingDirectory().getChildFile (job.inFile))
                                       : makeScene (job.scene, job.seconds);
    }

    /** Everything measured about one run (the output already aligned to the input). */
    Result analyse (const Buffer& input, const Buffer& output, const juce::String& scene)
    {
        Result res;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < output.getNumSamples(); ++i)
                res.finite = res.finite && std::isfinite (output.getSample (c, i));
        res.in = stats (input);
        res.out = stats (output);
        res.clicks = (int) newClicks (input, output).size();
        {
            // Each channel's own spectrum, averaged: the mid signal is empty for out-of-phase material
            auto channels = [] (const Buffer& b)
            {
                Buffer l (2, b.getNumSamples()), r (2, b.getNumSamples());
                for (int c = 0; c < 2; ++c) { l.copyFrom (c, 0, b, 0, 0, b.getNumSamples()); r.copyFrom (c, 0, b, 1, 0, b.getNumSamples()); }
                return 0.5f * (bandDb (averageSpectrum (l), 20.0f, 150.0f) + bandDb (averageSpectrum (r), 20.0f, 150.0f));
            };
            res.lowDelta = channels (output) - channels (input);
        }
        {
            const auto bi = bandLevels (input), bo = bandLevels (output);
            auto spread = [&] (size_t k)
            {
                std::vector<float> d;
                for (size_t i = 0; i < bi[k].size(); ++i) if (bi[k][i] > -60.0f) d.push_back (bo[k][i] - bi[k][i]);
                if (d.size() < 4) return 0.0f;
                float m = 0; for (float x : d) m += x; m /= (float) d.size();
                float v = 0; for (float x : d) v += (x - m) * (x - m);
                return std::sqrt (v / (float) d.size());
            };
            res.lowSpread = spread (0);
            res.highSpread = spread (2);
        }
        if (scene == "tones")
        {
            const double seg = input.getNumSamples() / sr / 4.0;
            res.thd1k = thd (output, 0.3, seg - 0.3, 1000.0f).thdPercent;
        }
        if (scene == "start")
        {
            // Momentary loudness every 100 ms: the most in the 4 s after the sound starts, against the
            // median of the last 3 s (where it has settled). The input's own figure is taken off.
            auto rise = [] (const std::vector<float>& m, size_t onset)
            {
                if (m.size() < onset + 40) return 0.0f;
                float most = -200.0f;
                for (size_t i = onset; i < onset + 40; ++i) most = std::max (most, m[i]);
                std::vector<float> last (m.end() - 30, m.end());
                std::nth_element (last.begin(), last.begin() + 15, last.end());
                return most - last[15];
            };
            size_t onset = 0;
            while (onset < res.in.momentary.size() && res.in.momentary[onset] < -60.0f) ++onset;
            onset += 4;   // the momentary window (400 ms) is full of sound from here
            res.surgeDb = rise (res.out.momentary, onset) - rise (res.in.momentary, onset);
        }
        if (scene == "silence")
        {
            const int from = std::max (0, output.getNumSamples() - (int) (2.0 * sr));
            double e = 0.0;
            for (int c = 0; c < 2; ++c)
                for (int i = from; i < output.getNumSamples(); ++i)
                    e += (double) output.getSample (c, i) * output.getSample (c, i);
            res.tailDb = dbOf (e / std::max (1, 2 * (output.getNumSamples() - from)));
        }
        {
            auto channelDb = [] (const Buffer& b, int c)
            {
                double e = 0.0;
                for (int i = 0; i < b.getNumSamples(); ++i) e += (double) b.getSample (c, i) * b.getSample (c, i);
                return dbOf (e / std::max (1, b.getNumSamples()));
            };
            res.balanceDb = (channelDb (output, 0) - channelDb (output, 1)) - (channelDb (input, 0) - channelDb (input, 1));
        }
        return res;
    }

    Result render (const Job& job, bool pictures = true)
    {
        const auto input = inputFor (job);
        if (input.getNumSamples() == 0) { std::printf ("no input (unknown scene '%s' or unreadable file)\n", job.scene.toRawUTF8()); return {}; }
        int latency = 0;
        const auto output = run (input, parametersFor (job.setup), latency);
        job.out.createDirectory();
        writeWav (input, job.out.getChildFile ("in.wav"));
        writeWav (output, job.out.getChildFile ("out.wav"));
        const auto res = analyse (input, output, job.scene);
        const auto spi = averageSpectrum (input), spo = averageSpectrum (output);
        juce::String title = (job.inFile.isNotEmpty() ? job.inFile : job.scene) + "  through  " + job.setup.preset;
        for (auto& s : job.setup.sets) title << "  " << s.first << "=" << juce::String (s.second);
        const auto text = report (title, input, output, latency, res.in, res.out, spi, spo, job.scene);
        job.out.getChildFile ("report.txt").replaceWithText (text);
        if (pictures)
        {
            spectrogramImage (input, output, job.out.getChildFile ("spectrogram.png"), title);
            spectrumImage (spi, spo, job.out.getChildFile ("spectrum.png"), title);
            waveformImage (input, output, res.in, res.out, job.out.getChildFile ("waveform.png"), title);
            spectrogramImage (input, output, job.out.getChildFile ("lowend.png"), title + "  -  LOW END 10-250 Hz", 10.0f, 250.0f, 15, -120.0f);
        }
        return res;
    }

    /** What each unit does: the rack as set, against the rack with just that one unit out. */
    void contrib (const Job& job)
    {
        const auto input = inputFor (job);
        if (input.getNumSamples() == 0) { std::printf ("no input\n"); return; }
        job.out.createDirectory();
        int latency = 0;
        const auto full = run (input, parametersFor (job.setup), latency);
        const auto spFull = averageSpectrum (full);
        struct Off { const char* unit; std::vector<std::pair<juce::String, float>> sets; };
        const std::vector<Off> units {
            { "LEVEL CONTROL", { { "levelGain", 0.0f } } },
            { "ADAPTIVE ENHANCER", { { "enhStrength", 0.0f }, { "sub", 0.0f }, { "footstep", 0.0f } } },
            { "UPWARD LEVELER", { { "lumenActive", 0.0f } } },
            { "DEEP SUB", { { "deepActive", 0.0f } } },
            { "SPECTRAL LIMITER", { { "spectralActive", 0.0f } } },
            { "MIX BALANCER", { { "balActive", 0.0f } } },
            { "ADAPTIVE COMPRESSOR", { { "tideActive", 0.0f } } },
            { "TONE & SPACE", { { "seraphMode", 0.0f } } },
        };
        juce::String text;
        text << "What each unit contributes: the rack as set minus the rack with that unit out (dB per band)\n"
             << (job.inFile.isNotEmpty() ? job.inFile : job.scene) << " through " << job.setup.preset << "\n\n";
        text << "unit                     LUFS    ";
        const std::array<std::pair<float, float>, 8> bands {{ { 20, 40 }, { 40, 80 }, { 80, 160 }, { 160, 320 }, { 320, 640 }, { 640, 1280 }, { 1280, 5000 }, { 5000, 20000 } }};
        for (auto b : bands) text << (juce::String ((int) b.first) + "-" + (b.second >= 1000 ? juce::String ((int) (b.second / 1000)) + "k" : juce::String ((int) b.second))).paddedLeft (' ', 10);
        text << "\n";
        const Stats fullStats = stats (full);
        std::vector<std::vector<float>> grid;
        for (auto& u : units)
        {
            auto setup = job.setup;
            for (auto& s : u.sets) setup.sets.push_back (s);
            int lat = 0;
            const auto without = run (input, parametersFor (setup), lat);
            const auto sp = averageSpectrum (without);
            std::vector<float> row;
            text << juce::String (u.unit).paddedRight (' ', 22) << ((fullStats.lufsI - stats (without).lufsI >= 0 ? " +" : " ")
                     + fmt (fullStats.lufsI - stats (without).lufsI)).paddedLeft (' ', 7) << "   ";
            for (auto b : bands)
            {
                const float d = bandDb (spFull, b.first, b.second) - bandDb (sp, b.first, b.second);
                row.push_back (d);
                text << ((d >= 0 ? "+" : "") + fmt (d)).paddedLeft (' ', 10);
            }
            text << "\n";
            grid.push_back (row);
        }
        job.out.getChildFile ("contrib.txt").replaceWithText (text);

        // Heat map
        const int cellW = 110, cellH = 40, left = 200, top = 60;
        juce::Image img (juce::Image::RGB, left + cellW * (int) bands.size() + 20, top + cellH * (int) units.size() + 30, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff0b0b0e));
        g.setColour (juce::Colours::white);
        g.setFont (font (15.0f, true));
        g.drawText ("WHAT EACH UNIT DOES  (dB per band, red: adds, blue: takes away)  -  " + (job.inFile.isNotEmpty() ? job.inFile : job.scene)
                    + " / " + job.setup.preset, 10, 8, img.getWidth() - 20, 20, juce::Justification::left);
        g.setFont (font (12.0f, true));
        for (size_t b = 0; b < bands.size(); ++b)
            g.drawText (juce::String ((int) bands[b].first) + "-" + (bands[b].second >= 1000 ? juce::String ((int) (bands[b].second / 1000)) + "k"
                                                                                               : juce::String ((int) bands[b].second)),
                        left + (int) b * cellW, top - 20, cellW, 16, juce::Justification::centred);
        for (size_t u = 0; u < units.size(); ++u)
        {
            g.setColour (juce::Colours::white);
            g.drawText (units[u].unit, 8, top + (int) u * cellH, left - 16, cellH, juce::Justification::centredRight);
            for (size_t b = 0; b < bands.size(); ++b)
            {
                const float d = grid[u][b];
                g.setColour (diverging (d, 6.0f));
                g.fillRect (left + (int) b * cellW + 1, top + (int) u * cellH + 1, cellW - 2, cellH - 2);
                g.setColour (juce::Colours::white);
                g.drawText (((d >= 0 ? "+" : "") + fmt (d)), left + (int) b * cellW, top + (int) u * cellH, cellW, cellH, juce::Justification::centred);
            }
        }
        save (img, job.out.getChildFile ("contrib.png"));
        std::printf ("%s", text.toRawUTF8());
    }

    /** Every unit's own gain, read from the engine while it runs (10 ms): not what the output did, but what
        each unit decided to do and when. This is how to see which unit moves first when something changes. */
    void trace (const Job& job)
    {
        const auto input = inputFor (job);
        if (input.getNumSamples() == 0) { std::printf ("no input\n"); return; }
        job.out.createDirectory();

        struct Row { float t; std::array<float, 28> v {}; };
        struct Col { const char* name; juce::Colour colour; bool inPicture; };
        const std::array<Col, 28> cols {{
            { "LEVELER low",      juce::Colour (0xffc8a060), true },
            { "LEVELER mid",      juce::Colour (0xffe0c080), true },
            { "LEVELER high",     juce::Colour (0xfff0e0b0), true },
            { "BALANCER 70",      juce::Colour (0xfff0e080), false },
            { "BALANCER 200",     juce::Colour (0xfff0e080), false },
            { "BALANCER 500",     juce::Colour (0xfff0e080), true },
            { "BALANCER 1.3k",    juce::Colour (0xffd8c860), true },
            { "BALANCER 3.5k",    juce::Colour (0xffc0b040), true },
            { "BALANCER 9k",      juce::Colour (0xfff0e080), false },
            { "COMPRESSOR GR",    juce::Colour (0xff4fd6d6), true },
            { "SPECTRAL deepest", juce::Colour (0xffff4f8b), true },
            { "SPECTRAL broad",   juce::Colour (0xffff90b8), true },
            { "SPECTRAL makeup",  juce::Colour (0xffff6f9b), false },
            { "TONE&SPACE MATCH", juce::Colour (0xffb388ff), true },
            { "TONE&SPACE dip",   juce::Colour (0xffd0b0ff), false },
            { "OUT LIMITER",      juce::Colour (0xffff5040), true },
            { "ENH auto gain",    juce::Colour (0xffe8a33c), true },
            { "LOUDNESS TARGET",  juce::Colour (0xff90f0ff), true },
            { "ENH sub lift",     juce::Colour (0xff5a7cff), false },
            { "footstep conf",    juce::Colour (0xff60ff90), false },
            { "DEEP SUB out",     juce::Colour (0xff5a7cff), false },
            { "LEVELER lvl low",  juce::Colour (0xff808080), false },   // what the leveler is reading, per band
            { "LEVELER lvl mid",  juce::Colour (0xff808080), false },
            { "LEVELER lvl high", juce::Colour (0xff808080), false },
            { "LEVELER loud mid", juce::Colour (0xff808080), false },   // its own view of the mid band
            { "LEVELER floor mid",juce::Colour (0xff808080), false },
            { "LEVELER gate mid", juce::Colour (0xff808080), false },
            { "LEVELER want mid", juce::Colour (0xff808080), false },
        }};

        enh::dsp::EnhEngine engine;
        const int block = 64;
        engine.prepare (sr, block, 2);
        const auto params = parametersFor (job.setup);
        const int n = input.getNumSamples(), hop = (int) (0.01 * sr);
        Buffer chunk (2, block);
        std::vector<Row> rows;
        for (int pos = 0; pos < n; pos += block)
        {
            const int len = std::min (block, n - pos);
            chunk.setSize (2, len, false, false, true);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < len; ++i)
                    chunk.setSample (c, i, input.getSample (c, pos + i));
            engine.process (chunk, params);

            if (pos / hop != (pos + block) / hop)
            {
                const auto& m = engine.getMeters();
                Row row { (float) (pos / sr) };
                for (int b = 0; b < 3; ++b) row.v[(size_t) b] = m.lumenGainDb[(size_t) b].load();
                for (int b = 0; b < 6; ++b) row.v[(size_t) (3 + b)] = m.balanceGainDb[(size_t) b].load();
                row.v[9] = -std::abs (m.tideGrDb.load());
                row.v[10] = -std::abs (m.limitDeepestDb.load());
                row.v[11] = -std::abs (m.limitBroadbandDb.load());
                row.v[12] = m.limitMakeupDb.load();
                row.v[13] = m.silkMatchDb.load();
                row.v[14] = -std::abs (m.silkSmoothingDb.load());
                row.v[15] = -std::abs (m.outputLimitDb.load());
                row.v[16] = m.autoGainDb.load();
                row.v[17] = m.targetGainDb.load();
                row.v[18] = m.subLiftDb.load();
                row.v[19] = m.footstepConfidence.load();
                row.v[20] = m.deepGeneratedDb.load();
                for (int b = 0; b < 3; ++b) row.v[(size_t) (21 + b)] = m.lumenLevelDb[(size_t) b].load();
                const auto& lum = engine.getLeveler().getReadout();
                row.v[24] = lum.loudDb[1]; row.v[25] = lum.floorDb[1]; row.v[26] = lum.gate[1]; row.v[27] = lum.wantedDb[1];
                rows.push_back (row);
            }
        }

        // What the rest of the material actually came out at, next to the decisions: the detail band
        // (2 kHz and up) of the output, so a duck can be matched to whoever caused it.
        int latency = 0;
        const auto output = run (input, params, latency);
        const auto outBands = bandLevels (output), inBands = bandLevels (input);

        juce::String text;
        text << "What each unit is doing, every 10 ms (dB; negative is taking away)\n"
             << (job.inFile.isNotEmpty() ? job.inFile : job.scene) << " through " << job.setup.preset << "\n\n"
             << "     t";
        for (auto& c : cols) text << juce::String (c.name).paddedLeft (' ', 18);
        text << "    low out   mid out  high out\n";
        for (size_t r = 0; r < rows.size(); ++r)
        {
            text << fmt (rows[r].t, 2).paddedLeft (' ', 6);
            for (size_t k = 0; k < cols.size(); ++k) text << fmt (rows[r].v[k], 2).paddedLeft (' ', 18);
            const size_t band = (size_t) (rows[r].t / 0.05f);
            for (size_t k = 0; k < 3; ++k)
                text << (band < outBands[k].size() ? fmt (outBands[k][band], 1) : juce::String ("-")).paddedLeft (' ', 10);
            text << "\n";
        }

        // The summary that answers "who did it": for every unit, how far it moved and when it moved most
        juce::String head;
        head << "unit                    from      to   biggest move   at\n";
        for (size_t k = 0; k < cols.size(); ++k)
        {
            float lo = 1.0e9f, hi = -1.0e9f, moveAt = 0.0f, prev = rows.empty() ? 0.0f : rows[0].v[k], worst = 0.0f;
            for (auto& r : rows)
            {
                lo = std::min (lo, r.v[k]); hi = std::max (hi, r.v[k]);
                const float d = r.v[k] - prev;
                if (std::abs (d) > std::abs (worst)) { worst = d; moveAt = r.t; }
                prev = r.v[k];
            }
            head << juce::String (cols[k].name).paddedRight (' ', 20) << fmt (lo, 2).paddedLeft (' ', 8) << fmt (hi, 2).paddedLeft (' ', 8)
                 << fmt (worst, 2).paddedLeft (' ', 12) << "   " << fmt (moveAt, 2) << " s\n";
        }
        std::printf ("%s", head.toRawUTF8());
        job.out.getChildFile ("trace.txt").replaceWithText (head + "\n" + text);

        // The picture: the input's detail band as a grey fill for orientation, each unit's gain as a line
        const int w = 1200, left = 70, top = 40, h = 520;
        juce::Image img (juce::Image::RGB, w + left + 210, top + h + 40, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff101014));
        g.setColour (juce::Colours::white);
        g.setFont (15.0f);
        g.drawText ("WHAT EACH UNIT IS DOING  -  " + (job.inFile.isNotEmpty() ? job.inFile : job.scene) + " / " + job.setup.preset,
                    left, 10, w, 20, juce::Justification::left);

        const float top_ = 12.0f, bottom = -18.0f;    // dB range of the picture
        const auto yFor = [&] (float db) { return (float) top + (float) h * (top_ - std::clamp (db, bottom, top_)) / (top_ - bottom); };
        const auto xFor = [&] (float t) { return (float) left + (float) w * t / std::max (0.01f, rows.empty() ? 1.0f : rows.back().t); };

        // The input's low band behind everything: where the bass is
        juce::Path bassFill;
        bassFill.startNewSubPath ((float) left, (float) (top + h));
        for (size_t i = 0; i < inBands[0].size(); ++i)
            bassFill.lineTo (xFor ((float) i * 0.05f), yFor (std::clamp ((inBands[0][i] + 60.0f) * 0.5f - 18.0f, bottom, top_)));
        bassFill.lineTo ((float) (left + w), (float) (top + h));
        bassFill.closeSubPath();
        g.setColour (juce::Colour (0x22ffffff));
        g.fillPath (bassFill);

        g.setColour (juce::Colour (0x40ffffff));
        for (float db = bottom; db <= top_; db += 3.0f)
        {
            g.drawHorizontalLine ((int) yFor (db), (float) left, (float) (left + w));
            g.setFont (11.0f);
            g.drawText (juce::String ((int) db), 4, (int) yFor (db) - 8, left - 10, 16, juce::Justification::right);
        }
        for (float t = 0.0f; t <= (rows.empty() ? 0.0f : rows.back().t); t += 1.0f)
        {
            g.drawVerticalLine ((int) xFor (t), (float) top, (float) (top + h));
            g.drawText (juce::String (t, 0) + " s", (int) xFor (t) - 20, top + h + 4, 40, 16, juce::Justification::centred);
        }

        int legend = top;
        for (size_t k = 0; k < cols.size(); ++k)
        {
            if (! cols[k].inPicture) continue;
            juce::Path p;
            for (size_t r = 0; r < rows.size(); ++r)
            {
                const float x = xFor (rows[r].t), y = yFor (rows[r].v[k]);
                if (r == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (cols[k].colour);
            g.strokePath (p, juce::PathStrokeType (1.6f));
            g.setFont (12.0f);
            g.drawText (cols[k].name, left + w + 12, legend, 190, 16, juce::Justification::left);
            legend += 18;
        }
        save (img, job.out.getChildFile ("trace.png"));
        std::printf ("wrote %s\n", job.out.getChildFile ("trace.png").getFullPathName().toRawUTF8());
    }

    /** Who ducks what, when: for each unit, the rack as set against the rack with that unit out, band by band
        over time (50 ms). A unit's curve below zero is it taking that band down at that moment. */
    void ducks (const Job& job)
    {
        const auto input = inputFor (job);
        if (input.getNumSamples() == 0) { std::printf ("no input\n"); return; }
        job.out.createDirectory();
        int latency = 0;
        const auto full = run (input, parametersFor (job.setup), latency);
        const auto fullBands = bandLevels (full), inBands = bandLevels (input);
        struct Off { const char* unit; std::vector<std::pair<juce::String, float>> sets; juce::Colour colour; };
        const std::vector<Off> units {
            { "ADAPTIVE ENHANCER", { { "enhStrength", 0.0f }, { "sub", 0.0f }, { "footstep", 0.0f } }, juce::Colour (0xffe8a33c) },
            { "UPWARD LEVELER", { { "lumenActive", 0.0f } }, juce::Colour (0xffc8a060) },
            { "DEEP SUB", { { "deepActive", 0.0f } }, juce::Colour (0xff5a7cff) },
            { "SPECTRAL LIMITER", { { "spectralActive", 0.0f } }, juce::Colour (0xffff4f8b) },
            { "MIX BALANCER", { { "balActive", 0.0f } }, juce::Colour (0xfff0e080) },
            { "ADAPTIVE COMPRESSOR", { { "tideActive", 0.0f } }, juce::Colour (0xff4fd6d6) },
            { "TONE & SPACE", { { "seraphMode", 0.0f } }, juce::Colour (0xffb388ff) },
        };
        std::vector<std::array<std::vector<float>, 3>> effect;
        juce::String text;
        text << "Who ducks what: each unit's effect on each band over time (the rack minus the rack without it)\n"
             << (job.inFile.isNotEmpty() ? job.inFile : job.scene) << " through " << job.setup.preset << "\n\n"
             << "unit                  deepest duck  low / mid / high (dB)     time of the deepest mid duck\n";
        for (auto& u : units)
        {
            auto setup = job.setup;
            for (auto& s : u.sets) setup.sets.push_back (s);
            int lat = 0;
            const auto without = bandLevels (run (input, parametersFor (setup), lat));
            std::array<std::vector<float>, 3> e;
            std::array<float, 3> deepest {};
            float deepestAt = 0.0f;
            for (size_t k = 0; k < 3; ++k)
                for (size_t i = 0; i < fullBands[k].size(); ++i)
                {
                    const float d = inBands[k][i] < -65.0f ? 0.0f : fullBands[k][i] - without[k][i];
                    e[k].push_back (d);
                    if (d < deepest[k]) { deepest[k] = d; if (k == 1) deepestAt = (float) i * 0.05f; }
                }
            effect.push_back (e);
            text << juce::String (u.unit).paddedRight (' ', 22) << fmt (deepest[0]).paddedLeft (' ', 8) << fmt (deepest[1]).paddedLeft (' ', 7)
                 << fmt (deepest[2]).paddedLeft (' ', 7) << "            " << fmt (deepestAt, 2) << " s\n";
        }
        job.out.getChildFile ("ducks.txt").replaceWithText (text);
        std::printf ("%s", text.toRawUTF8());

        // One panel per band: the input's level as a grey fill for orientation, each unit's effect as a line
        const int w = 1100, left = 60, top = 34, panelH = 190, gap = 34;
        juce::Image img (juce::Image::RGB, w + left + 200, top + 3 * (panelH + gap) + 20, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xff0b0b0e));
        g.setColour (juce::Colours::white);
        g.setFont (font (15.0f, true));
        g.drawText ("WHO DUCKS WHAT  -  " + (job.inFile.isNotEmpty() ? job.inFile : job.scene) + " / " + job.setup.preset, left, 6, w, 20,
                    juce::Justification::left);
        const char* bandNames[3] { "LOW < 150 Hz", "MID 150 Hz - 2 kHz", "HIGH > 2 kHz" };
        const double seconds = input.getNumSamples() / sr;
        for (size_t k = 0; k < 3; ++k)
        {
            const juce::Rectangle<int> area (left, top + (int) k * (panelH + gap), w, panelH);
            const size_t n = inBands[k].size();
            for (size_t i = 0; i < n; ++i)   // the input's level, faint, for orientation (-80 .. 0 dB)
            {
                const float x = (float) area.getX() + (float) area.getWidth() * (float) i / (float) std::max<size_t> (1, n - 1);
                const float h = (float) area.getHeight() * juce::jlimit (0.0f, 1.0f, (inBands[k][i] + 80.0f) / 80.0f);
                g.setColour (juce::Colour (0xff24242c));
                g.drawVerticalLine ((int) x, (float) area.getBottom() - h, (float) area.getBottom());
            }
            for (float d : { -12.f, -6.f, 0.f, 6.f })
            {
                const float y = (float) area.getY() + (float) area.getHeight() * (6.0f - d) / 18.0f;
                g.setColour (juce::Colours::white.withAlpha (d == 0 ? 0.45f : 0.15f));
                g.drawHorizontalLine ((int) y, (float) left, (float) (left + w));
                g.setColour (juce::Colours::white.withAlpha (0.7f)); g.setFont (font (11.0f));
                g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), 4, (int) y - 7, 50, 14, juce::Justification::right);
            }
            for (size_t u = 0; u < units.size(); ++u)
            {
                juce::Path p;
                const auto& e = effect[u][k];
                for (size_t i = 0; i < e.size(); ++i)
                {
                    const float x = (float) area.getX() + (float) area.getWidth() * (float) i / (float) std::max<size_t> (1, e.size() - 1);
                    const float y = (float) area.getY() + (float) area.getHeight() * juce::jlimit (0.0f, 1.0f, (6.0f - e[i]) / 18.0f);
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (units[u].colour);
                g.strokePath (p, juce::PathStrokeType (1.6f));
            }
            g.setColour (juce::Colours::white); g.setFont (font (12.0f, true));
            g.drawText (bandNames[k], area.getX() + 6, area.getY() + 4, 300, 14, juce::Justification::left);
            g.setColour (juce::Colours::white.withAlpha (0.3f)); g.drawRect (area);
            timeAxis (g, area, seconds);
        }
        for (size_t u = 0; u < units.size(); ++u)
        {
            g.setColour (units[u].colour);
            g.fillRect (left + w + 16, top + 10 + (int) u * 22, 14, 3);
            g.setColour (juce::Colours::white); g.setFont (font (12.0f));
            g.drawText (units[u].unit, left + w + 36, top + 3 + (int) u * 22, 170, 16, juce::Justification::left);
        }
        save (img, job.out.getChildFile ("ducks.png"));
    }

    int suite (const juce::File& dir)
    {
        const std::vector<std::pair<const char*, std::vector<const char*>>> plan {
            { "game",      { "DEFAULT", "COMPETITIVE FOOTSTEPS", "IMMERSIVE GAMES", "TRANSPARENT (ALL OUT)" } },
            { "music",     { "DEFAULT", "MUSIC: WARM MASTER", "MUSIC: WIDE & AIRY", "DEEP SUB: SUBMARINE" } },
            { "drumsbass", { "DEFAULT", "BASS HEAVY, PROTECTED", "DEEP SUB: SUBMARINE" } },
            { "bassline",  { "DEFAULT", "DEEP SUB: SUBMARINE" } },
            { "explosion", { "DEFAULT", "COMPETITIVE FOOTSTEPS", "NIGHT MODE", "DEEP SUB: SUBMARINE" } },
            { "quiet",     { "DEFAULT", "COMPETITIVE FOOTSTEPS", "NIGHT MODE" } },
            { "voice",     { "DEFAULT", "VOICE & STREAMING" } },
            { "sweep",     { "DEFAULT", "TRANSPARENT (ALL OUT)", "DEEP SUB: SUBMARINE" } },
            { "tones",     { "DEFAULT", "TRANSPARENT (ALL OUT)", "DEEP SUB: SUBMARINE" } },
            { "impulses",  { "DEFAULT", "IMMERSIVE GAMES" } },
            { "pink",      { "DEFAULT", "TRANSPARENT (ALL OUT)" } },
        };
        juce::String summary;
        summary << "EnhAudioLab suite: every scene through a set of presets (48 kHz, 8 s each). Details in each folder.\n\n"
                << "scene      preset                    LUFS in  out   peak out  TP out  LRA in  out  low d  pump lo  pump hi  clicks  THD1k\n";
        for (auto& [scene, presets] : plan)
            for (auto* preset : presets)
            {
                Job job;
                job.scene = scene;
                job.setup.preset = preset;
                const auto folder = juce::String (scene) + "__"
                                  + juce::String (preset).toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789 ").trim().replaceCharacter (' ', '-');
                job.out = dir.getChildFile (folder);
                std::printf ("  %-10s %-26s ...", scene, preset);
                std::fflush (stdout);
                const auto r = render (job);
                std::printf (" %.1f -> %.1f LUFS\n", r.in.lufsI, r.out.lufsI);
                summary << juce::String (scene).paddedRight (' ', 11) << juce::String (preset).paddedRight (' ', 26)
                        << fmt (r.in.lufsI).paddedLeft (' ', 6) << fmt (r.out.lufsI).paddedLeft (' ', 6)
                        << fmt (r.out.peakDb).paddedLeft (' ', 10) << fmt (r.out.truePeakDb).paddedLeft (' ', 8)
                        << fmt (r.in.lra).paddedLeft (' ', 8) << fmt (r.out.lra).paddedLeft (' ', 5)
                        << ((r.lowDelta >= 0 ? "+" : "") + fmt (r.lowDelta)).paddedLeft (' ', 7)
                        << fmt (r.lowSpread).paddedLeft (' ', 9) << fmt (r.highSpread).paddedLeft (' ', 9)
                        << juce::String (r.clicks).paddedLeft (' ', 8)
                        << (juce::String (scene) == "tones" ? fmt (r.thd1k, 3) : juce::String ("-")).paddedLeft (' ', 7) << "\n";
            }
        summary << "\nLUFS: integrated.  peak / TP: sample peak and true peak of the output (dBFS / dBTP).  LRA: loudness range (LU).\n"
                   "low d: change in 20-150 Hz energy (dB).  pump lo / hi: spread (sd, dB) of the gain the rack applied over time in the\n"
                   "low (< 150 Hz) and high (> 2 kHz) bands: how much it moves.  clicks: discontinuities in the output that are not in the\n"
                   "input.  THD1k: distortion of the 1 kHz tone (%).\n";
        dir.getChildFile ("summary.txt").replaceWithText (summary);
        std::printf ("\n%s", summary.toRawUTF8());
        return 0;
    }
}

namespace lab
{
    //==============================================================================
    /** check: the self-test. Every preset through the scenes that matter for it, measured, held to hard
        rules (things that must never happen) and compared with the stored baseline (things that must not
        get worse without anyone noticing). Exit code 0 only when everything holds. */
    struct CheckRun
    {
        juce::String scene, preset, label;
        std::vector<std::pair<juce::String, float>> sets;
        Result r;
        juce::String key() const { return scene + " | " + label; }
    };

    std::vector<CheckRun> checkPlan()
    {
        std::vector<CheckRun> plan;
        auto add = [&] (const char* scene, const juce::String& preset, std::vector<std::pair<juce::String, float>> sets = {}, juce::String label = {})
        {
            plan.push_back ({ scene, preset, label.isEmpty() ? preset : label, std::move (sets), {} });
        };
        // Every preset on the scenes every preset must get right
        for (auto& p : pad::presets::factory())
            for (auto* scene : { "game", "music", "explosion", "start", "silence", "wide", "antiphase", "gaps" })
                add (scene, p.name);
        // The rest of the scenes on the presets they are about
        for (auto* scene : { "drumsbass", "bassline", "bassduck", "steps", "quiet", "voice", "sweep", "tones", "impulses", "pink" })
            for (auto* preset : { "DEFAULT", "TRANSPARENT (ALL OUT)", "DEEP SUB: SUBMARINE", "COMPETITIVE FOOTSTEPS", "MASTERING: ANALOG BUS" })
                add (scene, preset);
        // CHARACTER: every model, driven hard, with GRIT and without
        for (int model = 0; model < 9; ++model)
            for (int grit = 0; grit < 2; ++grit)
                for (auto* scene : { "music", "tones" })
                    add (scene, "DEFAULT", { { "charActive", 1.0f }, { "charModelA", (float) model }, { "charModelB", (float) model },
                                             { "charDrive", 9.0f }, { "charGrit", (float) grit } },
                         "CHARACTER model " + juce::String (model) + " drive 9" + (grit ? " GRIT" : " clean"));
        return plan;
    }

    /** Hard rules: none of these may ever happen, whatever the preset. */
    juce::StringArray hardRules (const CheckRun& c)
    {
        juce::StringArray f;
        const auto& r = c.r;
        if (! r.finite) f.add ("output not finite (NaN / inf)");
        if (r.out.clipped > 0) f.add ("samples over full scale: " + juce::String (r.out.clipped));
        if (r.out.truePeakDb > 0.3f) f.add ("true peak " + fmt (r.out.truePeakDb, 2) + " dBTP (over 0)");
        if (r.clicks > 0) f.add (juce::String (r.clicks) + " new click(s)");
        if (std::abs (r.out.dc) > 1.0e-3f) f.add ("DC offset " + juce::String (r.out.dc, 5));
        if (c.scene == "start" && r.surgeDb > 2.0f) f.add ("surges " + fmt (r.surgeDb) + " dB past its settled level when sound starts");
        if (c.scene == "silence" && r.tailDb > -80.0f) f.add ("not silent 4 s after the music stops: " + fmt (r.tailDb) + " dBFS");
        if (std::abs (r.balanceDb) > 1.5f) f.add ("leans to one side: " + fmt (r.balanceDb) + " dB left minus right");
        // Mono compatibility: a truly mono input (left = right) stays largely mono, and nothing that went in
        // in phase comes out out of phase. (A mostly-mono mix is not held to it: a mono explosion taken down
        // under a wide bed rightly leaves a wide output.)
        if (r.in.correlation > 0.999f && r.out.correlation < 0.5f) f.add ("mono in, not mono-compatible out (correlation " + fmt (r.out.correlation, 2) + ")");
        if (r.in.correlation >= 0.0f && r.out.correlation < -0.1f) f.add ("out of phase (correlation " + fmt (r.out.correlation, 2) + ")");
        if (r.in.lufsI > -40.0f && std::abs (r.out.lufsI - r.in.lufsI) > 9.0f) f.add ("loudness moved " + fmt (r.out.lufsI - r.in.lufsI) + " LU");
        return f;
    }

    juce::var metricsOf (const Result& r)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("lufs", r.out.lufsI);        o->setProperty ("tp", r.out.truePeakDb);
        o->setProperty ("lra", r.out.lra);           o->setProperty ("lraIn", r.in.lra);
        o->setProperty ("pumpLo", r.lowSpread);      o->setProperty ("pumpHi", r.highSpread);
        o->setProperty ("lowD", r.lowDelta);         o->setProperty ("thd", r.thd1k);
        o->setProperty ("surge", r.surgeDb);         o->setProperty ("tail", r.tailDb);
        o->setProperty ("corr", r.out.correlation);  o->setProperty ("balance", r.balanceDb);
        return juce::var (o);
    }

    /** Against the baseline: WORSE (fails), CHANGED (fails: someone must look, then --update). */
    juce::StringArray againstBaseline (const CheckRun& c, const juce::var& base)
    {
        juce::StringArray f;
        if (! base.isObject()) { f.add ("NEW: not in the baseline"); return f; }
        auto b = [&] (const char* k) { return (float) (double) base.getProperty (k, 0.0); };
        const auto& r = c.r;
        if (std::abs (r.out.lufsI - b ("lufs")) > 1.0f)           f.add ("CHANGED loudness " + fmt (b ("lufs")) + " -> " + fmt (r.out.lufsI) + " LUFS");
        if (r.out.truePeakDb > b ("tp") + 0.5f && r.out.truePeakDb > -1.0f) f.add ("WORSE true peak " + fmt (b ("tp")) + " -> " + fmt (r.out.truePeakDb) + " dBTP");
        if (r.in.lra >= 2.0f && r.out.lra < b ("lra") - 1.0f)     f.add ("WORSE dynamics (LRA) " + fmt (b ("lra")) + " -> " + fmt (r.out.lra) + " LU");
        if (r.lowSpread > b ("pumpLo") + 0.5f)                   f.add ("WORSE low-band pumping " + fmt (b ("pumpLo")) + " -> " + fmt (r.lowSpread) + " dB");
        if (r.highSpread > b ("pumpHi") + 0.5f)                  f.add ("WORSE high-band pumping " + fmt (b ("pumpHi")) + " -> " + fmt (r.highSpread) + " dB");
        if (std::abs (r.lowDelta - b ("lowD")) > 1.5f)            f.add ("CHANGED low end " + fmt (b ("lowD")) + " -> " + fmt (r.lowDelta) + " dB");
        if (r.thd1k > b ("thd") + 1.0f)                          f.add ("WORSE THD at 1 kHz " + fmt (b ("thd"), 2) + " -> " + fmt (r.thd1k, 2) + " %");
        if (r.surgeDb > b ("surge") + 1.0f)                      f.add ("WORSE start surge " + fmt (b ("surge")) + " -> " + fmt (r.surgeDb) + " dB");
        if (r.out.correlation < b ("corr") - 0.1f)               f.add ("WORSE stereo correlation " + fmt (b ("corr"), 2) + " -> " + fmt (r.out.correlation, 2));
        if (r.tailDb > b ("tail") + 6.0f && r.tailDb > -100.0f)  f.add ("WORSE tail after silence " + fmt (b ("tail")) + " -> " + fmt (r.tailDb) + " dBFS");
        return f;
    }

    int check (const juce::StringArray& args)
    {
        factoryPresetsOnly = true;
        juce::File baselineFile = juce::File::getCurrentWorkingDirectory().getChildFile ("Tests/lab-baseline.json");
        juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile ("build/lab/check");
        bool update = false;
        int threads = std::max (1, std::min (3, (int) std::thread::hardware_concurrency() - 1));
        juce::String only;
        for (int i = 1; i < args.size(); ++i)
        {
            if (args[i] == "--update") update = true;
            else if (args[i] == "--baseline" && i + 1 < args.size()) baselineFile = juce::File::getCurrentWorkingDirectory().getChildFile (args[++i]);
            else if (args[i] == "--out" && i + 1 < args.size()) out = juce::File::getCurrentWorkingDirectory().getChildFile (args[++i]);
            else if (args[i] == "--threads" && i + 1 < args.size()) threads = std::max (1, args[++i].getIntValue());
            else if (args[i] == "--only" && i + 1 < args.size()) only = args[++i];   // runs whose key contains this
        }

        auto plan = checkPlan();
        if (only.isNotEmpty())
            plan.erase (std::remove_if (plan.begin(), plan.end(), [&] (const CheckRun& c) { return ! c.key().containsIgnoreCase (only); }), plan.end());
        std::printf ("EnhAudioLab check: %d runs on %d threads (factory presets, 48 kHz, 8 s each)\n", (int) plan.size(), threads);

        std::atomic<int> next { 0 }, done { 0 };
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back ([&]
            {
                for (int i; (i = next++) < (int) plan.size();)
                {
                    auto& c = plan[(size_t) i];
                    const auto input = makeScene (c.scene, 8.0);
                    int latency = 0;
                    const auto output = run (input, parametersFor ({ c.preset, c.sets }), latency);
                    c.r = analyse (input, output, c.scene);
                    const int d = ++done;
                    if (d % 10 == 0 || d == (int) plan.size())
                    {
                        std::printf ("  %d / %d\n", d, (int) plan.size());
                        std::fflush (stdout);
                    }
                }
            });
        for (auto& t : pool) t.join();

        const auto baseline = baselineFile.existsAsFile() ? juce::JSON::parse (baselineFile) : juce::var();
        juce::String text;
        text << "EnhAudioLab check - " << juce::Time::getCurrentTime().toString (true, true) << "\n\n"
             << "scene      run                                         LUFS in   out    TP out   LRA in  out  pump lo  hi   low d  surge   tail  corr  bal\n";
        int hard = 0, regress = 0;
        juce::String failures;
        auto* newBase = new juce::DynamicObject();
        for (auto& c : plan)
        {
            const auto& r = c.r;
            text << c.scene.paddedRight (' ', 11) << c.label.substring (0, 42).paddedRight (' ', 42)
                 << fmt (r.in.lufsI).paddedLeft (' ', 8) << fmt (r.out.lufsI).paddedLeft (' ', 6) << fmt (r.out.truePeakDb, 2).paddedLeft (' ', 9)
                 << fmt (r.in.lra).paddedLeft (' ', 8) << fmt (r.out.lra).paddedLeft (' ', 5)
                 << fmt (r.lowSpread).paddedLeft (' ', 8) << fmt (r.highSpread).paddedLeft (' ', 5) << fmt (r.lowDelta).paddedLeft (' ', 7)
                 << (c.scene == "start" ? fmt (r.surgeDb) : juce::String ("-")).paddedLeft (' ', 7)
                 << (c.scene == "silence" ? fmt (r.tailDb, 0) : juce::String ("-")).paddedLeft (' ', 7)
                 << fmt (r.out.correlation, 2).paddedLeft (' ', 6) << fmt (r.balanceDb).paddedLeft (' ', 5) << "\n";
            newBase->setProperty (c.key(), metricsOf (r));
            for (auto& f : hardRules (c))       { ++hard;    failures << "  RULE     " << c.key() << ": " << f << "\n"; }
            if (! update)
                for (auto& f : againstBaseline (c, baseline.getProperty (c.key(), {})))
                    { ++regress; failures << "  BASELINE " << c.key() << ": " << f << "\n"; }
        }
        text << "\n" << (failures.isEmpty() ? juce::String ("Everything holds.\n") : "Failures:\n" + failures);
        text << "\nRULES (always): finite; nothing over full scale; true peak <= +0.3 dBTP (0 dBFS ceiling, meter tolerance); no new clicks;\n"
                "DC under 0.001; 'start' surges no more than 2 dB; 'silence' under -80 dBFS 4 s after the music; left / right\n"
                "balance within 1.5 dB of the input's; mono in (L = R) stays mono-compatible (correlation >= 0.5) and nothing goes out of\n"
                "phase; loudness within 9 LU.\n"
                "BASELINE (Tests/lab-baseline.json; --update after a change you meant): loudness +-1 LU, true peak +0.5 dB, LRA -1 LU,\n"
                "pumping +0.5 dB, low end +-1.5 dB, THD +1 %, start surge +1 dB, correlation -0.1, tail +6 dB.\n";
        out.createDirectory();
        out.getChildFile ("check.txt").replaceWithText (text);
        std::printf ("\n%s", failures.isEmpty() ? "Everything holds.\n" : ("Failures:\n" + failures).toRawUTF8());
        std::printf ("\n%d hard-rule failure(s), %d baseline difference(s). Table: %s\n", hard, regress, out.getChildFile ("check.txt").getFullPathName().toRawUTF8());

        if (update)
        {
            baselineFile.replaceWithText (juce::JSON::toString (juce::var (newBase), false));
            std::printf ("baseline written: %s (%d runs)\n", baselineFile.getFullPathName().toRawUTF8(), (int) plan.size());
        }
        else
            delete newBase;
        return hard > 0 || regress > 0 ? 1 : 0;
    }
}

int main (int argc, char** argv)
{
    using namespace lab;
    juce::ScopedJuceInitialiser_GUI gui;   // fonts for the pictures
    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);
    if (args.isEmpty() || args[0] == "-h" || args[0] == "--help")
    {
        std::printf ("EnhAudioLab scenes | render | contrib | ducks | trace | compare | suite | check   (see the top of Tools/AudioLab.cpp)\n");
        return 0;
    }
    const auto command = args[0];
    Job job;
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile ("audiolab");
    juce::StringArray files;
    for (int i = 1; i < args.size(); ++i)
    {
        const auto a = args[i];
        auto next = [&] { return i + 1 < args.size() ? args[++i] : juce::String(); };
        if (a == "--scene") job.scene = next();
        else if (a == "--in") job.inFile = next();
        else if (a == "--preset") job.setup.preset = next();
        else if (a == "--seconds") job.seconds = next().getDoubleValue();
        else if (a == "--out") out = juce::File::getCurrentWorkingDirectory().getChildFile (next());
        else if (a == "--set")
        {
            const auto kv = next();
            job.setup.sets.push_back ({ kv.upToFirstOccurrenceOf ("=", false, false), kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
        }
        else files.add (a);
    }
    job.out = out;

    if (command == "scenes")
    {
        for (auto& s : sceneList) std::printf ("  %-10s %s\n", s.name, s.what);
        return 0;
    }
    if (command == "render")
    {
        const auto r = render (job);
        std::printf ("%s", out.getChildFile ("report.txt").loadFileAsString().toRawUTF8());
        std::printf ("\nwrote %s\n", out.getFullPathName().toRawUTF8());
        return r.in.lufsI > -200 ? 0 : 1;
    }
    if (command == "contrib") { contrib (job); return 0; }
    if (command == "ducks") { ducks (job); return 0; }
    if (command == "trace") { trace (job); return 0; }
    if (command == "compare" && files.size() >= 2)
    {
        const auto a = readWav (juce::File::getCurrentWorkingDirectory().getChildFile (files[0]));
        const auto b = readWav (juce::File::getCurrentWorkingDirectory().getChildFile (files[1]));
        const int n = std::min (a.getNumSamples(), b.getNumSamples());
        Buffer aa (2, n), bb (2, n);
        for (int c = 0; c < 2; ++c) { aa.copyFrom (c, 0, a, c, 0, n); bb.copyFrom (c, 0, b, c, 0, n); }
        out.createDirectory();
        const auto sa = stats (aa), sb = stats (bb);
        const auto spa = averageSpectrum (aa), spb = averageSpectrum (bb);
        const auto title = files[0] + "  vs  " + files[1];
        out.getChildFile ("report.txt").replaceWithText (report (title, aa, bb, 0, sa, sb, spa, spb, {}));
        spectrogramImage (aa, bb, out.getChildFile ("spectrogram.png"), title);
        spectrumImage (spa, spb, out.getChildFile ("spectrum.png"), title);
        waveformImage (aa, bb, sa, sb, out.getChildFile ("waveform.png"), title);
        spectrogramImage (aa, bb, out.getChildFile ("lowend.png"), title + "  -  LOW END", 10.0f, 250.0f, 15, -120.0f);
        std::printf ("%s", out.getChildFile ("report.txt").loadFileAsString().toRawUTF8());
        return 0;
    }
    if (command == "suite") return suite (out);
    if (command == "check") return check (args);
    std::printf ("unknown command (EnhAudioLab --help)\n");
    return 1;
}
