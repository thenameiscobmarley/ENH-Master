#include "Seraph.h"

namespace enh::dsp
{
    namespace
    {
        /** Envelope-normalised harmonic generator (Chebyshev T2/T3) on a moving band: the same
            relative amount of harmonics for quiet and loud material. */
        template <typename Exciter>
        inline float excite (Exciter& e, const SvfCoeffs& band, const SvfCoeffs& post, const SvfCoeffs& top,
                             float w2, float w3, float in, float attack, float release) noexcept
        {
            const float x = e.band2.process (band, e.band1.process (band, in).band).band;
            const float m = std::abs (x);
            e.env = (m > e.env ? attack : release) * (e.env - m) + m;
            const float amp = e.env + 1.0e-6f;
            const float v = x / amp;
            const float u = v / (1.0f + 0.2f * std::abs (v));
            const float h = (w2 * (2.0f * u * u - 1.0f) + w3 * (4.0f * u * u - 3.0f) * u) * amp;
            const float hp = e.post2.process (post, e.post1.process (post, h).high).high;
            return e.top.process (top, hp).low * (e.env / (e.env + 1.0e-4f));
        }

        inline float onePoleHz (double hz, double sr) noexcept
        {
            return (float) std::exp (-2.0 * pi * std::min (hz, 0.45 * sr) / sr);
        }
    }

    //==============================================================================
    // SILK
    //==============================================================================
    void SilkStage::prepare (double sampleRate)
    {
        sr = sampleRate;
        controlInterval = std::max (8, (int) std::lround (sr / 1500.0));
        const double controlRate = sr / controlInterval;

        for (int k = 0; k < numBands; ++k)
        {
            const auto i = (size_t) k;
            bandHz[i] = (float) (150.0 * std::pow (16000.0 / 150.0, k / (double) (numBands - 1)));
            detect[i] = BiquadCoeffs::bandPass (sr, bandHz[i], 4.0);
            designers[i].setup (sr, bandHz[i], 4.0);
        }

        fastAtt = onePole (0.0005, sr);
        fastRel = onePole (0.030, sr);
        slowK   = onePole (0.300, sr);
        longK   = onePole (3.0, controlRate);
        cutAtt  = onePole (0.003, controlRate);
        cutRel  = onePole (0.060, controlRate);

        airBand  = SvfCoeffs::make (sr, 5500.0, 0.9);
        airPost  = SvfCoeffs::make (sr, 9500.0, 0.7071);
        airTop   = SvfCoeffs::make (sr, std::min (19000.0, 0.45 * sr), 0.7071);
        warmBand = SvfCoeffs::make (sr, 320.0, 0.6);
        triSplitCoeffs = SvfCoeffs::make (sr, 120.0, 0.7071);
        detectHpCoeffs = BiquadCoeffs::highPass (sr, 120.0, 0.7071);

        // SUB
        subLowCoeffs = SvfCoeffs::make (sr, 120.0, 0.7071);
        glowBandCoeffs = SvfCoeffs::make (sr, 180.0, 0.8);
        bassLpCoeffs = BiquadCoeffs::lowPass (sr, 120.0, 0.7071);
        bassFastAtt = onePole (0.010, sr);
        bassFastRel = onePole (0.150, sr);
        bassSlowK = onePole (1.2, sr);
        bassK = onePole (0.30, sr);
        bloomLpK = (float) (1.0 - std::exp (-2.0 * pi * 140.0 / sr));
        bloomLen = { (int) (0.071 * sr), (int) (0.097 * sr) };
        for (size_t d = 0; d < 2; ++d)
            bloomLine[d].assign ((size_t) bloomLen[d] + 1, 0.0f);
        triEnvAtt = onePole (0.003, sr);
        triEnvRel = onePole (0.120, sr);
        warmPost = SvfCoeffs::make (sr, 450.0, 0.7071);
        warmTop  = SvfCoeffs::make (sr, 3000.0, 0.7071);
        tapePre  = BiquadCoeffs::highShelf (sr, 3500.0, 0.7071, 5.0);
        tapePost = BiquadCoeffs::highShelf (sr, 3500.0, 0.7071, -5.0);

        envAtt = onePole (0.001, sr);
        envRel = onePole (0.050, sr);
        dcCoeff = (float) std::exp (-2.0 * pi * 6.0 / sr);

        kHp = BiquadCoeffs::highPass (sr, 60.0, 0.7071);
        kShelf = BiquadCoeffs::highShelf (sr, 1500.0, 0.7071, 4.0);
        msCoeff = onePole (0.4, sr);
        blendCoeff = onePole (0.020, sr);
        meterSmoothing = onePole (0.12, controlRate);
        reset();
    }

    void SilkStage::reset()
    {
        for (auto& s : detectState) s.reset();
        detectHp1.reset();
        detectHp2.reset();
        bassLp1.reset();
        bassLp2.reset();
        bassPower = bassFast = bassSlow = bloomIn = 0.0f;
        subShelfDb = 1000.0f;
        glowMix = bloomMix = 0.0f;
        for (auto& l : bloomLine) std::fill (l.begin(), l.end(), 0.0f);
        bloomPos = {};
        bloomLp = {};
        fast.fill (0.0f); slow.fill (0.0f); longTerm.fill (0.0f);
        cutDb.fill (0.0f); designedDb.fill (0.0f); onsetHold.fill (0);
        for (int k = 0; k < numBands; ++k)
            dipCoeffs[(size_t) k] = designers[(size_t) k].make (0.0f);

        for (auto& c : ch) c = Channel {};
        airShelf = BiquadCoeffs::highShelf (sr, 9500.0, 0.55, 0.0);
        bodyPeak = BiquadCoeffs::peaking (sr, 180.0, 0.8, 0.0);
        airShelfDb = bodyDb = 0.0f;
        airMix = warmMix = 0.0f;
        triodeK = 0.6f;

        inHp.reset(); inShelf.reset(); outHp.reset(); outShelf.reset();
        inMs = outMs = 0.0f;
        autoDb = 0.0f;
        outGain = 1.0f;
        smoothingDb = 0.0f;
        blend = 0.0f;
        toTick = 0;
        for (auto* m : { &smoothMeter, &airMeter, &warmthMeter, &bodyMeter, &tapeMeter })
            m->clear();
    }

    void SilkStage::controlTick (const Settings& s) noexcept
    {
        const float dt = (float) controlInterval / (float) sr;
        const float strength = std::clamp (s.strength, 0.0f, 5.0f);
        const float thr = std::max (0.8f, 7.0f - 0.45f * s.smooth);
        const float maxCut = std::min (24.0f, 1.6f * s.smooth);
        const int holdTicks = std::max (1, (int) std::lround (0.015 * sr / controlInterval));

        std::array<float, numBands> fastDb {}, slowDb {};
        for (int k = 0; k < numBands; ++k)
        {
            fastDb[(size_t) k] = powerToDb (fast[(size_t) k]);
            slowDb[(size_t) k] = powerToDb (slow[(size_t) k]);
            longTerm[(size_t) k] += (fast[(size_t) k] - longTerm[(size_t) k]) * (1.0f - longK);
        }

        float deepest = 0.0f;
        for (int k = 0; k < numBands; ++k)
        {
            const auto i = (size_t) k;
            const bool active = bandHz[i] < 0.43 * sr;
            float target = 0.0f;

            if (active && s.smooth > 0.01f && fastDb[i] > -75.0f)
            {
                // The band's spectral neighbourhood right now (triangular weights, self excluded)
                float sum = 0.0f, weight = 0.0f;
                for (int j = std::max (0, k - 5); j <= std::min (numBands - 1, k + 5); ++j)
                {
                    // Skip the band and its direct neighbours: they hear the resonance's own skirts
                    if (std::abs (j - k) < 2 || bandHz[(size_t) j] >= 0.43 * sr) continue;
                    const float w = (float) (6 - std::abs (j - k));
                    sum += fastDb[(size_t) j] * w;
                    weight += w;
                }
                const float excess = weight > 0.0f ? fastDb[i] - sum / weight : 0.0f;

                float bandMax = maxCut;
                if (s.protect)
                {
                    // Keep attacks (footsteps, plucks, consonants) untouched for their first 15 ms:
                    // an existing dip lets go at once rather than eating the transient
                    if (fastDb[i] - slowDb[i] > 5.0f && onsetHold[i] == 0)
                    {
                        onsetHold[i] = holdTicks;
                        cutDb[i] = 0.0f;
                        // the neighbours share the transient's skirts: release them too
                        if (k > 0)            cutDb[i - 1] = 0.0f;
                        if (k + 1 < numBands) cutDb[i + 1] = 0.0f;
                    }
                    if (bandHz[i] >= 1000.0f && bandHz[i] <= 4500.0f)
                        bandMax *= 0.5f;
                }

                target = std::max (-30.0f, -std::min (bandMax, 1.0f * softRamp (excess - thr, 2.0f)) * strength);

                if (onsetHold[i] > 0)
                {
                    --onsetHold[i];
                    target = 0.0f;   // let the attack through untouched
                }
            }

            cutDb[i] = target + (cutDb[i] - target) * (target < cutDb[i] ? cutAtt : cutRel);
            if (std::abs (cutDb[i]) < 0.02f && designedDb[i] != 0.0f)
            {
                // Fully released: take the dip out of the signal path
                cutDb[i] = 0.0f;
                designedDb[i] = 0.0f;
                dipCoeffs[i] = designers[i].make (0.0f);
                for (auto& c : ch)
                    c.dip[i].reset();
            }
            else if (std::abs (cutDb[i] - designedDb[i]) > 0.03f)
            {
                dipCoeffs[i] = designers[i].make (cutDb[i]);
                designedDb[i] = cutDb[i];
            }
            deepest = std::min (deepest, cutDb[i]);
        }
        smoothingDb = -deepest;

        // Long-term balance of the source: how dull (air) and how thin (body) it is
        auto regionDb = [this] (float lo, float hi)
        {
            double p = 0.0; int n = 0;
            for (int k = 0; k < numBands; ++k)
                if (bandHz[(size_t) k] >= lo && bandHz[(size_t) k] <= hi && bandHz[(size_t) k] < 0.43 * sr)
                {
                    p += longTerm[(size_t) k];
                    ++n;
                }
            return n > 0 ? (float) (10.0 * std::log10 (p / n + 1.0e-20)) : -200.0f;
        };

        const float presence = regionDb (2000.0f, 5000.0f), airRegion = regionDb (9000.0f, 16000.0f);
        const float deficit = airRegion > -150.0f ? saturate01 ((-3.0f - (airRegion - presence)) / 10.0f) : 0.0f;
        const float harsh = saturate01 (smoothingDb / 6.0f);
        const float quietGate = saturate01 ((presence + 80.0f) / 10.0f);

        const float shelf = std::min (18.0f, s.air * 0.7f * (0.25f + 0.75f * deficit) * (1.0f - 0.5f * harsh) * quietGate * strength);
        if (std::abs (shelf - airShelfDb) > 0.05f)
        {
            airShelf = BiquadCoeffs::highShelf (sr, std::min (9500.0, 0.4 * sr), 0.55, shelf);
            airShelfDb = shelf;
        }
        airMix = std::min (1.5f, s.air * 0.05f * (0.25f + 0.75f * deficit) * (1.0f - 0.5f * harsh) * strength);

        const float bodyRatio = regionDb (150.0f, 300.0f) - regionDb (500.0f, 2000.0f);
        const float thin = saturate01 ((-3.0f - bodyRatio) / 9.0f);
        const float body = std::min (18.0f, s.body * 0.6f * (0.35f + 0.65f * thin) * quietGate * strength);

        // SUB: everything backs off as the bass gets loud (from -24 dBFS of bass to -12, down to 20 %)
        {
            const float loud = saturate01 ((powerToDb (bassPower) + 24.0f) / 12.0f);
            const float amount = std::clamp (s.sub, 0.0f, 30.0f) * strength * (1.0f - 0.8f * loud);
            const float shelfDb = std::min (8.0f, amount * 0.6f);
            if (std::abs (shelfDb - subShelfDb) > 0.05f)
            {
                subShelf = BiquadCoeffs::lowShelf (sr, 80.0, 0.7, shelfDb);
                subShelfDb = shelfDb;
            }
            glowMix = std::min (0.6f, amount * 0.03f) * (1.0f - 0.6f * loud);   // loud bass is already felt
            bloomMix = std::min (0.5f, amount * 0.025f);
        }
        if (std::abs (body - bodyDb) > 0.05f)
        {
            bodyPeak = BiquadCoeffs::peaking (sr, 180.0, 0.8, body);
            bodyDb = body;
        }

        for (auto* m : { &smoothMeter, &airMeter, &warmthMeter, &bodyMeter, &tapeMeter })
            m->publish (meterSmoothing);

        warmMix = std::min (1.5f, s.warmth * 0.05f * strength);
        triodeK = std::min (4.0f, 0.5f + 0.12f * s.warmth);
        triodeMix = strength;
        tapeOn = s.tape;

        // Loudness match
        const float inDb = powerToDb (inMs), outDb = powerToDb (outMs);
        if (s.autoGain && inDb > -60.0f)
            autoDb = std::clamp (autoDb + (inDb - outDb) * (1.0f - std::exp (-dt / 1.5f)), -9.0f, 9.0f);
        else if (! s.autoGain)
            autoDb *= std::exp (-dt / 0.3f);
    }

    void SilkStage::process (float* const* data, int numChannels, int n, const Settings& s, float blendTarget) noexcept
    {
        const int chans = std::min (numChannels, 2);
        if (chans <= 0 || n <= 0)
            return;

        if (blendTarget <= 0.0f && blend < 1.0e-4f)
        {
            if (blend != 0.0f || smoothingDb != 0.0f || autoDb != 0.0f)
                reset();
            return;   // out of circuit: untouched
        }

        constexpr float bias = 0.20f;
        const float tanhBias = std::tanh (bias), sech2 = 1.0f - tanhBias * tanhBias;

        for (int i = 0; i < n; ++i)
        {
            if (--toTick <= 0)
            {
                controlTick (s);
                toTick = controlInterval;
            }

            // SMOOTH's detector listens above 120 Hz only (24 dB/oct): its band-pass skirts used to pick up
            // a loud bass note, which moved every band's neighbourhood and so the dips (pumping)
            const float rawMono = chans == 2 ? 0.5f * (data[0][i] + data[1][i]) : data[0][i];
            const float mono = detectHp2.process (detectHpCoeffs, detectHp1.process (detectHpCoeffs, rawMono));

            // The low end, for SUB: how loud the bass is, and the bloom that follows it
            {
                const float b = bassLp2.process (bassLpCoeffs, bassLp1.process (bassLpCoeffs, rawMono));
                const float bp = b * b;
                bassPower = bassK * bassPower + (1.0f - bassK) * bp;
                bassFast = (bp > bassFast ? bassFastAtt : bassFastRel) * (bassFast - bp) + bp;
                bassSlow = bassSlowK * bassSlow + (1.0f - bassSlowK) * bp;
                if (bloomMix > 1.0e-4f)
                {
                    float out = 0.0f;
                    for (size_t d = 0; d < 2; ++d)
                    {
                        auto& line = bloomLine[d];
                        const float y = line[(size_t) bloomPos[d]];
                        bloomLp[d] += (y - bloomLp[d]) * bloomLpK;
                        line[(size_t) bloomPos[d]] = b + 0.55f * bloomLp[d];
                        bloomPos[d] = (bloomPos[d] + 1) % bloomLen[d];
                        out += y;
                    }
                    bloomIn = 0.5f * out;
                }
                else
                    bloomIn = 0.0f;
            }

            for (int k = 0; k < numBands; ++k)
            {
                const auto b = (size_t) k;
                const float y = detectState[b].process (detect[b], mono);
                const float p = y * y;
                fast[b] = (p > fast[b] ? fastAtt : fastRel) * (fast[b] - p) + p;
                slow[b] = slowK * (slow[b] - p) + p;

                // PROTECT, sample-accurate: a dip in the way of a fresh attack is lifted immediately
                if (s.protect && designedDb[b] != 0.0f && fast[b] > 3.2f * slow[b] && onsetHold[b] == 0)
                {
                    onsetHold[b] = std::max (1, (int) std::lround (0.015 * sr / controlInterval));
                    for (int j = std::max (0, k - 1); j <= std::min (numBands - 1, k + 1); ++j)
                    {
                        cutDb[(size_t) j] = designedDb[(size_t) j] = 0.0f;
                        dipCoeffs[(size_t) j] = designers[(size_t) j].make (0.0f);
                    }
                }
            }

            const float kin = inShelf.process (kShelf, inHp.process (kHp, mono));
            inMs = msCoeff * inMs + (1.0f - msCoeff) * kin * kin;

            const float gainTarget = dbToGain (autoDb + s.outputDb);
            outGain += (gainTarget - outGain) * 0.001f;
            blend = blendTarget + (blend - blendTarget) * blendCoeff;

            float outMono = 0.0f;
            for (int c = 0; c < chans; ++c)
            {
                auto& st = ch[(size_t) c];
                const float x = data[c][i];
                float w = x;

                for (int k = 0; k < numBands; ++k)
                    if (designedDb[(size_t) k] != 0.0f)
                        w = st.dip[(size_t) k].process (dipCoeffs[(size_t) k], w);
                smoothMeter.add (c, x, w - x);

                float before = w;
                w = st.bodyPeak.process (bodyPeak, w);

                // SUB: shelf, warm 2nd harmonic of the bass (envelope-normalised: same share at any
                // level), and the mono bloom that swells in the gaps after bass notes
                if (subShelfDb != 0.0f || glowMix > 1.0e-4f || bloomMix > 1.0e-4f)
                {
                    w = st.subShelf.process (subShelf, w);
                    const float low = st.subLow.process (subLowCoeffs, w).low;
                    const float la = std::abs (low);
                    st.glowEnv = (la > st.glowEnv ? envAtt : envRel) * (st.glowEnv - la) + la;
                    const float lvl = std::max (1.0e-5f, st.glowEnv);
                    const float u = std::clamp (low / lvl, -1.5f, 1.5f);
                    const float glow = st.glowBand.process (glowBandCoeffs, u * u).band * lvl * 0.5f;
                    w += glow * glowMix;
                    const float busy = saturate01 ((powerToDb (bassFast) - powerToDb (bassSlow) + 2.0f) / 6.0f);
                    w += bloomIn * bloomMix * (1.0f - busy);
                }
                bodyMeter.add (c, x, w - before);

                before = w;
                w = st.airShelf.process (airShelf, w);
                if (airMix > 1.0e-4f)
                    w += airMix * excite (st.air, airBand, airPost, airTop, 0.55f, 0.45f, w, envAtt, envRel);
                airMeter.add (c, x, w - before);

                before = w;
                if (warmMix > 1.0e-4f)
                    w += warmMix * excite (st.warm, warmBand, warmPost, warmTop, 0.80f, 0.30f, w, envAtt, envRel);

                // Gentle triode curve (unity small-signal gain) on what lies above 120 Hz, driven as if that
                // part sat at -12 dBFS whatever its real level (envelope-normalised), then scaled back. Only
                // the change is added, scaled by STRENGTH and DC-blocked: at strength 0 nothing changes.
                const float upper = w - st.triSplit.process (triSplitCoeffs, w).low;
                const float ua = std::abs (upper);
                st.triEnv = (ua > st.triEnv ? triEnvAtt : triEnvRel) * (st.triEnv - ua) + ua;
                const float scale = 0.25f / std::max (1.0e-4f, st.triEnv);
                const float u = std::clamp (upper * scale, -1.5f, 1.5f);
                const float tri = ((std::tanh (triodeK * u + bias) - tanhBias) / (triodeK * sech2) - u) / scale;
                const float dc = tri - st.triodeDcX + dcCoeff * st.triodeDcY;
                st.triodeDcX = tri;
                st.triodeDcY = dc;
                w += dc * triodeMix;
                warmthMeter.add (c, x, w - before);

                before = w;
                if (tapeOn)
                {
                    const float v = st.tapePre.process (tapePre, w);
                    const float taped = st.tapePost.process (tapePost, 1.2f * std::tanh (v / 1.2f));
                    w += (taped - w) * triodeMix;
                }
                tapeMeter.add (c, x, w - before);

                w *= outGain;
                const float y = x + (w - x) * blend;
                data[c][i] = y;
                outMono += y;
            }

            outMono /= (float) chans;
            const float kout = outShelf.process (kShelf, outHp.process (kHp, outMono));
            outMs = msCoeff * outMs + (1.0f - msCoeff) * kout * kout;
        }
    }

    //==============================================================================
    // HALO
    //==============================================================================
    void HaloStage::prepare (double sampleRate)
    {
        sr = sampleRate;
        const auto ms = [this] (double m) { return (float) (m * 0.001 * sr); };

        sideLpCoeffs = BiquadCoeffs::lowPass (sr, 120.0, 0.7071);
        sideHpCoeffs = BiquadCoeffs::highPass (sr, 120.0, 0.7071);
        midHpK = onePoleHz (300.0, sr);
        powerK = onePole (1.0, sr);

        const std::array<double, 3> decoMs { 2.3, 3.7, 5.3 };
        for (size_t i = 0; i < decorrelate.size(); ++i)
        {
            decorrelate[i].delay = ms (decoMs[i]);
            decorrelate[i].gain = 0.5f;
            decorrelate[i].line.prepare ((int) decorrelate[i].delay + 4);
        }

        preDelay.prepare ((int) ms (20.0) + 8);
        const std::array<double, 8> earlyMs { 7.1, 13.9, 23.3, 31.7, 9.3, 17.1, 26.9, 37.3 };
        for (size_t t = 0; t < earlyTap.size(); ++t)
            earlyTap[t] = ms (earlyMs[t]);
        early.prepare ((int) ms (40.0) + 8);
        const std::array<double, 4> diffMs { 4.7, 3.6, 12.7, 9.3 };
        for (size_t i = 0; i < diffusers.size(); ++i)
        {
            diffusers[i].delay = ms (diffMs[i]);
            diffusers[i].gain = 0.65f;
            diffusers[i].line.prepare ((int) diffusers[i].delay + 4);
        }

        const std::array<double, numLines> lineMs { 31.3, 37.9, 43.1, 49.7, 57.1, 63.7, 71.9, 79.1 };
        for (size_t i = 0; i < (size_t) numLines; ++i)
        {
            lineLength[i] = ms (lineMs[i]);
            lines[i].prepare ((int) (lineLength[i] + ms (1.0)) + 8);
            lfoRate[i] = (float) (0.13 + 0.055 * (double) i);
            lfoPhase[i] = (float) i * 0.785f;
        }

        inHpK = onePoleHz (200.0, sr);
        inLpK = onePoleHz (11000.0, sr);
        shimmerWindow = ms (50.0);
        fifthWindow = ms (62.0);
        shimmerLine.prepare ((int) std::max (shimmerWindow, fifthWindow) * 2 + 8);
        shimmerHpK = onePoleHz (400.0, sr);

        fastAtt = onePole (0.010, sr);
        fastRel = onePole (0.200, sr);
        slowK = onePole (1.5, sr);
        duckK = onePole (0.080, sr);
        meterK = onePole (0.300, sr);
        blendCoeff = onePole (0.030, sr);
        paramK = onePole (0.050, sr);
        meterSmoothing = onePole (0.12, sr / 32.0);
        reset();
    }

    void HaloStage::reset()
    {
        sideLp1.reset(); sideLp2.reset(); sideHp1.reset(); sideHp2.reset();
        midHpState = inHpState = inLpState = shimmerHpState = shimmerFeed = 0.0f;
        for (auto& a : decorrelate) a.line.clear();
        for (auto& a : diffusers) a.line.clear();
        for (auto& l : lines) l.clear();
        preDelay.clear();
        early.clear();
        shimmerLine.clear();
        damp.fill (0.0f);
        lfoOffset.fill (0.0f);
        shimmerPhase = fifthPhase = driftPhase = 0.0f;
        monoPowerM = monoPowerS = 0.0f;
        dryFast = drySlow = 0.0f;
        duckGain = 1.0f;
        wetPower = dryPower = 0.0f;
        haloDb = -60.0f;
        blend = 0.0f;
        widthSmoothed = 1.0f;
        spaceSmoothed = 0.0f;
        decayDesigned = toneDesigned = -1.0f;
        shimmerEnergy = networkInputEnergy = 0.0;
        for (auto* m : { &widthMeter, &spaceMeter, &shimmerMeter })
            m->clear();
    }

    void HaloStage::process (float* const* data, int numChannels, int n, const Settings& s, float blendTarget) noexcept
    {
        const int chans = std::min (numChannels, 2);
        if (chans <= 0 || n <= 0)
            return;

        if (blendTarget <= 0.0f && blend < 1.0e-4f)
        {
            if (blend != 0.0f || haloDb != -60.0f)
                reset();
            return;
        }

        if (std::abs (s.decayS - decayDesigned) > 0.005f)
        {
            const float decay = std::clamp (s.decayS, 0.2f, 12.0f);
            for (size_t i = 0; i < (size_t) numLines; ++i)
                lineGain[i] = std::pow (10.0f, -3.0f * lineLength[i] / (decay * (float) sr));
            decayDesigned = s.decayS;
        }
        if (std::abs (s.tone - toneDesigned) > 0.002f)
        {
            dampK = onePoleHz (2500.0 * std::pow (12000.0 / 2500.0, (double) std::clamp (s.tone, 0.0f, 1.0f)), sr);
            toneDesigned = s.tone;
        }

        const float modDepth = s.mod ? (float) (0.00045 * sr) : 0.0f;
        const float twoPiOverSr = (float) (2.0 * pi / sr);
        constexpr float invSqrt8 = 0.35355339f;
        constexpr std::array<float, numLines> inSign { 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f };
        const float preDelaySamples = (float) (0.018 * sr);

        for (int i = 0; i < n; ++i)
        {
            const float L = data[0][i], R = chans == 2 ? data[1][i] : L;
            blend = blendTarget + (blend - blendTarget) * blendCoeff;
            widthSmoothed = s.width + (widthSmoothed - s.width) * paramK;
            spaceSmoothed = s.space + (spaceSmoothed - s.space) * paramK;

            // --- width -------------------------------------------------------------------
            const float M = 0.5f * (L + R), S = 0.5f * (L - R);
            // Linkwitz-Riley (LR4) split of the side at 120 Hz: flat when recombined
            const float sLow = sideLp2.process (sideLpCoeffs, sideLp1.process (sideLpCoeffs, S));
            const float sHigh = sideHp2.process (sideHpCoeffs, sideHp1.process (sideHpCoeffs, S));

            monoPowerM = powerK * monoPowerM + (1.0f - powerK) * M * M;
            monoPowerS = powerK * monoPowerS + (1.0f - powerK) * S * S;
            const float mononess = saturate01 (1.0f - (monoPowerS / (monoPowerM + 1.0e-12f)) / 0.3f);

            midHpState = midHpK * midHpState + (1.0f - midHpK) * M;
            float deco = M - midHpState;
            for (auto& a : decorrelate)
                deco = a.process (deco);

            const float strength = std::clamp (s.strength, 0.0f, 5.0f);
            const float width = std::clamp (1.0f + (widthSmoothed - 1.0f) * strength, 0.0f, 8.0f);
            const float extra = std::max (0.0f, width - 1.0f) * mononess * 0.45f * deco;
            const float lowKeep = s.bassMono ? 1.0f - std::min (1.0f, strength) : 1.0f;
            // (at STRENGTH 0 with BASS MONO off, sLow + sHigh is the LR4 all-pass of the side: inaudible)
            const float side = chans == 2 ? sLow * lowKeep + sHigh * width + extra : 0.0f;
            float outL = M + side, outR = M - side;
            widthMeter.add (0, L, outL - L);
            if (chans == 2)
                widthMeter.add (1, R, outR - R);

            // --- reverb input: mid above 200 Hz, below 11 kHz, plus shimmer ------------------
            inHpState = inHpK * inHpState + (1.0f - inHpK) * M;
            inLpState = inLpK * inLpState + (1.0f - inLpK) * (M - inHpState);
            shimmerEnergy += (double) shimmerFeed * shimmerFeed;
            networkInputEnergy += (double) (inLpState + shimmerFeed) * (inLpState + shimmerFeed);
            preDelay.push (inLpState + shimmerFeed);
            float x = preDelay.read (preDelaySamples);

            // Early reflections: sparse stereo taps give the space a believable size before the dense tail
            early.push (x);
            float earlyL = 0.0f, earlyR = 0.0f;
            constexpr std::array<float, 8> earlyGain { 0.62f, 0.48f, 0.38f, 0.28f, 0.60f, 0.46f, 0.36f, 0.27f };
            for (size_t t = 0; t < 4; ++t)
            {
                earlyL += early.read (earlyTap[t]) * earlyGain[t];
                earlyR += early.read (earlyTap[t + 4]) * earlyGain[t + 4];
            }
            for (auto& d : diffusers)
                x = d.process (x);

            // --- feedback delay network ----------------------------------------------------
            if ((i & 31) == 0)
                for (size_t k = 0; k < (size_t) numLines; ++k)
                {
                    lfoPhase[k] += lfoRate[k] * twoPiOverSr * 32.0f;
                    if (lfoPhase[k] > 6.2831853f) lfoPhase[k] -= 6.2831853f;
                    lfoOffset[k] = modDepth * std::sin (lfoPhase[k]);
                }

            std::array<float, numLines> out {}, h {};
            for (size_t k = 0; k < (size_t) numLines; ++k)
            {
                out[k] = lines[k].read (lineLength[k] + lfoOffset[k]);
                damp[k] = (1.0f - dampK) * out[k] + dampK * damp[k];
                h[k] = damp[k];
            }

            for (size_t len = 1; len < (size_t) numLines; len <<= 1)          // fast Walsh-Hadamard
                for (size_t a = 0; a < (size_t) numLines; a += 2 * len)
                    for (size_t b = a; b < a + len; ++b)
                    {
                        const float u = h[b], v = h[b + len];
                        h[b] = u + v;
                        h[b + len] = u - v;
                    }

            for (size_t k = 0; k < (size_t) numLines; ++k)
            {
                float v = h[k] * invSqrt8 * lineGain[k] + x * inSign[k] * 0.35f;
                if (std::abs (v) > 1.5f)
                    v = 1.5f * std::tanh (v / 1.5f);
                lines[k].push (v);
            }

            const float wetL = 0.5f * (out[0] - out[2] + out[4] - out[6]);
            const float wetR = 0.5f * (out[1] - out[3] + out[5] - out[7]);

            // --- shimmer: +12 st two-tap delay pitch shifter fed back into the network --------
            shimmerLine.push (0.5f * (wetL + wetR));
            shimmerPhase += 1.0f / shimmerWindow;
            if (shimmerPhase >= 1.0f) shimmerPhase -= 1.0f;
            const float phaseB = shimmerPhase + 0.5f >= 1.0f ? shimmerPhase - 0.5f : shimmerPhase + 0.5f;
            const float gA = std::sin (3.14159265f * shimmerPhase), gB = std::sin (3.14159265f * phaseB);
            float shifted = shimmerLine.read (1.0f + shimmerWindow * (1.0f - shimmerPhase)) * gA * gA
                          + shimmerLine.read (1.0f + shimmerWindow * (1.0f - phaseB)) * gB * gB;

            // ... and an octave and a fifth up (x3: the read point sweeps 2 samples a sample), quieter
            fifthPhase += 2.0f / fifthWindow;
            if (fifthPhase >= 1.0f) fifthPhase -= 1.0f;
            const float fB = fifthPhase + 0.5f >= 1.0f ? fifthPhase - 0.5f : fifthPhase + 0.5f;
            const float hA = std::sin (3.14159265f * fifthPhase), hB = std::sin (3.14159265f * fB);
            shifted += 0.45f * (shimmerLine.read (1.0f + fifthWindow * (1.0f - fifthPhase)) * hA * hA
                              + shimmerLine.read (1.0f + fifthWindow * (1.0f - fB)) * hB * hB);
            shimmerHpState = shimmerHpK * shimmerHpState + (1.0f - shimmerHpK) * shifted;
            shimmerFeed = std::min (0.85f, s.shimmer * 0.55f * std::clamp (s.strength, 0.0f, 5.0f)) * (shifted - shimmerHpState);

            // --- ducking: the tail makes room while the programme is busy ---------------------
            // Keyed on the same band the tail is made from (mid, 200 Hz up): a bass note coming and
            // going no longer ducks the tail and lets it swell back (pumping).
            const float keyed = M - inHpState;
            const float p = keyed * keyed;
            dryFast = (p > dryFast ? fastAtt : fastRel) * (dryFast - p) + p;
            drySlow = slowK * (drySlow - p) + p;
            if ((i & 15) == 0)
            {
                const float busy = saturate01 ((powerToDb (dryFast) - powerToDb (drySlow) + 3.0f) / 9.0f);
                const float target = s.duck ? dbToGain (-10.0f * busy) : 1.0f;
                duckGain = target + (duckGain - target) * std::pow (duckK, 16.0f);
            }

            const float level = 0.7f * spaceSmoothed * duckGain * std::clamp (s.strength, 0.0f, 5.0f);
            float tailL = wetL + 0.45f * earlyL, tailR = wetR + 0.45f * earlyR;

            // MOD: the tail drifts slowly around the stereo field (energy-preserving rotation, ~20 s a cycle)
            if (s.mod)
            {
                if ((i & 63) == 0)
                {
                    driftPhase += 64.0f * 6.2831853f * 0.05f / (float) sr;
                    if (driftPhase > 6.2831853f) driftPhase -= 6.2831853f;
                }
                const float angle = 0.35f * std::sin (driftPhase), cs = std::cos (angle), sn = std::sin (angle);
                const float rl = cs * tailL - sn * tailR, rr = sn * tailL + cs * tailR;
                tailL = rl;
                tailR = rr;
            }
            outL += tailL * level;
            outR += tailR * level;
            spaceMeter.add (0, L, tailL * level);
            shimmerMeter.add (0, L, wetL * level);
            if (chans == 2)
            {
                spaceMeter.add (1, R, tailR * level);
                shimmerMeter.add (1, R, wetR * level);
            }

            if ((i & 31) == 31)
            {
                // The shimmer's share of what feeds the tail, applied to each channel's tail energy
                const double share = networkInputEnergy > 1.0e-12 ? shimmerEnergy / networkInputEnergy : 0.0;
                widthMeter.publish (meterSmoothing);
                spaceMeter.publish (meterSmoothing);
                shimmerMeter.publish (meterSmoothing, share > 1.0e-6 ? 10.0 * std::log10 (share) : -80.0);
                shimmerEnergy *= 0.5;
                networkInputEnergy *= 0.5;
            }

            wetPower = meterK * wetPower + (1.0f - meterK) * 0.5f * (tailL * tailL + tailR * tailR) * level * level;
            dryPower = meterK * dryPower + (1.0f - meterK) * M * M;

            data[0][i] = L + (outL - L) * blend;
            if (chans == 2)
                data[1][i] = R + (outR - R) * blend;
        }

        // Relative tail level; rests at the bottom while nothing is playing
        haloDb = dryPower > 1.0e-8f ? std::clamp (10.0f * std::log10 ((wetPower + 1.0e-12f) / dryPower), -60.0f, 12.0f) : -60.0f;
    }
}
