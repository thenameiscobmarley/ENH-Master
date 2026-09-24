// Mastering tools (3.6.4.1): COMPARE and STEREO (mid/side). Included into EnhDspTests.cpp after
// check() and presetParameters(); run with   EnhDspTests --mastering

namespace mastertest
{
    using enh::dsp::EnhEngine;

    /** Stereo pink-ish noise (independent channels), at a level. */
    inline void noise (std::vector<float>& l, std::vector<float>& r, int n, float gain, juce::uint32 seed)
    {
        juce::Random rng ((juce::int64) seed);
        float a0 = 0, a1 = 0, b0 = 0, b1 = 0;
        l.resize ((size_t) n); r.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const float wl = rng.nextFloat() * 2.0f - 1.0f, wr = rng.nextFloat() * 2.0f - 1.0f;
            a0 = 0.97f * a0 + 0.3f * wl; a1 = 0.6f * a1 + wl;
            b0 = 0.97f * b0 + 0.3f * wr; b1 = 0.6f * b1 + wr;
            l[(size_t) i] = gain * 0.25f * (a0 + 0.4f * a1);
            r[(size_t) i] = gain * 0.25f * (b0 + 0.4f * b1);
        }
    }

    /** Runs the engine over the input in 256-sample blocks; `during` may change the parameters per block. */
    inline void render (EnhEngine& e, const std::vector<float>& inL, const std::vector<float>& inR, EnhEngine::Parameters p,
                        std::vector<float>& outL, std::vector<float>& outR, std::function<void (int, EnhEngine::Parameters&)> during = {})
    {
        const int n = (int) inL.size();
        outL.assign ((size_t) n, 0.0f); outR.assign ((size_t) n, 0.0f);
        juce::AudioBuffer<float> buf (2, 256);
        for (int pos = 0; pos < n; pos += 256)
        {
            const int m = std::min (256, n - pos);
            buf.setSize (2, m, false, false, true);
            for (int i = 0; i < m; ++i) { buf.setSample (0, i, inL[(size_t) (pos + i)]); buf.setSample (1, i, inR[(size_t) (pos + i)]); }
            if (during) during (pos, p);
            e.process (buf, p);
            for (int i = 0; i < m; ++i) { outL[(size_t) (pos + i)] = buf.getSample (0, i); outR[(size_t) (pos + i)] = buf.getSample (1, i); }
        }
    }

    inline double rms (const std::vector<float>& v, int from, int to)
    {
        double s = 0;
        for (int i = from; i < to; ++i) s += (double) v[(size_t) i] * v[(size_t) i];
        return std::sqrt (s / std::max (1, to - from));
    }
    inline double db (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }
}

static void runMasteringTests (double sr)
{
    using namespace mastertest;
    std::printf ("\n== MASTERING: COMPARE ==\n");

    // A mix the rack changes audibly: noise, then COMPARE on for the second half
    const int n = (int) (sr * 16.0), half = n / 2;
    std::vector<float> inL, inR, outL, outR;
    noise (inL, inR, n, 0.25f, 5);   // under the output limiter: this checks what COMPARE plays, not the limiter
    auto p = presetParameters (presetNamed ("DEFAULT"));
    {
        EnhEngine e;
        e.prepare (sr, 256, 2);
        const int lat = e.getLatencySamples();
        render (e, inL, inR, p, outL, outR, [&] (int pos, EnhEngine::Parameters& q) { q.compare = pos >= half; });

        // Loudness: the last seconds of the rack against the last seconds of COMPARE (plain RMS on noise
        // is close enough to K-weighted for this: the rack keeps the tilt near)
        const double rack = db (rms (outL, half - (int) sr * 4, half)), cmp = db (rms (outL, n - (int) sr * 4, n));
        check (std::abs (rack - cmp) < 1.0, "COMPARE plays at the rack's loudness (" + juce::String (rack - cmp, 2) + " dB apart, < 1)");

        // What COMPARE plays is the input, delayed by exactly the latency, with nothing done to it but the
        // speakers' protection (the 8 Hz high-pass): correlation with that
        std::vector<float> reference (inL.size());
        {
            const auto hp = enh::dsp::SvfCoeffs::make (sr, 8.0, 0.7071);
            enh::dsp::SvfState st;
            for (size_t i = 0; i < inL.size(); ++i)
                reference[i] = st.process (hp, inL[i]).high;
        }
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = n - (int) sr * 3; i < n; ++i)
        {
            const double x = reference[(size_t) (i - lat)], y = outL[(size_t) i];
            sxy += x * y; sxx += x * x; syy += y * y;
        }
        const double corr = sxy / std::sqrt (sxx * syy);
        if (std::getenv ("MASTER_DEBUG") != nullptr)
            for (int lag = lat - 3; lag <= lat + 3; ++lag)
            {
                double a = 0, b = 0, c2 = 0;
                for (int i = n - (int) sr * 3; i < n; ++i) { const double x = reference[(size_t) (i - lag)], y = outL[(size_t) i]; a += x * y; b += x * x; c2 += y * y; }
                std::printf ("    lag %d (latency %d): correlation %.6f\n", lag, lat, a / std::sqrt (b * c2));
            }
        check (corr > 0.9999, "COMPARE is the input with only the speakers' protection, in time with the rack (correlation " + juce::String (corr, 6) + ")");
        check (e.getLatencySamples() == lat, "COMPARE keeps the latency");

        // The switch is a 30 ms crossfade, not a jump
        float worst = 0.0f;
        for (int i = half - 64; i < half + (int) (sr * 0.05); ++i)
            worst = std::max (worst, std::abs (outL[(size_t) i] - outL[(size_t) (i - 1)]));
        float typical = 0.0f;
        for (int i = half - (int) sr; i < half - 64; ++i)
            typical = std::max (typical, std::abs (outL[(size_t) i] - outL[(size_t) (i - 1)]));
        check (worst <= typical * 1.2f, "switching COMPARE does not step the audio (" + juce::String (worst, 4) + " against "
                                        + juce::String (typical, 4) + " in the music)");
    }

    std::printf ("\n== MASTERING: STEREO (mid/side) ==\n");

    // Everything out but the unit under test, on material that is all side (L = -R) or all mid (L = R)
    auto only = [&] (auto setup)
    {
        return presetParameters (presetNamed ("TRANSPARENT (ALL OUT)"), [&] (auto& kv) { setup (kv); });
    };
    auto sideOnly = [&] (std::vector<float>& l, std::vector<float>& r) { for (size_t i = 0; i < l.size(); ++i) r[i] = -l[i]; };
    auto midOnly  = [&] (std::vector<float>& l, std::vector<float>& r) { for (size_t i = 0; i < l.size(); ++i) r[i] = l[i]; };

    // Each check compares two runs that differ only in the setting under test (the rest of the chain -
    // the subsonic filter, the output limiter - is in both).
    auto runWith = [&] (const std::vector<float>& l, const std::vector<float>& r, const EnhEngine::Parameters& q,
                        std::vector<float>& ol, std::vector<float>& orr)
    {
        EnhEngine e; e.prepare (sr, 256, 2);
        render (e, l, r, q, ol, orr);
    };

    // 1. The compressor on MID leaves pure side as if it were out (sample for sample); on L/R it works on
    //    it. Bursts - loud for 250 ms, quiet for 250 ms - so the adaptive compressor has something to do.
    {
        std::vector<float> l, r, outOff, outMid, outLr, dummy;
        noise (l, r, (int) (sr * 6.0), 1.0f, 9);
        for (size_t i = 0; i < l.size(); ++i)
            l[i] *= ((i / (size_t) (sr * 0.25)) % 2 == 0) ? 1.0f : 0.12f;
        sideOnly (l, r);
        auto with = [&] (bool active, int mode)
        {
            return only ([&] (auto& kv) { kv.tideActive = active; kv.tideMixPercent = 100.0f; kv.tideResponse = 8.0f;
                                          kv.methods[(size_t) enh::dsp::methods::tideStereo] = mode; });
        };
        runWith (l, r, with (false, 0), outOff, dummy);
        runWith (l, r, with (true, 1), outMid, dummy);
        runWith (l, r, with (true, 0), outLr, dummy);
        double midDiff = 0, lrDiff = 0, ref = 0;
        for (int i = (int) sr * 2; i < (int) l.size(); ++i)
        {
            midDiff = std::max (midDiff, (double) std::abs (outMid[(size_t) i] - outOff[(size_t) i]));
            lrDiff += std::pow ((double) outLr[(size_t) i] - outOff[(size_t) i], 2.0);
            ref += std::pow ((double) outOff[(size_t) i], 2.0);
        }
        const double lrDb = 10.0 * std::log10 (std::max (1e-20, lrDiff) / std::max (1e-20, ref));
        check (db (midDiff) < -80.0 && lrDb > -40.0, "ADAPTIVE COMPRESSOR on MID leaves the sides as if it were out (largest difference "
                                                     + juce::String (db (midDiff), 1) + " dBFS); on L/R it changes them ("
                                                     + juce::String (lrDb, 1) + " dB of difference)");
    }

    // 2. CHARACTER on SIDE leaves pure mid exactly as with CHARACTER out, in time (its kept part waits for
    //    its latency, and OUT is the same delay)
    {
        std::vector<float> l, r, outOff, outSide, dummy;
        noise (l, r, (int) (sr * 3.0), 0.7f, 13);
        midOnly (l, r);
        runWith (l, r, only ([&] (auto& kv) { kv.charActive = false; }), outOff, dummy);
        runWith (l, r, only ([&] (auto& kv) { kv.charActive = true; kv.charDrive = 10.0f;
                                              kv.methods[(size_t) enh::dsp::methods::charStereo] = 2; }), outSide, dummy);
        double worst = 0;
        for (int i = (int) sr; i < (int) l.size(); ++i)
            worst = std::max (worst, (double) std::abs (outSide[(size_t) i] - outOff[(size_t) i]));
        check (db (worst) < -80.0, "CHARACTER on SIDE: the middle passes untouched and in time (largest difference "
                                   + juce::String (db (worst), 1) + " dBFS against CHARACTER out)");
    }

    // 3. TONE & SPACE on MID keeps the width it makes: its reverb on a mono source still comes out stereo
    {
        std::vector<float> l, r;
        noise (l, r, (int) (sr * 6.0), 0.8f, 17);
        midOnly (l, r);
        auto q = only ([&] (auto& kv) { kv.seraphMode = 2; kv.space = 8.0f; kv.widthPercent = 150.0f; kv.methods[(size_t) enh::dsp::methods::seraphStereo] = 1; });
        EnhEngine e; e.prepare (sr, 256, 2);
        std::vector<float> ol, orr;
        render (e, l, r, q, ol, orr);
        double side = 0, mid = 0;
        for (int i = (int) sr * 3; i < (int) l.size(); ++i)
        {
            const double m = 0.5 * (ol[(size_t) i] + orr[(size_t) i]), s = 0.5 * (ol[(size_t) i] - orr[(size_t) i]);
            mid += m * m; side += s * s;
        }
        const double width = 10.0 * std::log10 (std::max (1e-20, side) / std::max (1e-20, mid));
        check (width > -40.0, "TONE & SPACE on MID: what it makes wide stays wide (side " + juce::String (width, 1) + " dB under mid)");
    }

    // 4. Every STEREO setting, every unit that has one, full rack on, odd blocks: finite, bounded, latency kept
    {
        std::vector<float> l, r;
        noise (l, r, (int) (sr * 2.0), 3.0f, 21);
        bool ok = true;
        EnhEngine ref; ref.prepare (sr, 256, 2);
        const int lat = ref.getLatencySamples();
        for (int mode = 0; mode < 3; ++mode)
        {
            auto q = presetParameters (presetNamed ("IMMERSIVE GAMES"), [&] (auto& kv)
            {
                kv.charActive = true;
                for (auto id : { enh::dsp::methods::tideStereo, enh::dsp::methods::levelerStereo, enh::dsp::methods::limiterStereo,
                                 enh::dsp::methods::balancerStereo, enh::dsp::methods::seraphStereo, enh::dsp::methods::charStereo })
                    kv.methods[(size_t) id] = mode;
            });
            EnhEngine e; e.prepare (sr, 1024, 2);
            juce::AudioBuffer<float> buf (2, 1024);
            juce::Random rng (3);
            for (size_t pos = 0; pos < l.size();)
            {
                const int m = std::min ((int) (l.size() - pos), 1 + rng.nextInt (1023));
                buf.setSize (2, m, false, false, true);
                for (int i = 0; i < m; ++i) { buf.setSample (0, i, l[pos + (size_t) i]); buf.setSample (1, i, r[pos + (size_t) i]); }
                e.process (buf, q);
                for (int i = 0; i < m; ++i)
                    ok = ok && std::isfinite (buf.getSample (0, i)) && std::abs (buf.getSample (0, i)) <= 1.0f && std::abs (buf.getSample (1, i)) <= 1.0f;
                pos += (size_t) m;
            }
            ok = ok && e.getLatencySamples() == lat;
        }
        check (ok, "every STEREO setting on every unit at once, random block sizes, hot noise: finite, under full scale, same latency");
    }
}

// Speaker and headset safety (3.6.4.1): EnhDspTests --mastering runs these too.
static void runSafetyTests (double sr)
{
    using namespace mastertest;
    std::printf ("\n== SAFETY ==\n");
    const auto base = presetParameters (presetNamed ("DEFAULT"));

    // 1. Something that is not a number comes in: silence, not a burst - and the rack comes back
    {
        EnhEngine e; e.prepare (sr, 256, 2);
        juce::AudioBuffer<float> buf (2, 256);
        juce::Random rng (1);
        bool finite = true, silentOnBad = true;
        float after = 0.0f;
        // The block where each bad sample reaches the output (the rack's delay may carry it into the next one)
        const int badOut1 = 100 + (17 + e.getLatencySamples()) / 256, badOut2 = 150 + (3 + e.getLatencySamples()) / 256;
        for (int blk = 0; blk < 400; ++blk)
        {
            for (int i = 0; i < 256; ++i) { const float v = 0.3f * (rng.nextFloat() - 0.5f); buf.setSample (0, i, v); buf.setSample (1, i, v); }
            if (blk == 100) buf.setSample (0, 17, std::numeric_limits<float>::quiet_NaN());
            if (blk == 150) buf.setSample (1, 3, std::numeric_limits<float>::infinity());
            e.process (buf, base);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                {
                    finite = finite && std::isfinite (buf.getSample (c, i));
                    if (blk == badOut1 || blk == badOut2) silentOnBad = silentOnBad && buf.getSample (c, i) == 0.0f;
                    if (blk > 350) after = std::max (after, std::abs (buf.getSample (c, i)));
                }
        }
        check (finite && silentOnBad && after > 0.01f, "a NaN or an infinity coming in: that block is silent, nothing non-finite leaves, the rack comes back (level "
                                                       + juce::String (after, 3) + ")");
    }

    // 2. DC coming in (or made inside) never reaches the speakers
    {
        std::vector<float> l, r, ol, orr;
        noise (l, r, (int) (sr * 4.0), 0.4f, 31);
        for (size_t i = 0; i < l.size(); ++i) { l[i] += 0.4f; r[i] += 0.4f; }
        EnhEngine e; e.prepare (sr, 256, 2);
        auto q = base; q.character.active = true; q.character.modelA = enh::dsp::Character::valve; q.character.drive = 10.0f;
        render (e, l, r, q, ol, orr);
        double dc = 0;
        for (int i = (int) sr * 2; i < (int) ol.size(); ++i) dc += ol[(size_t) i];
        dc /= (double) ((int) ol.size() - (int) sr * 2);
        check (db (std::abs (dc)) < -50.0, "DC of 0.4 in, VALVE at full DRIVE: DC out " + juce::String (db (std::abs (dc)), 1) + " dBFS (< -50)");
    }

    // 3. Starting: a fade-in, not a step (no pop)
    {
        EnhEngine e; e.prepare (sr, 256, 2);
        juce::AudioBuffer<float> buf (2, 256);
        for (int i = 0; i < 256; ++i) { buf.setSample (0, i, 0.5f); buf.setSample (1, i, 0.5f); }
        e.process (buf, base);
        check (std::abs (buf.getSample (0, 0)) < 0.01f, "the first sample after a start is faded in (" + juce::String (buf.getSample (0, 0), 4) + ")");
    }

    // 4. COMPARE learns a big lift on quiet material, then the input gets loud: still never over full scale
    {
        std::vector<float> l, r, ol, orr;
        noise (l, r, (int) (sr * 14.0), 0.02f, 41);
        for (size_t i = (size_t) (sr * 10.0); i < l.size(); ++i) { l[i] *= 60.0f; r[i] *= 60.0f; }   // +35 dB, hot
        EnhEngine e; e.prepare (sr, 256, 2);
        auto q = base; q.methods[(size_t) enh::dsp::methods::outputTarget] = 3;   // -14 LUFS: a big lift on the quiet part
        render (e, l, r, q, ol, orr, [&] (int pos, EnhEngine::Parameters& pp) { pp.compare = pos >= (int) (sr * 8.0); });
        float peak = 0.0f;
        for (size_t i = 0; i < ol.size(); ++i) peak = std::max ({ peak, std::abs (ol[i]), std::abs (orr[i]) });
        check (peak <= 1.0f, "COMPARE after a lift, then a loud input: peak " + juce::String (peak, 4) + " (never over full scale)");
    }

    // 5. Everything at its most, full-scale noise, COMPARE flicking on and off, random blocks
    {
        std::vector<float> l, r;
        noise (l, r, (int) (sr * 4.0), 6.0f, 51);
        auto q = presetParameters (presetNamed ("DEEP SUB: SUBMARINE"), [&] (auto& kv)
        {
            kv.charActive = true; kv.charDrive = 10.0f; kv.charModelA = 6.0f; kv.charModelB = 4.0f; kv.charBlend = 50.0f;
            kv.methods[(size_t) enh::dsp::methods::outputTarget] = 3;
            kv.levelDb = 12.0f;
        });
        EnhEngine e; e.prepare (sr, 1024, 2);
        juce::AudioBuffer<float> buf (2, 1024);
        juce::Random rng (9);
        float peak = 0.0f;
        bool finite = true;
        for (size_t pos = 0; pos < l.size();)
        {
            const int m = std::min ((int) (l.size() - pos), 1 + rng.nextInt (1023));
            buf.setSize (2, m, false, false, true);
            for (int i = 0; i < m; ++i) { buf.setSample (0, i, l[pos + (size_t) i]); buf.setSample (1, i, r[pos + (size_t) i]); }
            q.compare = rng.nextInt (6) == 0;
            e.process (buf, q);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < m; ++i) { finite = finite && std::isfinite (buf.getSample (c, i)); peak = std::max (peak, std::abs (buf.getSample (c, i))); }
            pos += (size_t) m;
        }
        check (finite && peak <= 1.0f, "everything at its most, +12 dB LEVEL, full-scale noise, COMPARE flicking: peak " + juce::String (peak, 4)
                                       + ", finite");
    }

    // 7. EAR GUARD: quiet for a while (the rack lifting it), then a sudden +35 dB - the jump is held to
    //    about 12 dB over how loud it had been; loud material that stays loud is never touched
    {
        // Loudness as BS.1770 measures it (K-weighted), over a window
        enh::dsp::BiquadCoeffs kPre, kRlb;
        enh::dsp::kWeighting (sr, kPre, kRlb);
        auto kWeighted = [&] (const std::vector<float>& x)
        {
            std::vector<float> y (x.size());
            enh::dsp::BiquadState a, b;
            for (size_t i = 0; i < x.size(); ++i) y[i] = b.process (kRlb, a.process (kPre, x[i]));
            return y;
        };
        auto windowDb = [] (const std::vector<float>& l, const std::vector<float>& r, int from, int len)
        {
            double e = 0.0;
            for (int i = from; i < from + len && i < (int) l.size(); ++i)
                e += (double) l[(size_t) i] * l[(size_t) i] + (double) r[(size_t) i] * r[(size_t) i];
            return 10.0 * std::log10 (e / len + 1.0e-20) - 0.691;
        };
        std::vector<float> l, r, ol, orr;
        noise (l, r, (int) (sr * 12.0), 0.01f, 61);                                   // about -45 dBFS
        for (size_t i = (size_t) (sr * 8.0); i < l.size(); ++i) { l[i] *= 56.0f; r[i] *= 56.0f; }   // +35 dB
        EnhEngine e; e.prepare (sr, 256, 2);
        float guardMost = 0.0f;
        float usualBefore = 0.0f;
        render (e, l, r, base, ol, orr, [&] (int at, EnhEngine::Parameters&)
        {
            guardMost = std::max (guardMost, e.getMeters().earGuardDb.load());
            if (at < (int) (sr * 8.0)) usualBefore = e.getMeters().earGuardUsualLufs.load();
            if (std::getenv ("EAR_TRACE") != nullptr && at % (int) (sr * 0.25) < 256)
                std::printf ("    t %5.2f usual %6.1f guard %5.1f\n", at / sr, e.getMeters().earGuardUsualLufs.load(), e.getMeters().earGuardDb.load());
        });
        const int w = (int) (0.4 * sr);   // momentary loudness: 400 ms, as BS.1770 measures it
        ol = kWeighted (ol);
        orr = kWeighted (orr);
        const double before = windowDb (ol, orr, (int) (sr * 6.0), (int) (sr * 2.0));
        double after = -200.0;
        for (int at = (int) (sr * 8.0); at + w < (int) (sr * 10.0); at += w / 4)   // the first two seconds of it
            after = std::max (after, windowDb (ol, orr, at, w));
        std::printf ("  EAR GUARD: before the jump %.1f LUFS, loudest 400 ms after it %.1f LUFS (+%.1f), guard up to %.1f dB\n",
                     before, after, after - before, guardMost);
        check (after - before < 14.0, "a sudden +35 dB after a quiet stretch comes out at most ~12 dB louder (+" + juce::String (after - before, 1) + " dB, < 14)");

        std::vector<float> ll, rr, ol2, or2;
        noise (ll, rr, (int) (sr * 10.0), 0.3f, 71);                                  // loud, and it stays loud
        EnhEngine e2; e2.prepare (sr, 256, 2);
        float most = 0.0f;
        render (e2, ll, rr, base, ol2, or2, [&] (int at, EnhEngine::Parameters&) { if (at > (int) sr) most = std::max (most, e2.getMeters().earGuardDb.load()); });
        check (most < 0.5f, "loud material that stays loud is left alone (guard " + juce::String (most, 2) + " dB, < 0.5)");

        // Music, a quiet break (the rack still running), and the music back at the level it had: not a jump
        std::vector<float> bl, br, ob, obr;
        noise (bl, br, (int) (sr * 16.0), 0.2f, 91);
        for (size_t i = (size_t) (sr * 6.0); i < (size_t) (sr * 10.0); ++i) { bl[i] *= 0.003f; br[i] *= 0.003f; }   // -50 dB for 4 s
        EnhEngine eb; eb.prepare (sr, 256, 2);
        float backMost = 0.0f;
        render (eb, bl, br, base, ob, obr, [&] (int at, EnhEngine::Parameters&) { if (at > (int) (sr * 10.0)) backMost = std::max (backMost, eb.getMeters().earGuardDb.load()); });
        check (backMost < 0.5f, "music back after a 4 s quiet break, at the level it had: not held (guard " + juce::String (backMost, 2) + " dB, < 0.5)");

        std::vector<float> ml, mr, ol3, or3;
        noise (ml, mr, (int) (sr * 12.0), 0.1f, 81);
        for (size_t i = 0; i < ml.size(); ++i)   // music-like swells: +-6 dB every couple of seconds
        {
            const float g = std::pow (10.0f, 6.0f * std::sin (6.2831853f * (float) i / (float) (2.2 * sr)) / 20.0f);
            ml[i] *= g; mr[i] *= g;
        }
        EnhEngine e3; e3.prepare (sr, 256, 2);
        float musicMost = 0.0f;
        render (e3, ml, mr, base, ol3, or3, [&] (int at, EnhEngine::Parameters&) { if (at > (int) sr) musicMost = std::max (musicMost, e3.getMeters().earGuardDb.load()); });
        check (musicMost < 0.5f, "ordinary ups and downs (+-6 dB) are left alone (guard " + juce::String (musicMost, 2) + " dB, < 0.5)");
    }

    // 6. GRIT off: CHARACTER at full DRIVE stays clean in every model
    for (int m = 0; m < enh::dsp::Character::numModels; ++m)
    {
        const float thd = chartest::sineWith (m, 10.0f, 1000.0f, -6.0f, false).thd;
        check (thd < 2.0f, juce::String (enh::dsp::Character::names[(size_t) m]) + ": GRIT off, DRIVE 10, 1 kHz at -6 dBFS - THD "
                           + juce::String (thd, 2) + " % (< 2 %)");
    }
}

// Knob fuzz: every automatable parameter and every processing method moved at random, block after block,
// as a user playing with the rack would - and as fast as automation can. Reports the first block where
// anything that is not a number was made inside the rack (the output guard tripped), with what had just
// changed.   EnhDspTests --fuzz [seconds] [seed]
static void runKnobFuzz (double sr, double seconds, int seed)
{
    using namespace mastertest;
    std::printf ("\n== KNOB FUZZ (%g s, seed %d) ==\n", seconds, seed);

    std::map<juce::String, float> vals;
    const auto base = presetNamed ("DEFAULT");
    std::vector<const pad::params::Spec*> knobs;
    for (auto& spec : pad::params::allSpecs())
    {
        vals[spec.id] = pad::presets::valueFor (base, spec);
        if (spec.automatable && spec.id != pad::params::id::loudnessReset)
            knobs.push_back (&spec);
    }
    std::array<int, enh::dsp::methods::numMethodIds> methods {};

    std::vector<float> l, r;
    noise (l, r, (int) (sr * 4.0), 0.5f, (juce::uint32) seed);

    EnhEngine e;
    e.prepare (sr, 512, 2);
    juce::AudioBuffer<float> buf (2, 512);
    juce::Random rng (seed);
    std::vector<juce::String> history;
    int trips = 0, firstTrip = -1;
    const int blocks = (int) (seconds * sr / 512.0);
    size_t pos = 0;

    for (int blk = 0; blk < blocks; ++blk)
    {
        // One to three knobs move this block (sometimes a switch or a method)
        for (int k = 0, count = 1 + rng.nextInt (3); k < count; ++k)
        {
            if (rng.nextInt (8) == 0)
            {
                int unitStages = 0;
                for (int unit : enh::dsp::methods::unitsInRackOrder) unitStages += enh::dsp::methods::stagesForUnit (unit).count;
                int pick = rng.nextInt (unitStages);
                for (int unit : enh::dsp::methods::unitsInRackOrder)
                {
                    const auto list = enh::dsp::methods::stagesForUnit (unit);
                    if (pick < list.count)
                    {
                        const auto& st = list.stages[pick];
                        if (st.id >= 0)
                        {
                            methods[(size_t) st.id] = rng.nextInt (st.numMethods);
                            history.push_back ("method " + juce::String (st.param.data(), st.param.size()) + " = " + juce::String (methods[(size_t) st.id]));
                        }
                        break;
                    }
                    pick -= list.count;
                }
                continue;
            }
            const auto* spec = knobs[(size_t) rng.nextInt ((int) knobs.size())];
            float v = spec->minValue + rng.nextFloat() * (spec->maxValue - spec->minValue);
            if (spec->kind != pad::params::Kind::continuous)
                v = (float) juce::roundToInt (v);
            vals[spec->id] = v;
            history.push_back (spec->id + " = " + juce::String (v, 2));
        }
        if (history.size() > 12)
            history.erase (history.begin(), history.end() - 12);

        pad::presets::Preset cur { "fuzz", "", {} };
        for (auto& [pid, v] : vals) cur.values.push_back ({ pid, v });
        const auto p = presetParameters (cur, [&] (auto& kv) { kv.methods = methods; });

        for (int i = 0; i < 512; ++i, ++pos)
        {
            if (pos >= l.size()) pos = 0;
            buf.setSample (0, i, l[pos]); buf.setSample (1, i, r[pos]);
        }
        e.process (buf, p);

        if (e.getMeters().protectionTripped.load())
        {
            ++trips;
            if (firstTrip < 0)
            {
                firstTrip = blk;
                std::printf ("  first non-number at block %d (%.2f s). Changed just before, oldest first:\n", blk, blk * 512.0 / sr);
                for (auto& h : history) std::printf ("    %s\n", h.toRawUTF8());
            }
        }
    }

    check (trips == 0, "knobs moved at random for " + juce::String (seconds) + " s: no non-number made inside the rack (" + juce::String (trips) + " blocks tripped the guard)");
}

// Zipper noise (3.6.5.1): every continuous knob jumped from one end to the other in a single block while a
// steady 60 Hz + 1 kHz tone plays. A knob that steps the audio makes a burst of high frequencies (a click);
// one that glides does not. Measured above 9 kHz, in the 30 ms after the jump, against the steadier of the
// two settings.   EnhDspTests --zipper
static void runZipperTests (double sr)
{
    using namespace mastertest;
    std::printf ("\n== ZIPPER NOISE: every knob, end to end in one block ==\n");

    const int n = (int) (sr * 2.0), jumpAt = (int) (sr * 1.0);
    std::vector<float> l ((size_t) n), r ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        // -18 dBFS peak together, so that even LEVEL's +12 dB stays under full scale: this is about clicks,
        // not about limiting an over (which the limiters rightly do fast)
        l[(size_t) i] = r[(size_t) i] = (float) (0.085 * std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * t)
                                               + 0.04 * std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * t));
    }

    const auto hpCoeffs = enh::dsp::SvfCoeffs::make (sr, 9000.0, 0.7071);
    auto hfLevel = [&] (const std::vector<float>& x, int from, int to)
    {
        enh::dsp::SvfState a, b;
        double peak = 0.0;
        for (int i = from - 2000; i < to; ++i)
        {
            const float y = b.process (hpCoeffs, a.process (hpCoeffs, x[(size_t) i]).high).high;
            if (i >= from) peak = std::max (peak, (double) std::abs (y));
        }
        return peak;
    };

    int worstCount = 0;
    for (auto& spec : pad::params::allSpecs())
    {
        if (spec.kind != pad::params::Kind::continuous || ! spec.automatable)
            continue;
        // Diagnosis: ZIP_PARAM=<id> one knob only; ZIP_PRESET=<name>; ZIP_SET="id=v;id=v" forced
        if (const auto only = juce::SystemStats::getEnvironmentVariable ("ZIP_PARAM", {}); only.isNotEmpty() && only != spec.id)
            continue;

        for (int dir = 0; dir < 2; ++dir)
        {
            const float from = dir == 0 ? spec.minValue : spec.maxValue, to = dir == 0 ? spec.maxValue : spec.minValue;
            auto preset = presetNamed (juce::SystemStats::getEnvironmentVariable ("ZIP_PRESET", "IMMERSIVE GAMES").toRawUTF8());
            for (auto& kv : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("ZIP_SET", {}), ";", {}))
                if (kv.containsChar ('='))
                    preset.values.push_back ({ kv.upToFirstOccurrenceOf ("=", false, false), kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
            auto withValue = [&] (float v)
            {
                pad::presets::Preset p = preset;
                p.values.push_back ({ spec.id, v });
                if (spec.id.startsWith ("char")) p.values.push_back ({ pad::params::id::charActive, 1.0f });
                return presetParameters (p);
            };
            const auto before = withValue (from), after = withValue (to);

            EnhEngine e; e.prepare (sr, 256, 2);
            std::vector<float> ol, orr;
            render (e, l, r, before, ol, orr, [&] (int pos, EnhEngine::Parameters& q) { q = pos >= jumpAt ? after : before; });

            const double jump = hfLevel (ol, jumpAt, jumpAt + (int) (sr * 0.030));
            const double steadyBefore = hfLevel (ol, jumpAt - (int) (sr * 0.2), jumpAt - 256);
            const double steadyAfter = hfLevel (ol, n - (int) (sr * 0.3), n);
            const double over = db (jump) - db (std::max ({ steadyBefore, steadyAfter, 1.0e-4 }));
            if (std::getenv ("ZIP_DUMP") != nullptr)
            {
                // Where the burst is: the largest sample-to-sample step in the 30 ms after the jump, and around it
                int at = jumpAt; float big = 0.0f;
                for (int i = jumpAt; i < jumpAt + (int) (sr * 0.030); ++i)
                    if (std::abs (ol[(size_t) i] - ol[(size_t) (i - 1)]) > big) { big = std::abs (ol[(size_t) i] - ol[(size_t) (i - 1)]); at = i; }
                std::printf ("    %s %s->%s: biggest step %.5f at +%d samples; around it:", spec.id.toRawUTF8(), juce::String (from).toRawUTF8(),
                             juce::String (to).toRawUTF8(), big, at - jumpAt);
                for (int i = at - 4; i <= at + 4; ++i) std::printf (" %.4f", ol[(size_t) i]);
                std::printf ("\n");
            }
            if (over > 12.0 && db (jump) > -50.0)   // under -50 dBFS, beneath a -18 dBFS tone, it is masked
            {
                ++worstCount;
                std::printf ("  %-18s %s -> %s: a burst %+.1f dB over the steady sound (%.1f dBFS)\n", spec.id.toRawUTF8(),
                             juce::String (from, 1).toRawUTF8(), juce::String (to, 1).toRawUTF8(), over, db (jump));
            }
        }
    }
    check (worstCount == 0, "no knob clicks when it jumps end to end in one block (" + juce::String (worstCount) + " clicked)");
}

// Aliasing through the rack's own non-linear stages (3.6.5.1): a 7 kHz tone, loud, through one stage at a
// time pushed hard. Harmonics above 24 kHz must be filtered, not folded back into the audio band.
//   EnhDspTests --alias
static void runAliasTests (double sr)
{
    using namespace mastertest;
    std::printf ("\n== ALIASING: the rack's non-linear stages, 7 kHz at -6 dBFS ==\n");

    auto measure = [&] (const EnhEngine::Parameters& q)
    {
        const int n = (int) (sr * 3.0), skip = (int) (sr * 2.0);
        std::vector<float> l ((size_t) n), r, ol, orr;
        for (int i = 0; i < n; ++i) l[(size_t) i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 7000.0 * i / sr);
        r = l;
        EnhEngine e; e.prepare (sr, 256, 2);
        render (e, l, r, q, ol, orr);
        std::vector<double> x (ol.begin() + skip, ol.end());
        const int N = (int) x.size();
        auto fit = [&] (double f)
        {
            double sc = 0, cc = 0;
            for (int i = 0; i < N; ++i) { const double ph = 2.0 * juce::MathConstants<double>::pi * f * (skip + i) / sr; sc += x[(size_t) i] * std::sin (ph); cc += x[(size_t) i] * std::cos (ph); }
            const double a = 2.0 * sc / N, b = 2.0 * cc / N;
            for (int i = 0; i < N; ++i) { const double ph = 2.0 * juce::MathConstants<double>::pi * f * (skip + i) / sr; x[(size_t) i] -= a * std::sin (ph) + b * std::cos (ph); }
            return std::sqrt (a * a + b * b);
        };
        // Aliases land at known places: harmonic h of 7 kHz, above half the sample rate, folds back to
        // |h x 7 kHz - k x 48 kHz|. Measured there only, so a stage whose gain moves (MATCH, SMOOTH) is not
        // mistaken for one that aliases.
        const double fund = fit (7000.0);
        double aliasPower = 0.0;
        for (int h = 4; h <= 15; ++h)
        {
            double f = std::fmod (h * 7000.0, sr);
            if (f > sr * 0.5) f = sr - f;
            if (std::abs (f - 7000.0) < 200.0 || std::abs (f - 14000.0) < 200.0 || std::abs (f - 21000.0) < 200.0)
                continue;
            const double a = fit (f);
            aliasPower += a * a;
        }
        return db (std::sqrt (aliasPower) / std::max (1e-12, fund));
    };

    struct Case { const char* name; std::function<void (enh::dsp::KnobValues&)> set; };
    const std::vector<Case> cases {
        { "ENHANCER (ADD 10, STRENGTH 5)", [] (auto& k) { k.enhStrength = 5.0f; k.clarityAddMode = true; k.clarityAdd = 10.0f; } },
        { "TONE: WARMTH 10, STRENGTH 5",   [] (auto& k) { k.seraphMode = 1; k.warmth = 10.0f; k.seraphStrength = 5.0f; } },
        { "TONE: TAPE, STRENGTH 5",        [] (auto& k) { k.seraphMode = 1; k.tape = true; k.seraphStrength = 5.0f; } },
        { "TONE: AIR 10, STRENGTH 5",      [] (auto& k) { k.seraphMode = 1; k.air = 10.0f; k.seraphStrength = 5.0f; } },
        { "DEEP SUB: PRESSURE 10",         [] (auto& k) { k.deepActive = true; k.deepPressure = 10.0f; } },
        { "CHARACTER: VALVE, DRIVE 10",    [] (auto& k) { k.charActive = true; k.charModelA = 6.0f; k.charDrive = 10.0f; } },
    };
    if (std::getenv ("ALIAS_PRESETS") != nullptr)
        for (auto& pr : pad::presets::factory())
            std::printf ("  preset %-26s aliases %.1f dB under the tone\n", pr.name.toRawUTF8(), measure (presetParameters (pr)));

    for (auto& c : cases)
    {
        const auto q = presetParameters (presetNamed ("TRANSPARENT (ALL OUT)"), c.set);
        const double other = measure (q);
        check (other < -60.0, juce::String (c.name) + ": aliases " + juce::String (other, 1) + " dB under the tone (< -60)");
    }
}

// TONE at STRENGTH 0 against its input, with the oversampling's delay taken out: at a few lags, and below
// 4 kHz only (phase shifts of an oversampler live near the top).   EnhDspTests --str0
static void runStrengthZeroCheck (double sr)
{
    using enh::dsp::Seraph;
    const auto n = (size_t) (6.0 * sr);
    std::vector<float> in (n);
    juce::Random rnd (22);
    float b0 = 0, b1 = 0;
    for (auto& x : in) { const float w = rnd.nextFloat() * 2 - 1; b0 = 0.97f * b0 + 0.3f * w; b1 = 0.5f * b1 + w; x = 0.08f * (b0 + 0.3f * b1); }

    Seraph unit;
    unit.prepare (sr);
    Seraph::Settings st;
    st.mode = Seraph::heaven;
    st.silk.strength = st.halo.strength = 0.0f;
    st.silk.autoGain = false; st.silk.tape = true;
    auto l = in, r = in;
    for (size_t pos = 0; pos < n; pos += 256)
    {
        float* chp[2] { l.data() + pos, r.data() + pos };
        unit.process (chp, 2, (int) std::min ((size_t) 256, n - pos), st);
    }
    const int lat = unit.getLatencySamples();
    auto rms = [&] (const std::vector<float>& v) { double s = 0; for (size_t i = n / 2; i < n; ++i) s += (double) v[i] * v[i]; return 10.0 * std::log10 (s / (n / 2) + 1e-30); };
    const double ref = rms (in);
    for (int lag = lat - 2; lag <= lat + 2; ++lag)
    {
        std::vector<float> d (n, 0.0f);
        for (size_t i = (size_t) std::max (0, lag); i < n; ++i) d[i] = l[i] - in[i - (size_t) lag];
        std::printf ("  lag %d (latency %d): difference %.1f dB under the input\n", lag, lat, ref - rms (d));
    }
    // Below 4 kHz
    const auto lp = enh::dsp::SvfCoeffs::make (sr, 4000.0, 0.7071);
    enh::dsp::SvfState a1, a2, c1, c2;
    std::vector<float> dl (n, 0.0f), inl (n, 0.0f);
    for (size_t i = 0; i < n; ++i)
    {
        const float o = a2.process (lp, a1.process (lp, l[i]).low).low;
        const float x = i >= (size_t) lat ? in[i - (size_t) lat] : 0.0f;
        const float xi = c2.process (lp, c1.process (lp, x).low).low;
        dl[i] = o - xi; inl[i] = xi;
    }
    std::printf ("  below 4 kHz: difference %.1f dB under the input\n", rms (inl) - rms (dl));
    {
        const auto lp18 = enh::dsp::SvfCoeffs::make (sr, 18000.0, 0.7071);
        enh::dsp::SvfState e1, e2, f1, f2;
        for (size_t i = 0; i < n; ++i)
        {
            const float o = e2.process (lp18, e1.process (lp18, l[i]).low).low;
            const float x = i >= (size_t) lat ? in[i - (size_t) lat] : 0.0f;
            const float xi = f2.process (lp18, f1.process (lp18, x).low).low;
            dl[i] = o - xi; inl[i] = xi;
        }
        std::printf ("  below 18 kHz: difference %.1f dB under the input\n", rms (inl) - rms (dl));
    }
}
