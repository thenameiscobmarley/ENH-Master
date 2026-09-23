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
        dipFadeStep = 1.0f / (float) std::max (1.0, 0.001 * sr);   // PROTECT's fade: 1 ms (48 samples: no click, the attack still gets through)

        for (int k = 0; k < numBands; ++k)
        {
            const auto i = (size_t) k;
            bandHz[i] = (float) (150.0 * std::pow (16000.0 / 150.0, k / (double) (numBands - 1)));
            detect.set (k, BiquadCoeffs::bandPass (sr, bandHz[i], 4.0));
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
        mixK = onePole (0.010, sr);   // the mixes glide over 10 ms
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
        slowMsCoeff = onePole (3.0, sr);
        blendCoeff = onePole (0.020, sr);
        meterSmoothing = onePole (0.12, controlRate);
        reset();
    }

    void SilkStage::reset()
    {
        detect.reset();
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
        dipMix.fill (1.0f); dipFading.fill (false);
        dipsDesigned = 0;
        for (int k = 0; k < numBands; ++k)
            dipCoeffs[(size_t) k] = designers[(size_t) k].make (0.0f);

        for (auto& c : ch) c = Channel {};
        airShelf = SvfEqCoeffs::highShelf (sr, 9500.0, 0.55, 0.0);
        bodyPeak = SvfEqCoeffs::bell (sr, 180.0, 0.8, 0.0);
        airShelfDb = bodyDb = 0.0f;
        airMix = warmMix = 0.0f;
        airNow = warmNow = glowNow = bloomNow = 0.0f;
        triodeNow = triodeMix;
        triodeK = 0.6f;

        inHp.reset(); inShelf.reset(); outHp.reset(); outShelf.reset();
        inMs = outMs = 0.0f;
        inSlowMs = outSlowMs = 0.0f;
        inRefDb = -60.0f;
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
                        // Faded out of the signal over 1 ms (setting it to 0 dB in one step clicked); the
                        // neighbours share the transient's skirts: they go too
                        for (int j = std::max (0, k - 1); j <= std::min (numBands - 1, k + 1); ++j)
                            if (designedDb[(size_t) j] != 0.0f)
                                dipFading[(size_t) j] = true;
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
                dipMix[i] = 1.0f;
                dipFading[i] = false;
            }
            else if (std::abs (cutDb[i] - designedDb[i]) > 0.03f)
            {
                dipCoeffs[i] = designers[i].make (cutDb[i]);
                designedDb[i] = cutDb[i];
            }
            deepest = std::min (deepest, cutDb[i]);
        }
        smoothingDb = -deepest;
        dipsDesigned = (int) std::count_if (designedDb.begin(), designedDb.end(), [] (float d) { return d != 0.0f; });

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
        // The shelves glide (0.4 dB a tick, 18 dB in about 30 ms): a knob turned in one go used to
        // step them, and the sound with them
        auto glide = [] (float now, float want) { return now > 500.0f ? want : now + std::clamp (want - now, -0.4f, 0.4f); };
        if (std::abs (shelf - airShelfDb) > 0.05f)
        {
            airShelfDb = glide (airShelfDb, shelf);
            airShelf = SvfEqCoeffs::highShelf (sr, std::min (9500.0, 0.4 * sr), 0.55, airShelfDb);
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
                subShelfDb = glide (subShelfDb, shelfDb);
                subShelf = BiquadCoeffs::lowShelf (sr, 80.0, 0.7, subShelfDb);
            }
            glowMix = std::min (0.6f, amount * 0.03f) * (1.0f - 0.6f * loud);   // loud bass is already felt
            bloomMix = std::min (0.5f, amount * 0.025f);
        }
        if (std::abs (body - bodyDb) > 0.05f)
        {
            bodyDb = glide (bodyDb, body);
            bodyPeak = SvfEqCoeffs::bell (sr, 180.0, 0.8, bodyDb);
        }

        for (auto* m : { &smoothMeter, &airMeter, &warmthMeter, &bodyMeter, &tapeMeter })
            m->publish (meterSmoothing);

        warmMix = std::min (1.5f, s.warmth * 0.05f * strength);
        triodeK = std::min (4.0f, 0.5f + 0.12f * s.warmth);
        triodeMix = strength;
        tapeOn = s.tape;
        tapeFadeLen = std::max (1, (int) std::lround (0.030 * sr));
        if (const int want = std::clamp (s.tapeCurve, 0, 2); want != tapeCurve)
        {
            fadingTapeCurve = tapeCurve;
            tapeCurve = want;
            tapeFadeLeft = tapeFadeLen;
        }

        // Loudness match, on 3 s levels either side, following over 3 s, and holding still through a burst (the
        // input 4 dB or more above its own 3 s level). An explosion is not a reason to change the level policy:
        // on 0.4 s levels the harmonics TONE adds to a big bass hit made the output read louder, and MATCH
        // pulled the whole mix down by up to 4.5 dB for seconds after every one
        const float inDb = powerToDb (inSlowMs), outDb = powerToDb (outSlowMs);
        // What this material is when it is playing: quick to learn, very slow to forget, so a pause does
        // not become the new normal. MATCH used to keep integrating through silence - after a three second
        // gap it came back three decibels down and the music started quiet.
        inRefDb += (inDb > inRefDb ? 1.0f - std::exp (-dt / 1.0f) : 1.0f - std::exp (-dt / 8.0f)) * (inDb - inRefDb);
        const bool burst = powerToDb (inMs) > inDb + 4.0f;
        // Only silence holds it, not merely a quieter passage: holding on anything less meant that after a
        // loud passage MATCH stayed wherever that passage had left it, and the quiet that followed stayed down.
        const bool gap = powerToDb (inMs) < inRefDb - 20.0f;   // read on the fast level, so a gap holds at once
        // Ten seconds, and no more than six decibels either way: MATCH is a trim that keeps the unit from
        // changing the level, not a leveller of its own. At three seconds it followed the programme closely
        // enough to be heard as the mix ducking a second or two after every loud passage.
        if (s.autoGain && inDb > -60.0f && ! burst && ! gap)
            autoDb = std::clamp (autoDb + (inDb - outDb) * (1.0f - std::exp (-dt / 10.0f)), -6.0f, 6.0f);
        else if (! s.autoGain)
            autoDb *= std::exp (-dt / 0.3f);
    }

    // TAPE CURVE: slope 1 at the origin and a ceiling of 1.2 for all three, so they differ only in how they bend
    float SilkStage::tapeShape (int curve, float v) noexcept
    {
        switch (curve)
        {
            case 1:  return 0.76394373f * std::atan (1.30899694f * v);           // arctangent: (2.4 / pi) atan (pi v / 2.4)
            case 2:  { const float u = std::clamp (v / 1.8f, -1.0f, 1.0f);       // cubic soft clip, flat from 1.8
                       return 1.2f * (1.5f * u - 0.5f * u * u * u); }
            default: return 1.2f * std::tanh (v / 1.2f);                           // tanh (the original)
        }
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

        // MATCH's gain only moves on a control tick: work it out then, not with a pow() every sample
        float gainTarget = dbToGain (autoDb + s.outputDb);

        for (int i = 0; i < n; ++i)
        {
            if (--toTick <= 0)
            {
                controlTick (s);
                toTick = controlInterval;
                gainTarget = dbToGain (autoDb + s.outputDb);
            }

            // The mixes the control tick sets glide to it, sample by sample (10 ms): set once a tick they
            // stepped the harmonics in and out when STRENGTH, MULTIPLY or a TONE knob was turned quickly
            warmNow   = warmMix   + (warmNow   - warmMix)   * mixK;
            airNow    = airMix    + (airNow    - airMix)    * mixK;
            triodeNow = triodeMix + (triodeNow - triodeMix) * mixK;
            glowNow   = glowMix   + (glowNow   - glowMix)   * mixK;
            bloomNow  = bloomMix  + (bloomNow  - bloomMix)  * mixK;

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
                        if (++bloomPos[d] == bloomLen[d]) bloomPos[d] = 0;
                        out += y;
                    }
                    bloomIn = 0.5f * out;
                }
                else
                    bloomIn = 0.0f;
            }

            // All detection bands at once (vectorised; the same arithmetic as band by band)
            {
                alignas (16) float y[numBands];
                detect.process (mono, y, numBands);
                float* __restrict f = fast.data();
                float* __restrict sl = slow.data();
                for (int k = 0; k < numBands; ++k)
                {
                    const float p = y[k] * y[k];
                    f[k] = (p > f[k] ? fastAtt : fastRel) * (f[k] - p) + p;
                    sl[k] = slowK * (sl[k] - p) + p;
                }
            }

            for (int k = 0; s.protect && dipsDesigned > 0 && k < numBands; ++k)
            {
                const auto b = (size_t) k;

                // PROTECT, sample-accurate: a dip in the way of a fresh attack is lifted at once - faded out
                // of the signal over 1 ms (dropping it in one sample stepped the waveform: a click)
                if (designedDb[b] != 0.0f && fast[b] > 3.2f * slow[b] && onsetHold[b] == 0)
                {
                    onsetHold[b] = std::max (1, (int) std::lround (0.015 * sr / controlInterval));
                    for (int j = std::max (0, k - 1); j <= std::min (numBands - 1, k + 1); ++j)
                        if (designedDb[(size_t) j] != 0.0f)
                            dipFading[(size_t) j] = true;
                }
            }

            // The dips PROTECT is fading out: once silent, out of the signal path (and their state with them)
            for (int k = 0; dipsDesigned > 0 && k < numBands; ++k)
            {
                const auto b = (size_t) k;
                if (! dipFading[b])
                    continue;
                dipMix[b] -= dipFadeStep;
                if (dipMix[b] <= 0.0f)
                {
                    cutDb[b] = designedDb[b] = 0.0f;
                    dipCoeffs[b] = designers[b].make (0.0f);
                    for (auto& c : ch)
                        c.dip[b].reset();
                    dipMix[b] = 1.0f;
                    dipFading[b] = false;
                    dipsDesigned = (int) std::count_if (designedDb.begin(), designedDb.end(), [] (float d) { return d != 0.0f; });
                }
            }

            const float kin = inShelf.process (kShelf, inHp.process (kHp, mono));
            inMs = msCoeff * inMs + (1.0f - msCoeff) * kin * kin;
            inSlowMs = slowMsCoeff * inSlowMs + (1.0f - slowMsCoeff) * kin * kin;

            outGain += (gainTarget - outGain) * 0.001f;
            blend = blendTarget + (blend - blendTarget) * blendCoeff;

            // How busy the bass is right now (the bloom only swells in the gaps): once per sample, one log
            const float busy = saturate01 ((10.0f * std::log10 ((bassFast + 1.0e-12f) / (bassSlow + 1.0e-12f)) + 2.0f) / 6.0f);

            float outMono = 0.0f;
            // TAPE CURVE crossfade weight for this sample (1 = the chosen curve only)
            const float tapeT = tapeFadeLeft > 0 ? 1.0f - (float) --tapeFadeLeft / (float) tapeFadeLen : 1.0f;
            for (int c = 0; c < chans; ++c)
            {
                auto& st = ch[(size_t) c];
                const float x = data[c][i];
                float w = x;

                for (int k = 0; k < numBands; ++k)
                    if (designedDb[(size_t) k] != 0.0f)
                    {
                        const float y = st.dip[(size_t) k].process (dipCoeffs[(size_t) k], w);
                        w = dipFading[(size_t) k] ? w + (y - w) * dipMix[(size_t) k] : y;
                    }
                smoothMeter.add (c, x, w - x);

                float before = w;
                w = st.bodyPeak.process (bodyPeak, w);

                // SUB: shelf, warm 2nd harmonic of the bass (envelope-normalised: same share at any
                // level), and the mono bloom that swells in the gaps after bass notes
                if (subShelfDb != 0.0f || glowNow > 1.0e-4f || bloomNow > 1.0e-4f)
                {
                    w = st.subShelf.process (subShelf, w);
                    const float low = st.subLow.process (subLowCoeffs, w).low;
                    const float la = std::abs (low);
                    st.glowEnv = (la > st.glowEnv ? envAtt : envRel) * (st.glowEnv - la) + la;
                    const float lvl = std::max (1.0e-5f, st.glowEnv);
                    const float u = std::clamp (low / lvl, -1.5f, 1.5f);
                    const float glow = st.glowBand.process (glowBandCoeffs, u * u).band * lvl * 0.5f;
                    w += glow * glowNow;
                    w += bloomIn * bloomNow * (1.0f - busy);
                }
                bodyMeter.add (c, x, w - before);

                before = w;
                w = st.airShelf.process (airShelf, w);
                if (airNow > 1.0e-4f)
                    w += airNow * excite (st.air, airBand, airPost, airTop, 0.55f, 0.45f, w, envAtt, envRel);
                airMeter.add (c, x, w - before);

                before = w;
                if (warmNow > 1.0e-4f)
                    w += warmNow * excite (st.warm, warmBand, warmPost, warmTop, 0.80f, 0.30f, w, envAtt, envRel);

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
                // The bias makes even harmonics, and with them a DC term that rises and falls with every
                // transient's envelope. A 6 Hz DC blocker let that through as a faint thump under each hit
                // (a 2 kHz click came out with low end at -55 dB). The curve only sees what lies above
                // 120 Hz, so nothing it makes below 120 Hz is a harmonic: take it all out, 24 dB/octave.
                const float added = st.triHp2.process (triSplitCoeffs, st.triHp1.process (triSplitCoeffs, dc).high).high;
                w += added * triodeNow;
                warmthMeter.add (c, x, w - before);

                before = w;
                if (tapeOn)
                {
                    const float v = st.tapePre.process (tapePre, w);
                    float shaped = tapeShape (tapeCurve, v);
                    if (tapeT < 1.0f)
                    {
                        const float old = tapeShape (fadingTapeCurve, v);
                        shaped = old + (shaped - old) * tapeT;
                    }
                    const float taped = st.tapePost.process (tapePost, shaped);
                    w += (taped - w) * triodeNow;
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
            outSlowMs = slowMsCoeff * outSlowMs + (1.0f - slowMsCoeff) * kout * kout;
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
        lowPowerK = onePole (0.150, sr);

        const std::array<double, 3> decoMs { 2.3, 3.7, 5.3 };
        for (size_t i = 0; i < decorrelate.size(); ++i)
        {
            decorrelate[i].delay = ms (decoMs[i]);
            decorrelate[i].gain = 0.5f;
            decorrelate[i].line.prepare ((int) decorrelate[i].delay + 4);
        }

        preDelay.prepare ((int) ms (40.0) + 8);   // up to the longest PRE-DELAY method (35 ms)
        preDelayFadeLen = std::max (1, (int) std::lround (0.030 * sr));
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
        midLowMeter.reset();
        lowPowerM = lowPowerS = 0.0f;
        dryFast = drySlow = 0.0f;
        duckGain = 1.0f;
        wetPower = dryPower = 0.0f;
        haloDb = -60.0f;
        blend = 0.0f;
        widthSmoothed = 1.0f;
        spaceSmoothed = 0.0f;
        strengthSmoothed = 1.0f;
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
        if (const int want = std::clamp (s.preDelay, 0, 2); want != preDelayMethod)
        {
            fadingPreDelay = preDelayMethod;
            preDelayMethod = want;
            preDelayFadeLeft = preDelayFadeLen;
        }
        const float preDelaySamples = (float) (preDelaySeconds (preDelayMethod) * sr);
        const float oldPreDelaySamples = fadingPreDelay >= 0 ? (float) (preDelaySeconds (fadingPreDelay) * sr) : preDelaySamples;

        for (int i = 0; i < n; ++i)
        {
            const float L = data[0][i], R = chans == 2 ? data[1][i] : L;
            blend = blendTarget + (blend - blendTarget) * blendCoeff;
            widthSmoothed = s.width + (widthSmoothed - s.width) * paramK;
            spaceSmoothed = s.space + (spaceSmoothed - s.space) * paramK;
            // STRENGTH glides too: turned in one go it used to step the width and the tail (a click)
            strengthSmoothed = std::clamp (s.strength, 0.0f, 5.0f) + (strengthSmoothed - std::clamp (s.strength, 0.0f, 5.0f)) * paramK;

            // --- width -------------------------------------------------------------------
            const float M = 0.5f * (L + R), S = 0.5f * (L - R);
            // Linkwitz-Riley (LR4) split of the side at 120 Hz: flat when recombined
            const float sLow1 = sideLp1.process (sideLpCoeffs, S);
            const float sLow = sideLp2.process (sideLpCoeffs, sLow1);
            const float sHigh = sideHp2.process (sideHpCoeffs, sideHp1.process (sideHpCoeffs, S));

            monoPowerM = powerK * monoPowerM + (1.0f - powerK) * M * M;
            monoPowerS = powerK * monoPowerS + (1.0f - powerK) * S * S;
            const float mononess = saturate01 (1.0f - (monoPowerS / (monoPowerM + 1.0e-12f)) / 0.3f);

            midHpState = midHpK * midHpState + (1.0f - midHpK) * M;
            float deco = M - midHpState;
            for (auto& a : decorrelate)
                deco = a.process (deco);

            const float strength = strengthSmoothed;
            const float width = std::clamp (1.0f + (widthSmoothed - 1.0f) * strength, 0.0f, 8.0f);
            const float extra = std::max (0.0f, width - 1.0f) * mononess * 0.45f * deco;
            const float lowKeep = s.bassMono ? 1.0f - std::min (1.0f, strength) : 1.0f;
            // (at STRENGTH 0 with BASS MONO off, sLow + sHigh is the LR4 all-pass of the side: inaudible)
            const float side = chans == 2 ? sLow * lowKeep + sHigh * width + extra : 0.0f;

            // BASS MONO takes the low side away. Where the low end is mostly side - out of phase: a rear
            // effect in a surround downmix, a miswired channel - that was all of the low end (-16 dB on
            // out-of-phase footsteps). There the side's low end is folded into the middle instead: the
            // bass stays, now mono. Measured on one section of each crossover (150 ms); nothing changes
            // while the low end is mostly in phase, and the fold is full once the side carries 3x the mid.
            float fold = 0.0f;
            if (chans == 2 && lowKeep < 1.0f)
            {
                const float mLow1 = midLowMeter.process (sideLpCoeffs, M);
                lowPowerM = lowPowerK * lowPowerM + (1.0f - lowPowerK) * mLow1 * mLow1;
                lowPowerS = lowPowerK * lowPowerS + (1.0f - lowPowerK) * sLow1 * sLow1;
                fold = saturate01 (2.0f * (lowPowerS - lowPowerM) / (lowPowerS + lowPowerM + 1.0e-12f)) * (1.0f - lowKeep) * sLow;
            }
            float outL = M + fold + side, outR = M + fold - side;
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
            if (preDelayFadeLeft > 0)   // PRE-DELAY changed: crossfade from the old tap
            {
                const float old = preDelay.read (oldPreDelaySamples);
                x = old + (x - old) * (1.0f - (float) --preDelayFadeLeft / (float) preDelayFadeLen);
            }

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
            // The line is always fed and the phases always run, so turning SHIMMER up starts cleanly; the
            // pitch shifting itself is only done when it is heard. The two taps' windows are sin^2 half a
            // period apart, so the second is 1 - the first (one sin a shifter, not two).
            shimmerLine.push (0.5f * (wetL + wetR));
            shimmerPhase += 1.0f / shimmerWindow;
            if (shimmerPhase >= 1.0f) shimmerPhase -= 1.0f;
            fifthPhase += 2.0f / fifthWindow;
            if (fifthPhase >= 1.0f) fifthPhase -= 1.0f;

            const float shimmerAmount = std::min (0.85f, s.shimmer * 0.55f * strengthSmoothed);
            if (shimmerAmount > 0.0f)
            {
                const float phaseB = shimmerPhase + 0.5f >= 1.0f ? shimmerPhase - 0.5f : shimmerPhase + 0.5f;
                const float gA = std::sin (3.14159265f * shimmerPhase), wA = gA * gA;
                float shifted = shimmerLine.read (1.0f + shimmerWindow * (1.0f - shimmerPhase)) * wA
                              + shimmerLine.read (1.0f + shimmerWindow * (1.0f - phaseB)) * (1.0f - wA);

                // ... and an octave and a fifth up (x3: the read point sweeps 2 samples a sample), quieter
                const float fB = fifthPhase + 0.5f >= 1.0f ? fifthPhase - 0.5f : fifthPhase + 0.5f;
                const float hA = std::sin (3.14159265f * fifthPhase), vA = hA * hA;
                shifted += 0.45f * (shimmerLine.read (1.0f + fifthWindow * (1.0f - fifthPhase)) * vA
                                  + shimmerLine.read (1.0f + fifthWindow * (1.0f - fB)) * (1.0f - vA));
                shimmerHpState = shimmerHpK * shimmerHpState + (1.0f - shimmerHpK) * shifted;
                shimmerFeed = shimmerAmount * (shifted - shimmerHpState);
            }
            else
            {
                shimmerHpState *= shimmerHpK;
                shimmerFeed = 0.0f;
            }

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

            const float level = 0.7f * spaceSmoothed * duckGain * strengthSmoothed;
            float tailL = wetL + 0.45f * earlyL, tailR = wetR + 0.45f * earlyR;

            // MOD: the tail drifts slowly around the stereo field (energy-preserving rotation, ~20 s a cycle)
            if (s.mod)
            {
                if ((i & 63) == 0)
                {
                    driftPhase += 64.0f * 6.2831853f * 0.05f / (float) sr;
                    if (driftPhase > 6.2831853f) driftPhase -= 6.2831853f;
                    const float angle = 0.35f * std::sin (driftPhase);   // moves every 64 samples, so worked out then
                    driftCos = std::cos (angle);
                    driftSin = std::sin (angle);
                }
                const float cs = driftCos, sn = driftSin;
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
