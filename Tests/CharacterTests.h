// CHARACTER unit tests (included into EnhDspTests.cpp, after check()).
//   EnhDspTests --character          the checks
//   EnhDspTests --character table    also the calibration table: THD per model, level and drive

namespace chartest
{
    using enh::dsp::Character;

    /** Runs a stereo sine through one model; returns { fundamental amplitude, THD (harmonics below
        Nyquist, %), everything else - aliases and noise - against the fundamental (dB) }. */
    struct Measure { float fundamental = 0, thd = 0, otherDb = -200; };

    inline Measure sineWith (int model, float drive, float hz, float levelDb, bool grit, double rate = 48000.0, int components = 0, float colour = 5.0f)
    {
        Character ch;
        ch.prepare (rate, 256, 2);
        ch.setMaxBlock (256);
        Character::Settings s;
        s.active = true; s.modelA = model; s.modelB = model; s.blend = 0.0f; s.drive = drive; s.components = components; s.grit = grit;
        s.colour = colour;

        // 8 s to settle: the unit's loudness match (1.5 s) has to have found its level, or the fit reads
        // its last glide as distortion
        const int total = (int) (rate * 9.0), skip = (int) (rate * 8.0);
        const float amp = std::pow (10.0f, levelDb / 20.0f);
        std::vector<float> out;
        out.reserve ((size_t) total);

        juce::AudioBuffer<float> buf (2, 256);
        for (int pos = 0; pos < total; pos += 256)
        {
            const int n = std::min (256, total - pos);
            for (int i = 0; i < n; ++i)
            {
                const float v = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (pos + i) / rate);
                buf.setSample (0, i, v);
                buf.setSample (1, i, v);
            }
            ch.process (buf.getArrayOfWritePointers(), 2, n, s);
            for (int i = 0; i < n; ++i)
                out.push_back (buf.getSample (0, i));
        }

        // Least-squares fit of the fundamental and each harmonic below Nyquist; what is left is the rest
        std::vector<double> x (out.begin() + skip, out.end());
        const int N = (int) x.size();
        auto fit = [&] (double f, bool remove)
        {
            double sc = 0, cc = 0;
            for (int i = 0; i < N; ++i)
            {
                const double ph = 2.0 * juce::MathConstants<double>::pi * f * (skip + i) / rate;
                sc += x[(size_t) i] * std::sin (ph);
                cc += x[(size_t) i] * std::cos (ph);
            }
            const double a = 2.0 * sc / N, b = 2.0 * cc / N;
            if (remove)
                for (int i = 0; i < N; ++i)
                {
                    const double ph = 2.0 * juce::MathConstants<double>::pi * f * (skip + i) / rate;
                    x[(size_t) i] -= a * std::sin (ph) + b * std::cos (ph);
                }
            return std::sqrt (a * a + b * b);
        };

        Measure m;
        const double fund = fit (hz, true);
        double harm = 0;
        for (int k = 2; k * hz < rate * 0.5 - 50.0; ++k)
        {
            const double h = fit (k * hz, true);
            harm += h * h;
        }
        double rest = 0;
        for (auto v : x) rest += v * v;
        rest = std::sqrt (2.0 * rest / N);   // as a peak amplitude
        m.fundamental = (float) fund;
        m.thd = (float) (100.0 * std::sqrt (harm) / std::max (1e-12, fund));
        m.otherDb = (float) (20.0 * std::log10 (std::max (1e-12, rest) / std::max (1e-12, fund)));
        return m;
    }

    inline Measure sine (int model, float drive, float hz, float levelDb, double rate = 48000.0, int components = 0)
    {
        return sineWith (model, drive, hz, levelDb, true, rate, components);
    }

    /** A model's voicing: two tones at -30 dBFS each (a single one would be level-matched away), and how
        much the model moves the first against the second, in dB. */
    inline float voicingDb (int model, float colour, float hzA, float hzB)
    {
        Character ch;
        ch.prepare (48000.0, 256, 2);
        ch.setMaxBlock (256);
        Character::Settings s;
        s.active = true; s.modelA = s.modelB = model; s.drive = 5.0f; s.grit = false; s.colour = colour;
        const int total = 48000 * 6, skip = 48000 * 4;
        const double amp = std::pow (10.0, -30.0 / 20.0), tau = 2.0 * juce::MathConstants<double>::pi;
        double sa = 0, ca = 0, sb = 0, cb = 0;
        juce::AudioBuffer<float> buf (2, 256);
        for (int pos = 0; pos < total; pos += 256)
        {
            for (int i = 0; i < 256; ++i)
            {
                const double t = (pos + i) / 48000.0;
                const float v = (float) (amp * (std::sin (tau * hzA * t) + std::sin (tau * hzB * t)));
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            ch.process (buf.getArrayOfWritePointers(), 2, 256, s);
            if (pos >= skip)
                for (int i = 0; i < 256; ++i)
                {
                    // Against the input's phase, shifted by the unit's latency
                    const double t = (pos + i - ch.getLatencySamples()) / 48000.0;
                    const double y = buf.getSample (0, i);
                    sa += y * std::sin (tau * hzA * t); ca += y * std::cos (tau * hzA * t);
                    sb += y * std::sin (tau * hzB * t); cb += y * std::cos (tau * hzB * t);
                }
        }
        return (float) (10.0 * std::log10 ((sa * sa + ca * ca) / std::max (1e-30, sb * sb + cb * cb)));
    }

    /** Pink-ish noise at -18 dBFS through a model: output RMS change in dB. */
    inline float levelChange (int model, float drive, float colour = 5.0f)
    {
        Character ch;
        ch.prepare (48000.0, 256, 2);
        ch.setMaxBlock (256);
        Character::Settings s;
        s.active = true; s.modelA = s.modelB = model; s.drive = drive; s.colour = colour;
        juce::Random r (7);
        float b0 = 0, b1 = 0, b2 = 0;
        double inE = 0, outE = 0;
        // Loudness as it is heard (K-weighted, BS.1770), which is what CHARACTER keeps: plain RMS read a
        // model whose voicing tilts the spectrum (ARENA: less 280 Hz, more 3.2 kHz) as quieter than it sounds
        enh::dsp::BiquadCoeffs pre, rlb;
        enh::dsp::kWeighting (48000.0, pre, rlb);
        enh::dsp::BiquadState inPre, inRlb, outPre, outRlb;
        juce::AudioBuffer<float> buf (2, 256);
        for (int blk = 0; blk < 48000 * 10 / 256; ++blk)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float w = r.nextFloat() * 2.0f - 1.0f;
                b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f;
                const float v = (b0 + b1 + b2 + w * 0.1848f) * 0.05f;
                buf.setSample (0, i, v); buf.setSample (1, i, v);
                const float k = inRlb.process (rlb, inPre.process (pre, v));
                if (blk > 1125) inE += (double) k * k;
            }
            ch.process (buf.getArrayOfWritePointers(), 2, 256, s);
            for (int i = 0; i < 256; ++i)
            {
                const float k = outRlb.process (rlb, outPre.process (pre, buf.getSample (0, i)));
                if (blk > 1125) outE += (double) k * k;
            }
        }
        return (float) (10.0 * std::log10 (outE / inE));
    }
}

static void runCharacterTests (bool table)
{
    using namespace chartest;
    std::printf ("\n== CHARACTER ==\n");

    if (table)
    {
        std::printf ("  THD %% (harmonics below Nyquist) at 100 Hz / 1 kHz\n");
        std::printf ("  %-17s | drive 0: -18   -6    0 dBFS | drive 5: -18   -6    0 | drive 10: -18   -6    0\n", "model");
        for (int m = 0; m < Character::numModels; ++m)
        {
            for (float hz : { 100.0f, 1000.0f })
            {
                std::printf ("  %-12s %5.0f", m == 0 || hz == 100.0f ? Character::names[(size_t) m] : "", hz);
                for (float drive : { 0.0f, 5.0f, 10.0f })
                {
                    std::printf (" |");
                    for (float lv : { -18.0f, -6.0f, 0.0f })
                        std::printf (" %6.2f", sine (m, drive, hz, lv).thd);
                }
                std::printf ("\n");
            }
        }
        std::printf ("  level change on pink noise at -18 dBFS, drive 5 (dB):");
        for (int m = 0; m < Character::numModels; ++m)
            std::printf (" %+.2f", levelChange (m, 5.0f));
        std::printf ("\n");
    }

    // 1. Aliasing. At the default DRIVE a loud 7 kHz tone must stay alias-free (its harmonics above
    //    24 kHz filtered, not folded back); at full DRIVE, where the tone is half square, a little is allowed.
    for (int m = 0; m < Character::numModels; ++m)
    {
        const auto r = sine (m, 5.0f, 7000.0f, -6.0f);
        check (r.otherDb < -75.0f, juce::String (Character::names[(size_t) m]) + ": 7 kHz at -6 dBFS, drive 5 - aliases and noise "
                                       + juce::String (r.otherDb, 1) + " dB (< -75)");
        const auto h = sine (m, 10.0f, 7000.0f, -3.0f);
        check (h.otherDb < -50.0f, juce::String (Character::names[(size_t) m]) + ": 7 kHz at -3 dBFS, drive 10 - aliases and noise "
                                       + juce::String (h.otherDb, 1) + " dB (< -50)");
    }

    // 2. Working level stays clean at the default drive; a quiet 1 kHz tone is not coloured audibly
    for (int m = 0; m < Character::numModels; ++m)
    {
        const auto r = sine (m, 5.0f, 1000.0f, -18.0f);
        check (r.thd < 1.0f, juce::String (Character::names[(size_t) m]) + ": 1 kHz at -18 dBFS, drive 5 - THD "
                                 + juce::String (r.thd, 3) + " % (< 1 %)");
    }

    // 3. More drive, more colour (never less): THD at -6 dBFS, 100 Hz, rises with DRIVE
    for (int m = 0; m < Character::numModels; ++m)
    {
        const float a = sine (m, 0.0f, 100.0f, -6.0f).thd, b = sine (m, 5.0f, 100.0f, -6.0f).thd, c = sine (m, 10.0f, 100.0f, -6.0f).thd;
        check (a <= b + 0.01f && b <= c + 0.01f, juce::String (Character::names[(size_t) m]) + ": colour grows with DRIVE ("
                                                   + juce::String (a, 2) + " / " + juce::String (b, 2) + " / " + juce::String (c, 2) + " %)");
    }

    // 4. DRIVE is given back: pink noise at -18 dBFS keeps its loudness (K-weighted) within 1.5 dB at every drive
    for (int m = 0; m < Character::numModels; ++m)
    {
        float worst = 0.0f;
        for (float d : { 0.0f, 5.0f, 10.0f })
            worst = std::max (worst, std::abs (levelChange (m, d)));
        check (worst < 1.5f, juce::String (Character::names[(size_t) m]) + ": level kept within " + juce::String (worst, 2) + " dB (< 1.5)");
    }

    // 5. OUT is a pure delay of the reported latency; IN, BLEND and model changes never step the audio
    {
        Character ch;
        ch.prepare (48000.0, 256, 2);
        ch.setMaxBlock (256);
        const int lat = ch.getLatencySamples();
        Character::Settings s;   // inactive
        juce::AudioBuffer<float> buf (2, 256);
        std::vector<float> in, out;
        juce::Random r (3);
        for (int blk = 0; blk < 20; ++blk)
        {
            for (int i = 0; i < 256; ++i) { const float v = r.nextFloat() - 0.5f; buf.setSample (0, i, v); buf.setSample (1, i, v); in.push_back (v); }
            ch.process (buf.getArrayOfWritePointers(), 2, 256, s);
            for (int i = 0; i < 256; ++i) out.push_back (buf.getSample (0, i));
        }
        bool exact = true;
        for (size_t i = (size_t) lat; i < out.size(); ++i)
            exact = exact && out[i] == in[i - (size_t) lat];
        check (exact, "OUT: bit-exact, delayed by the reported latency (" + juce::String (lat) + " samples)");

        // A slow sine while everything is switched about: no sample-to-sample jump beyond the sine's own
        float worstStep = 0.0f, prev = 0.0f;
        int t = 0, worstAt = 0;
        const int schedule[][4] = { { 1, 3, 4, 0 }, { 1, 3, 4, 50 }, { 1, 0, 6, 50 }, { 1, 0, 6, 100 }, { 0, 5, 2, 100 }, { 1, 7, 8, 30 }, { 1, 8, 1, 70 } };
        for (auto& step : schedule)
        {
            s.active = step[0] == 1; s.modelA = step[1]; s.modelB = step[2]; s.blend = (float) step[3] / 100.0f; s.drive = 5.0f;
            for (int blk = 0; blk < 12; ++blk)
            {
                for (int i = 0; i < 256; ++i, ++t)
                {
                    const float v = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * t / 48000.0);
                    buf.setSample (0, i, v); buf.setSample (1, i, v);
                }
                ch.process (buf.getArrayOfWritePointers(), 2, 256, s);
                for (int i = 0; i < 256; ++i)
                {
                    const float v = buf.getSample (0, i);
                    const int at = t - 256 + i; juce::ignoreUnused (at);
                    if (t > 2000 && std::abs (v - prev) > worstStep) { worstStep = std::abs (v - prev); worstAt = t; }
                    if (std::getenv ("CHAR_STEPS") != nullptr && t > 2000 && std::abs (v - prev) > 0.012f)
                        std::printf ("    step %.4f at block %d sample %d (A %d B %d blend %.2f in %d)\n", v - prev, at / 256, at % 256, s.modelA, s.modelB, s.blend, (int) s.active);
                    prev = v;
                }
            }
        }
        // A 110 Hz sine at 0.3 moves at most 0.0043 a sample; a click would be several times that
        check (worstStep < 0.012f, "switching IN, models and BLEND is smooth (largest step " + juce::String (worstStep, 4)
                                       + " at sample " + juce::String (worstAt) + ", block " + juce::String (worstAt / 256) + ")");
    }

    // 6. Every sample rate and odd block sizes: finite, and the level where it should be
    for (double rate : { 44100.0, 96000.0 })
    {
        const auto r = sine (Character::vintage, 5.0f, 1000.0f, -6.0f, rate);
        check (std::isfinite (r.thd) && r.fundamental > 0.4f && r.fundamental < 0.63f,
               "VINTAGE at " + juce::String ((int) rate) + " Hz: stable, level " + juce::String (r.fundamental, 3));
    }
    {
        Character ch;
        ch.prepare (48000.0, 1024, 2);
        ch.setMaxBlock (1024);
        Character::Settings s; s.active = true; s.modelA = Character::valve; s.modelB = Character::tape15; s.blend = 0.4f; s.drive = 10.0f; s.components = 2;
        juce::AudioBuffer<float> buf (2, 1024);
        bool finite = true;
        juce::Random r (11);
        for (int n : { 1, 7, 33, 480, 1024, 5, 64 })
            for (int rep = 0; rep < 20; ++rep)
            {
                for (int i = 0; i < n; ++i) { buf.setSample (0, i, r.nextFloat() * 1.8f - 0.9f); buf.setSample (1, i, r.nextFloat() * 1.8f - 0.9f); }
                ch.process (buf.getArrayOfWritePointers(), 2, n, s);
                for (int i = 0; i < n; ++i) finite = finite && std::isfinite (buf.getSample (0, i)) && std::abs (buf.getSample (0, i)) < 4.0f;
            }
        check (finite, "odd block sizes, full-scale noise, drive 10, COMPONENTS vintage: finite and bounded");

    // 5. COLOUR: the character without the distortion. Its voicing is heard, its harmonics are there at a
    //    quiet level (and not at COLOUR 0), GRIT off stays clean at full COLOUR, and the loudness stays.
    std::printf ("\n  COLOUR: voicing 0 -> 10, harmonics at -30 dBFS (GRIT off) 0 -> 5 -> 10, THD at -6 dBFS COLOUR 10 GRIT off\n");
    {
        struct V { int model; float a, b; const char* what; float atLeast; };
        const V voicings[] { { Character::tape15, 52.0f, 1000.0f, "head bump at 52 Hz", 3.0f },
                             { Character::arena, 3200.0f, 1000.0f, "3.2 kHz presence", 2.0f },
                             { Character::cinema, 45.0f, 1000.0f, "weight at 45 Hz", 2.0f },
                             { Character::vintage, 60.0f, 1000.0f, "low shelf at 60 Hz", 1.5f } };
        for (auto& v : voicings)
        {
            const float at0 = voicingDb (v.model, 0.0f, v.a, v.b), at10 = voicingDb (v.model, 10.0f, v.a, v.b);
            std::printf ("  %-18s %-20s %+.2f -> %+.2f dB\n", Character::names[(size_t) v.model], v.what, at0, at10);
            check (at10 - at0 >= v.atLeast, juce::String (Character::names[(size_t) v.model]) + ": COLOUR brings its " + v.what
                   + " in (" + juce::String (at10 - at0, 1) + " dB, at least " + juce::String (v.atLeast, 1) + ")");
        }
        for (int m = 0; m < Character::numModels; ++m)
        {
            const float q0 = sineWith (m, 5.0f, 1000.0f, -30.0f, false, 48000.0, 0, 0.0f).thd;
            const float q5 = sineWith (m, 5.0f, 1000.0f, -30.0f, false, 48000.0, 0, 5.0f).thd;
            const float q10 = sineWith (m, 5.0f, 1000.0f, -30.0f, false, 48000.0, 0, 10.0f).thd;
            const float loud = sineWith (m, 5.0f, 1000.0f, -6.0f, false, 48000.0, 0, 10.0f).thd;
            std::printf ("  %-18s %.3f -> %.3f -> %.3f %%   loud %.2f %%\n", Character::names[(size_t) m], q0, q5, q10, loud);
            if (m != Character::clean)
                check (q10 >= 0.25f && q10 > 4.0f * q0, juce::String (Character::names[(size_t) m])
                       + ": COLOUR gives it character at a quiet level (" + juce::String (q10, 2) + " % at -30 dBFS)");
            check (loud <= 1.6f, juce::String (Character::names[(size_t) m]) + ": GRIT off stays clean at COLOUR 10 ("
                   + juce::String (loud, 2) + " % at -6 dBFS)");
            float worst = 0.0f;
            for (float c : { 0.0f, 10.0f })
                worst = std::max (worst, std::abs (levelChange (m, 5.0f, c)));
            check (worst < 1.5f, juce::String (Character::names[(size_t) m]) + ": loudness kept at COLOUR 0 and 10 (" + juce::String (worst, 2) + " dB)");
        }
    }
    }
}
