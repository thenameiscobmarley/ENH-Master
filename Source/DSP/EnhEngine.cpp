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
        seraph.reset();

        samplesToTick = controlInterval;
        planCountdown = 0;
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

        // Hosts may exceed the prepared block size: process in chunks the oversampler accepts.
        for (int start = 0; start < buffer.getNumSamples(); start += maxBlock)
            processChunk (buffer, start, std::min (maxBlock, buffer.getNumSamples() - start), p);

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

        float* seraphChannels[2] { write[0] + start, write[chans - 1] + start };
        seraph.process (seraphChannels, chans, n, p.seraph);
    }
}
