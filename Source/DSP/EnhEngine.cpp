#include "EnhEngine.h"

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
        steps.prepare (analyzer, controlRate);
        planner.prepare (analyzer);
        eq.prepare (sr, controlRate, analyzer, numChannels);
        sub.prepare (sr, controlRate);
        analog.prepare (sr, maxBlock, numChannels);
        lumen.prepare (sr, numChannels);
        limiter.prepare (sr, maxBlock, controlInterval);
        tide.prepare (sr, numChannels);
        seraph.prepare (sr);
        balancer.prepare (sr, numChannels);
        output.prepare (sr);
        loudness.prepare (sr);
        reset();
    }

    void EnhEngine::reset()
    {
        analyzer.reset();
        spectrum.reset();
        steps.reset();
        planner.reset();
        eq.reset();
        sub.reset();
        analog.reset();
        lumen.reset();
        limiter.reset();
        tide.reset();
        seraph.reset();
        balancer.reset();
        loudness.reset();

        samplesToTick = controlInterval;
        planCountdown = 0;
        output.reset();
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
        const float confidence = steps.update (analyzer, spectrum, dt);
        if (--planCountdown <= 0)
        {
            planCountdown = 2;
            planner.update (analyzer, speed, 2.0f * dt);
        }

        eq.update (analyzer, steps, { normalize, boost, std::min (speed, 1.5f), p.footstep, planner.depth, planner.clarity, strength }, dt);
        sub.update ({ subAmount, p.subBoost, std::min (speed, 1.5f), planner.bassHz, strength }, dt);

        const float t = saturate01 ((analyzer.fullTransientDb - analyzer.fullShortDb) / 6.0f);
        transient = std::max (t, transient * std::exp (-dt / 0.08f));

        // Publish meters (atomics, wait-free)
        for (int k = 0; k < numBands; ++k)
            meters.bandGainDb[(size_t) k].store (eq.getGainDb (k), std::memory_order_relaxed);

        meters.footstepConfidence.store (p.footstep ? confidence : 0.0f, std::memory_order_relaxed);
        // What is actually being done to the signal right now (EQ movement + harmonics being generated)
        const float harmonics = boost * std::max (planner.depth.amount, planner.clarity.amount) * saturate01 ((analyzer.fullShortDb + 70.0f) / 20.0f);
        meters.enhancement.store (saturate01 (eq.getActivity() + 0.6f * harmonics), std::memory_order_relaxed);
        meters.subLiftDb.store (sub.getLiftDb(), std::memory_order_relaxed);
    }

    void EnhEngine::process (juce::AudioBuffer<float>& buffer, const Parameters& p) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        // LEVEL, first: every unit after it hears the level it sets (smoothed over ~20 ms)
        {
            const float target = std::pow (10.0f, std::clamp (p.levelDb, -24.0f, 12.0f) / 20.0f);
            const float k = 1.0f - std::exp (-1.0f / (0.020f * (float) sampleRate));
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
        for (int r = 0; r < FinalLimiter::numRegions; ++r)
            meters.outputRegionCutDb[(size_t) r].store (output.getRegionCutDb()[(size_t) r], std::memory_order_relaxed);
        meters.outputLimitDb.store (output.getReductionDb(), std::memory_order_relaxed);

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

    void EnhEngine::processChunk (juce::AudioBuffer<float>& buffer, int start, int n, const Parameters& p) noexcept
    {
        const int chans = std::min (buffer.getNumChannels(), 2);
        if (chans == 0 || n <= 0)
            return;

        auto* const* write = buffer.getArrayOfWritePointers();
        const float* offsetPtrs[2] { write[0] + start, write[chans - 1] + start };
        analog.measureInput (offsetPtrs, chans, n);
        limiter.beginChunk();

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
        analog.process (block, { boost, transient, p.footstep ? steps.getConfidence() : 0.0f, planner.depth, planner.clarity, strength,
                                 p.limiter.active && limiter.isHandlingLocalisedEvent() });

        float* chunk[2] { write[0] + start, write[chans - 1] + start };

        // The leveler lifts what is too quiet; the spectral limiter takes out abnormal excess where it
        // is, so the compressor after it only reacts to what is loud across the whole programme.
        auto lumenSettings = p.lumen;
        lumenSettings.holdGains = p.limiter.active && limiter.isHandlingLocalisedEvent();
        lumenSettings.levelDb = levelDbNow;   // LEVEL moves the whole rack; the leveler reads levels relative to it
        lumen.process (chunk, chans, n, lumenSettings);
        limiter.process (chunk, chans, n, p.limiter);

        // MIX BALANCER, with taps either side of it for its display
        scopeBalIn.push (chunk, chans, n);
        balancer.process (chunk, chans, n, p.balancer);
        scopeBalOut.push (chunk, chans, n);

        tide.process (chunk, chans, n, p.tide, p.limiter.active ? limiter.key() : nullptr);
        seraph.process (chunk, chans, n, p.seraph);

        // The output limiter: full scale is looked after here, once, cleanly
        output.process (chunk, chans, n);
    }
}
