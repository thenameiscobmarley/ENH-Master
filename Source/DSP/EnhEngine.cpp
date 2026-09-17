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
        tide.prepare (sr, numChannels);
        seraph.prepare (sr);
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
        tide.reset();
        seraph.reset();

        samplesToTick = controlInterval;
        planCountdown = 0;
        safetyGain = 1.0f;
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

        // Analyser tap, before anything: the audio thread only copies samples.
        scopeIn.push (buffer.getArrayOfReadPointers(), std::min (2, buffer.getNumChannels()), buffer.getNumSamples());

        // Hosts may exceed the prepared block size: process in chunks the oversampler accepts.
        for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
            processChunk (buffer, start, std::min (maxBlock, buffer.getNumSamples() - start), p);

        scopeOut.push (buffer.getArrayOfReadPointers(), std::min (2, buffer.getNumChannels()), buffer.getNumSamples());

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
                samplesToTick = controlInterval;
            }

            eq.process (write, chans, at, seg);

            sub.process (write, chans, at, seg);
            pos += seg;
        }

        juce::dsp::AudioBlock<float> block (write, (size_t) chans, (size_t) start, (size_t) n);
        analog.process (block, { boost, transient, p.footstep ? steps.getConfidence() : 0.0f, planner.depth, planner.clarity, strength });

        float* chunk[2] { write[0] + start, write[chans - 1] + start };

        // LUMEN lifts what is too quiet, TIDE holds down what is too loud, SERAPH finishes.
        lumen.process (chunk, chans, n, p.lumen);
        tide.process (chunk, chans, n, p.tide);
        seraph.process (chunk, chans, n, p.seraph);

        // Safety limiter: instant gain-down, slow recovery, then a soft clip. Zero latency, so it
        // cannot catch a single sample perfectly - the soft clip is what handles that.
        constexpr float ceiling = 0.98f;
        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int c = 0; c < chans; ++c)
                peak = std::max (peak, std::abs (chunk[c][i]));

            const float needed = peak * safetyGain > ceiling ? ceiling / std::max (1.0e-6f, peak) : 1.0f;
            safetyGain = needed < safetyGain ? needed : needed + (safetyGain - needed) * 0.9999f;

            for (int c = 0; c < chans; ++c)
            {
                const float y = chunk[c][i] * safetyGain;
                chunk[c][i] = std::abs (y) <= ceiling ? y : std::copysign (ceiling + (1.0f - ceiling) * std::tanh ((std::abs (y) - ceiling) / (1.0f - ceiling)), y);
            }
        }
    }
}
