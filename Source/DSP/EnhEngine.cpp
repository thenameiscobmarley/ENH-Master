#include "EnhEngine.h"
#include <algorithm>

namespace enh::dsp
{
    void EnhEngine::prepare (double sr, int maxBlockSize, int numChannels)
    {
        sampleRate = sr;
        maxBlock = std::max (16, maxBlockSize);
        controlInterval = std::max (8, (int) std::lround (sr / 1500.0));
        controlDt = (float) controlInterval / (float) sr;
        const double controlRate = sr / controlInterval;

        analyzer.prepare (sr, controlRate);
        spectrum.prepare (sr);
        planner.prepare (analyzer);
        eq.prepare (sr, controlRate, analyzer, numChannels);
        sub.prepare (sr, controlRate);
        analog.prepare (sr, maxBlock, numChannels);
        lumen.prepare (sr, numChannels);
        limiter.prepare (sr, maxBlock, controlInterval);
        tide.prepare (sr, numChannels);
        seraph.prepare (sr, maxBlock);
        balancer.prepare (sr, numChannels);
        deep.prepare (sr);
        output.prepare (sr);
        target.prepare (sr);
        character.prepare (sr, maxBlock, numChannels);
        radar.prepare (sr, maxBlock);

        for (auto& v : msScratch)
            v.assign ((size_t) maxBlock, 0.0f);
        for (auto& d : keepDelays)
            d.line.assign ((size_t) std::max ({ 1, character.getLatencySamples(), seraph.getLatencySamples() }), 0.0f);

        // COMPARE: a delay as long as what the rack adds before its output limiter (which delays both
        // alike), and K-weighting to match by
        compareLen = std::max (1, analog.getLatencySamples() + radar.getLatencySamples() + seraph.getLatencySamples() + character.getLatencySamples());
        for (auto& v : compareLine)
            v.assign ((size_t) compareLen, 0.0f);
        for (auto& v : compareIn)
            v.assign ((size_t) maxBlock * 8, 0.0f);   // hosts may send more than they said; above this, COMPARE sits out
        kWeighting (sr, compareKPre, compareKRlb);

        protectHp = SvfCoeffs::make (sr, 8.0, 0.7071);
        startStep = 1.0f / (float) std::max (1, (int) std::lround (0.020 * sr));   // 20 ms fade-in
        loudness.prepare (sr);
        reset();
    }

    void EnhEngine::reset()
    {
        analyzer.reset();
        spectrum.reset();
        planner.reset();
        eq.reset();
        sub.reset();
        analog.reset();
        lumen.reset();
        limiter.reset();
        tide.reset();
        seraph.reset();
        balancer.reset();
        deep.reset();
        footstepRecentS = 0.0f;
        loudness.reset();

        samplesToTick = controlInterval;
        planCountdown = 0;
        output.reset();
        target.reset();
        character.reset();
        radar.reset();
        for (auto& d : keepDelays) { std::fill (d.line.begin(), d.line.end(), 0.0f); d.pos = 0; }
        for (auto& v : compareLine) std::fill (v.begin(), v.end(), 0.0f);
        comparePos = 0;
        for (auto* f : { &compareInPre, &compareInRlb, &compareOutPre, &compareOutRlb })
            for (auto& st : *f) st.reset();
        compareInLevel = compareOutLevel = 0.0;
        compareDb = 0.0f; compareGain = 1.0f; compareMix = 0.0f;
        for (auto& st : protectState) st.reset();
        startGain = 0.0f;
        transient = 0.0f;

        for (auto& g : meters.bandGainDb) g = 0.0f;
    }

    void EnhEngine::controlTick (const Parameters& p) noexcept
    {
        const float dt = controlDt;
        const float smooth = 1.0f - std::exp (-dt / 0.05f);
        normalize += (p.normalize - normalize) * smooth;
        boost     += (p.boost - boost) * smooth;
        strength  += (p.strength - strength) * smooth;
        speed     += (p.adaptSpeed - speed) * smooth;
        subAmount += (p.sub - subAmount) * smooth;

        // Long-term spectrum integration follows ADAPT: 6 s (steady) .. 0.8 s (fast)
        analyzer.update (dt, 6.0f * std::pow (0.8f / 6.0f, speed));
        if (--planCountdown <= 0)
        {
            planCountdown = 2;
            planner.update (analyzer, speed, 2.0f * dt);
        }

        eq.update (analyzer, { normalize, boost, std::min (speed, 1.5f), planner.depth, planner.clarity, strength }, dt);
        sub.update ({ subAmount, p.subBoost, std::min (speed, 1.5f), planner.bassHz, strength }, dt);

        const float t = saturate01 ((analyzer.fullTransientDb - analyzer.fullShortDb) / 6.0f);
        transient = std::max (t, transient * std::exp (-dt / 0.08f));

        // Publish meters (atomics, wait-free)
        for (int k = 0; k < numBands; ++k)
            meters.bandGainDb[(size_t) k].store (eq.getGainDb (k), std::memory_order_relaxed);

        meters.footstepConfidence.store (p.radar.active ? radar.getActivity() : 0.0f, std::memory_order_relaxed);
        // What is actually being done to the signal right now (EQ movement + harmonics being generated)
        const float harmonics = boost * std::max (planner.depth.amount, planner.clarity.amount) * saturate01 ((analyzer.fullShortDb + 70.0f) / 20.0f);
        meters.enhancement.store (saturate01 (eq.getActivity() + 0.6f * harmonics), std::memory_order_relaxed);
        meters.subLiftDb.store (sub.getLiftDb(), std::memory_order_relaxed);
    }

    void EnhEngine::process (juce::AudioBuffer<float>& buffer, const Parameters& p) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        // COMPARE needs the block as it came in, before anything touches it
        const int inChans = std::min (2, buffer.getNumChannels());
        const bool canCompare = inChans > 0 && buffer.getNumSamples() <= (int) compareIn[0].size();
        compareReady = canCompare;
        if (canCompare)
            for (int c = 0; c < 2; ++c)
                std::copy_n (buffer.getReadPointer (std::min (c, inChans - 1)), buffer.getNumSamples(), compareIn[(size_t) c].data());

        // LEVEL, first: every unit after it hears the level it sets (smoothed over ~20 ms)
        {
            const float target = std::pow (10.0f, std::clamp (p.levelDb, -24.0f, 12.0f) / 20.0f);
            const int glide = p.methods[(size_t) methods::levelGlide];   // GLIDE method: 20 ms, 5 ms or 150 ms
            const float k = 1.0f - std::exp (-1.0f / ((glide == 1 ? 0.005f : glide == 2 ? 0.150f : 0.020f) * (float) sampleRate));
            const int chans = std::min (2, buffer.getNumChannels());
            auto* const* w = buffer.getArrayOfWritePointers();
            if (std::abs (levelGain - target) > 1.0e-6f || std::abs (levelGain - 1.0f) > 1.0e-6f)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    levelGain += (target - levelGain) * k;
                    for (int c = 0; c < chans; ++c)
                        w[c][i] *= levelGain;
                }
            levelDbNow = 20.0f * std::log10 (std::max (1.0e-6f, levelGain));
            meters.levelDb.store (levelDbNow, std::memory_order_relaxed);
        }

        // Analyser tap, before anything else: the audio thread only copies samples.
        scopeIn.push (buffer.getArrayOfReadPointers(), std::min (2, buffer.getNumChannels()), buffer.getNumSamples());

        // Hosts may exceed the prepared block size: process in chunks the oversampler accepts.
        for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
            processChunk (buffer, start, std::min (maxBlock, buffer.getNumSamples() - start), p);

        // The last check on what leaves: a fade-in after a start or a reset (no pop in a headset), and
        // anything that is not a number - which would be a full-scale burst - becomes silence while the
        // rack starts clean again. Then a hard ceiling that the output limiter should never let it reach.
        {
            const int chansOut = std::min (2, buffer.getNumChannels());
            bool bad = false;
            for (int c = 0; c < chansOut && ! bad; ++c)
            {
                const float* x = buffer.getReadPointer (c);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                    if (! std::isfinite (x[i])) { bad = true; break; }
            }

            if (bad)
            {
                buffer.clear();
                reset();
            }
            else
            {
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    if (startGain < 1.0f)
                        startGain = std::min (1.0f, startGain + startStep);
                    for (int c = 0; c < chansOut; ++c)
                        buffer.setSample (c, i, std::clamp (buffer.getSample (c, i) * startGain, -1.0f, 1.0f));
                }
            }
            meters.protectionTripped.store (bad, std::memory_order_relaxed);
        }

        scopeOut.push (buffer.getArrayOfReadPointers(), std::min (2, buffer.getNumChannels()), buffer.getNumSamples());

        // Loudness of what leaves the rack
        if (loudnessResetPending.exchange (false, std::memory_order_relaxed))
            loudness.resetIntegrated();
        loudness.process (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());
        meters.momentaryLufs.store (loudness.getMomentaryLufs(), std::memory_order_relaxed);
        meters.shortTermLufs.store (loudness.getShortTermLufs(), std::memory_order_relaxed);
        meters.integratedLufs.store (loudness.getIntegratedLufs(), std::memory_order_relaxed);
        meters.truePeakDb.store (loudness.getTruePeakDb(), std::memory_order_relaxed);

        for (int b = 0; b < MixBalancer::numBands; ++b)
        {
            meters.balanceGainDb[(size_t) b].store (balancer.getGainDb (b), std::memory_order_relaxed);
            meters.balanceLevelDb[(size_t) b].store (balancer.getLevelDb (b), std::memory_order_relaxed);
        }
        for (int k = 0; k < MixBalancer::numFine; ++k)
            meters.balanceFineGainDb[(size_t) k].store (balancer.getFineGainDb (k), std::memory_order_relaxed);
        meters.balanceResolution.store (p.balancer.resolution, std::memory_order_relaxed);
        meters.balanceMakeupDb.store (balancer.getMakeupDb(), std::memory_order_relaxed);
        meters.deepGeneratedDb.store (deep.getGeneratedDb(), std::memory_order_relaxed);
        meters.deepPitchHz.store (deep.getPitchHz(), std::memory_order_relaxed);
        meters.deepConfidence.store (deep.getConfidence(), std::memory_order_relaxed);
        for (int r = 0; r < FinalLimiter::numRegions; ++r)
            meters.outputRegionCutDb[(size_t) r].store (output.getRegionCutDb()[(size_t) r], std::memory_order_relaxed);
        meters.outputLimitDb.store (output.getReductionDb(), std::memory_order_relaxed);
        meters.targetGainDb.store (target.getGainDb(), std::memory_order_relaxed);
        meters.charHarmonicsDb.store (character.getHarmonicsDb(), std::memory_order_relaxed);

        const auto& comp = tide.getReadout();
        meters.tideGrDb.store (comp.gainReductionDb, std::memory_order_relaxed);
        meters.tideThresholdDb.store (comp.thresholdDb, std::memory_order_relaxed);
        meters.tideRatio.store (comp.ratio, std::memory_order_relaxed);
        meters.tideInputDb.store (comp.inputDb, std::memory_order_relaxed);
        meters.tideOutputDb.store (comp.outputDb, std::memory_order_relaxed);
        meters.tideAdaptivity.store (comp.adaptivity, std::memory_order_relaxed);

        const auto& lev = lumen.getReadout();
        for (int b = 0; b < SpectralLeveler::numBands; ++b)
        {
            meters.lumenGainDb[(size_t) b].store (lev.gainDb[(size_t) b], std::memory_order_relaxed);
            meters.lumenLevelDb[(size_t) b].store (lev.levelDb[(size_t) b], std::memory_order_relaxed);
        }
        meters.lumenTotalDb.store (lev.totalGainDb, std::memory_order_relaxed);
        meters.lumenActivity.store (lev.activity, std::memory_order_relaxed);

        const auto cuts = limiter.getSlots();
        for (size_t s = 0; s < cuts.size(); ++s)
        {
            meters.limitHz[s].store (cuts[s].hz, std::memory_order_relaxed);
            meters.limitOctaves[s].store (cuts[s].octaves, std::memory_order_relaxed);
            meters.limitDepthDb[s].store (cuts[s].depthDb, std::memory_order_relaxed);
            meters.limitShape[s].store ((int) cuts[s].shape, std::memory_order_relaxed);
        }
        meters.limitDeepestDb.store (limiter.getDeepestCutDb(), std::memory_order_relaxed);
        meters.limitBroadbandDb.store (limiter.getBroadbandDb(), std::memory_order_relaxed);
        meters.limitMakeupDb.store (limiter.getMakeupDb(), std::memory_order_relaxed);

        meters.autoGainDb.store (analog.getAutoGainDb(), std::memory_order_relaxed);
        meters.outputPeakDb.store (analog.getPeakDb(), std::memory_order_relaxed);

        meters.silkSmoothingDb.store (seraph.getSilk().getSmoothingDb(), std::memory_order_relaxed);
        meters.haloDb.store (seraph.getHalo().getHaloDb(), std::memory_order_relaxed);
        meters.silkBlend.store (seraph.getSilk().getBlend(), std::memory_order_relaxed);
        meters.haloBlend.store (seraph.getHalo().getBlend(), std::memory_order_relaxed);

        for (int a = 0; a < Seraph::numActivities; ++a)
            for (int c = 0; c < 2; ++c)
                meters.seraphActivityDb[(size_t) (a * 2 + c)].store (seraph.getActivityDb (a, c), std::memory_order_relaxed);
        meters.seraphLevelDb.store (seraph.getSilk().getBlend() > 0.01f ? seraph.getSilk().getLevelDb() : 0.0f, std::memory_order_relaxed);
        meters.heavenAutoBlend.store (seraph.getAutoBlend(), std::memory_order_relaxed);
        {
            const auto& a = seraph.getAutoChoice();
            const float chosen[7] { a.space, a.decayS, a.shimmer, a.tone, a.width, a.air, a.sub };
            for (size_t k = 0; k < 7; ++k)
                meters.heavenAutoChoice[k].store (chosen[k], std::memory_order_relaxed);
        }
        meters.silkMatchDb.store (seraph.getSilk().getBlend() > 0.01f ? seraph.getSilk().getMatchDb() : 0.0f, std::memory_order_relaxed);
        const auto& dips = seraph.getSilk().getDips();
        for (size_t k = 0; k < dips.size(); ++k)
            meters.silkDipDb[k].store (dips[k], std::memory_order_relaxed);
    }

    template <typename Fn>
    void EnhEngine::inStereoMode (float* const* chunk, int chans, int n, int mode, int latency, KeepDelay& keep, Fn&& run) noexcept
    {
        if (mode <= 0 || chans < 2 || n > (int) msScratch[0].size())
        {
            run (chunk, chans);
            return;
        }

        float* a = msScratch[0].data();
        float* b = msScratch[1].data();
        float* kept = msScratch[2].data();
        const bool mid = mode == 1;

        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (chunk[0][i] + chunk[1][i]), sd = 0.5f * (chunk[0][i] - chunk[1][i]);
            a[i] = b[i] = mid ? m : sd;
            kept[i] = mid ? sd : m;
        }

        float* both[2] { a, b };
        run (both, 2);

        // The part left alone waits for the unit's latency, so the two meet again in time
        if (latency > 0)
        {
            // Delayed by this unit's own latency: the line may be longer (it is sized for the longest
            // unit), so wrap at the latency, not at the line's end
            const int len = std::min (latency, (int) keep.line.size());
            if (keep.pos >= len)
                keep.pos = 0;
            for (int i = 0; i < n; ++i)
            {
                const float v = keep.line[(size_t) keep.pos];
                keep.line[(size_t) keep.pos] = kept[i];
                kept[i] = v;
                if (++keep.pos == len) keep.pos = 0;
            }
        }

        // Back to left and right. The unit's two outputs are kept as a pair (not averaged), so what it
        // makes wide from the middle - a reverb's tail - stays wide.
        for (int i = 0; i < n; ++i)
        {
            if (mid)
            {
                chunk[0][i] = a[i] + kept[i];
                chunk[1][i] = b[i] - kept[i];
            }
            else
            {
                chunk[0][i] = kept[i] + a[i];
                chunk[1][i] = kept[i] - b[i];
            }
        }
    }

    void EnhEngine::compareStage (float* const* chunk, int chans, int n, int start, bool on) noexcept
    {
        if (! compareReady)
            return;
        const float dt = (float) n / (float) sampleRate;

        // Measuring while the rack plays, so a press is loudness-matched from the first moment: the input
        // (as it will be heard, delayed) against the output, K-weighted, over 3 s; held in silence.
        double inE = 0.0, outE = 0.0;
        const float mixStep = 1.0f / (float) std::max (1, (int) std::lround (0.030 * sampleRate));
        const float mixTarget = on ? 1.0f : 0.0f;
        const float gainFrom = compareGain;
        const float gainTo = dbToGain (compareDb);
        compareGain = gainTo;

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float g = gainFrom + (gainTo - gainFrom) * t;
            compareMix = compareMix < mixTarget ? std::min (mixTarget, compareMix + mixStep) : std::max (mixTarget, compareMix - mixStep);

            for (int c = 0; c < 2; ++c)
            {
                auto& line = compareLine[(size_t) c];
                const float dry = line[(size_t) comparePos];
                line[(size_t) comparePos] = compareIn[(size_t) c][(size_t) (start + i)];

                const float wet = chunk[std::min (c, chans - 1)][i];
                const float ki = compareInRlb[(size_t) c].process (compareKRlb, compareInPre[(size_t) c].process (compareKPre, dry));
                const float ko = compareOutRlb[(size_t) c].process (compareKRlb, compareOutPre[(size_t) c].process (compareKPre, wet));
                inE += (double) ki * ki;
                outE += (double) ko * ko;

                if (compareMix > 0.0f && c < chans)
                    chunk[c][i] = wet + (dry * g - wet) * compareMix;
            }
            if (++comparePos == compareLen) comparePos = 0;
        }

        const double k = std::exp (-(double) dt / 3.0);
        compareInLevel = k * compareInLevel + (1.0 - k) * inE / (double) (2 * n);
        compareOutLevel = k * compareOutLevel + (1.0 - k) * outE / (double) (2 * n);
        // Learnt only while listening to the rack: while COMPARE is on the input is played at one steady
        // gain (a gain that rode the rack's movements would not be the untouched input any more)
        if (! on && compareMix == 0.0f && compareInLevel > 1.0e-7 && compareOutLevel > 1.0e-7)   // both above about -70 LUFS
        {
            const float want = std::clamp ((float) (10.0 * std::log10 (compareOutLevel / compareInLevel)), -12.0f, 12.0f);
            compareDb += (want - compareDb) * std::min (1.0f, dt / 1.0f);
        }

        meters.compareGainDb.store (compareDb, std::memory_order_relaxed);
        meters.comparing.store (on, std::memory_order_relaxed);
    }

    void EnhEngine::publishRadar() noexcept
    {
        constexpr auto relaxed = std::memory_order_relaxed;
        meters.radarActivity.store (radar.getActivity(), relaxed);
        meters.radarOnset.store (radar.getOnsetStrength(), relaxed);
        meters.radarThreshold.store (radar.getThreshold(), relaxed);
        meters.radarMusicality.store (radar.getMusicality(), relaxed);
        meters.radarClock.store ((float) radar.getClock(), relaxed);
        for (int b = 0; b < EngineMeters::radarBands; ++b)
            meters.radarExcessDb[(size_t) b].store (radar.getBandExcessDb()[(size_t) b], relaxed);
        for (int t = 0; t < EngineMeters::radarTracks; ++t)
        {
            const auto& tr = radar.getTracks()[(size_t) t];
            meters.radarTrackPan[(size_t) t].store (tr.pan, relaxed);
            meters.radarTrackDistance[(size_t) t].store (tr.distance, relaxed);
            meters.radarTrackConfidence[(size_t) t].store (tr.active ? tr.confidence : 0.0f, relaxed);
            meters.radarTrackPeriod[(size_t) t].store (tr.period, relaxed);
            meters.radarTrackId[(size_t) t].store (tr.active ? tr.id : 0, relaxed);
        }
        const int total = radar.getStepsTotal();
        const int published = meters.radarStepsTotal.load (relaxed);
        if (total != published)
        {
            for (int k = std::max (published, total - EngineMeters::radarRecent); k < total; ++k)
            {
                const auto& st = radar.getRecent()[(size_t) (k % FootstepRadar::recentCapacity)];
                const auto i = (size_t) (k % EngineMeters::radarRecent);
                meters.radarStepTime[i].store ((float) st.time, relaxed);
                meters.radarStepPan[i].store (st.pan, relaxed);
                meters.radarStepRear[i].store (st.rear, relaxed);
                meters.radarStepDistance[i].store (st.distance, relaxed);
                meters.radarStepConfidence[i].store (st.confidence, relaxed);
                meters.radarStepBoost[i].store (st.boostDb, relaxed);
                meters.radarStepTrack[i].store (st.track, relaxed);
            }
            meters.radarStepsTotal.store (total, std::memory_order_release);
        }
    }

    void EnhEngine::processChunk (juce::AudioBuffer<float>& buffer, int start, int n, const Parameters& p) noexcept
    {
        const int chans = std::min (buffer.getNumChannels(), 2);
        if (chans == 0 || n <= 0)
            return;

        auto* const* write = buffer.getArrayOfWritePointers();
        const float* offsetPtrs[2] { write[0] + start, write[chans - 1] + start };
        analog.measureInput (offsetPtrs, chans, n);
        limiter.beginChunk();
        limiter.setAnalysisMethods (p.methods[(size_t) methods::limiterNormal], p.methods[(size_t) methods::limiterWidth]);

        int pos = 0;
        while (pos < n)
        {
            const int seg = std::min (samplesToTick, n - pos);
            const int at = start + pos;

            // Feed-forward analysis of the unprocessed input
            for (int i = 0; i < seg; ++i)
            {
                const float mono = chans == 2 ? 0.5f * (write[0][at + i] + write[1][at + i]) : write[0][at + i];
                analyzer.push (mono);
                spectrum.push (mono);
                sub.measure (mono);
            }

            samplesToTick -= seg;
            if (samplesToTick == 0)
            {
                controlTick (p);
                limiter.analyse (analyzer, controlDt, pos + seg);   // decision lands where it was made
                samplesToTick = controlInterval;
            }

            eq.process (write, chans, at, seg);

            sub.process (write, chans, at, seg);
            pos += seg;
        }

        juce::dsp::AudioBlock<float> block (write, (size_t) chans, (size_t) start, (size_t) n);
        analog.process (block, { boost, transient, planner.depth, planner.clarity, strength,
                                 p.limiter.active && limiter.isHandlingLocalisedEvent(), p.methods[(size_t) methods::enhancerHarmonics] });

        float* chunk[2] { write[0] + start, write[chans - 1] + start };

        // The leveler lifts what is too quiet; the spectral limiter takes out abnormal excess where it
        // is, so the compressor after it only reacts to what is loud across the whole programme.
        auto lumenSettings = p.lumen;
        lumenSettings.holdGains = p.limiter.active && limiter.isHandlingLocalisedEvent();
        lumenSettings.levelDb = levelDbNow;   // LEVEL moves the whole rack; the leveler reads levels relative to it
        inStereoMode (chunk, chans, n, p.methods[(size_t) methods::levelerStereo], 0, keepDelays[0],
                      [&] (float* const* c, int k) { lumen.process (c, k, n, lumenSettings); });
        deep.process (chunk, chans, n, p.deep);   // DEEP SUB: before the limiters, so they look after what it adds
        inStereoMode (chunk, chans, n, p.methods[(size_t) methods::limiterStereo], 0, keepDelays[1],
                      [&] (float* const* c, int k) { limiter.process (c, k, n, p.limiter); });

        // MIX BALANCER, with taps either side of it for its display
        scopeBalIn.push (chunk, chans, n);
        // Footsteps being lifted (one in the last second): the balancer lets its cuts go, so it never takes
        // the lift back
        footstepRecentS = p.radar.active && radar.getActivity() > 0.1f ? 1.0f : std::max (0.0f, footstepRecentS - (float) n / (float) sampleRate);
        auto balancerSettings = p.balancer;
        balancerSettings.holdCuts = footstepRecentS > 0.0f;
        inStereoMode (chunk, chans, n, p.methods[(size_t) methods::balancerStereo], 0, keepDelays[2],
                      [&] (float* const* c, int k) { balancer.process (c, k, n, balancerSettings); });
        scopeBalOut.push (chunk, chans, n);

        inStereoMode (chunk, chans, n, p.methods[(size_t) methods::tideStereo], 0, keepDelays[3],
                      [&] (float* const* c, int k) { tide.process (c, k, n, p.tide, p.limiter.active ? limiter.key() : nullptr); });
        // FOOTSTEP RADAR: after the dynamics (it lifts what they have settled), before TONE & SPACE finishes it
        // It listens to the rack's input (its steps as they came, before the dynamics flattened them) and
        // lifts them here; the few samples the stages between add are well inside its 2.5 ms lookahead
        const float* radarKey[2] { compareIn[0].data() + start, compareIn[chans > 1 ? 1 : 0].data() + start };
        radar.process (chunk, chans, n, p.radar, compareReady ? radarKey : nullptr);
        meters.radarOn.store (p.radar.active, std::memory_order_relaxed);
        publishRadar();

        inStereoMode (chunk, chans, n, p.methods[(size_t) methods::seraphStereo], seraph.getLatencySamples(), keepDelays[4],
                      [&] (float* const* c, int k) { seraph.process (c, k, n, p.seraph); });

        // CHARACTER: the hardware the rack is made of (out: a delay of the same length)
        inStereoMode (chunk, chans, n, p.methods[(size_t) methods::charStereo], character.getLatencySamples(), keepDelays[5],
                      [&] (float* const* c, int k) { character.process (c, k, n, p.character); });

        // LOUDNESS TARGET, then the output limiter: full scale is looked after here, once, cleanly
        target.process (chunk, chans, n, p.methods[(size_t) methods::outputTarget]);

        // COMPARE, then the speakers' protection: both go through the output limiter below
        compareStage (chunk, chans, n, start, p.compare);
        for (int c = 0; c < chans; ++c)
            for (int i = 0; i < n; ++i)
                chunk[c][i] = protectState[(size_t) c].process (protectHp, chunk[c][i]).high;
        output.setCeilingMethod (p.methods[(size_t) methods::outputCeiling]);
        output.process (chunk, chans, n);
    }
}
