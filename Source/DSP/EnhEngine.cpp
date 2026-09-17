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
        steps.prepare (analyzer, controlRate);
        eq.prepare (sr, controlRate, analyzer, numChannels);
        sub.prepare (sr, controlRate);
        analog.prepare (sr, maxBlock, numChannels);

        thumpCoeffs = BiquadCoeffs::peaking (sr, 160.0, 0.9, 0.0);
        scuffCoeffs = BiquadCoeffs::peaking (sr, 3800.0, 1.0, 0.0);
        reset();
    }

    void EnhEngine::reset()
    {
        analyzer.reset();
        steps.reset();
        eq.reset();
        sub.reset();
        analog.reset();

        for (auto& f : focus) f = FocusChannel {};
        focusAmount = -1.0f;
        samplesToTick = controlInterval;
        transient = 0.0f;

        for (auto& g : meters.bandGainDb) g = 0.0f;
    }

    void EnhEngine::controlTick (const Parameters& p) noexcept
    {
        const float dt = controlDt;
        const float smooth = 1.0f - std::exp (-dt / 0.05f);
        clarity   += (p.clarity - clarity) * smooth;
        speed     += (p.adaptSpeed - speed) * smooth;
        subAmount += (p.sub - subAmount) * smooth;
        focusTarget += ((p.footstep ? 1.0f : 0.0f) - focusTarget) * smooth;

        analyzer.update (dt);
        const float confidence = steps.update (analyzer, dt);

        eq.update (analyzer, steps, { clarity, speed, p.footstep }, dt);
        sub.update ({ subAmount, p.subBoost, speed }, dt);

        // Static footstep regions: +2.5 dB thump, +3.5 dB scuff, faded with the switch
        if (std::abs (focusTarget - focusAmount) > 0.002f)
        {
            focusAmount = focusTarget;
            thumpCoeffs = BiquadCoeffs::peaking (sampleRate, 160.0, 0.9, 2.5 * focusAmount);
            scuffCoeffs = BiquadCoeffs::peaking (sampleRate, 3800.0, 1.0, 3.5 * focusAmount);
        }

        const float t = saturate01 ((analyzer.fullTransientDb - analyzer.fullShortDb) / 6.0f);
        transient = std::max (t, transient * std::exp (-dt / 0.08f));

        // Publish meters (atomics, wait-free)
        for (int k = 0; k < numBands; ++k)
            meters.bandGainDb[(size_t) k].store (eq.getGainDb (k), std::memory_order_relaxed);

        meters.footstepConfidence.store (p.footstep ? confidence : 0.0f, std::memory_order_relaxed);
        meters.enhancement.store (saturate01 (eq.getActivity() + 0.25f * clarity), std::memory_order_relaxed);
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
                sub.measure (mono);
            }

            samplesToTick -= seg;
            if (samplesToTick == 0)
            {
                controlTick (p);
                samplesToTick = controlInterval;
            }

            eq.process (write, chans, at, seg);

            if (focusAmount > 0.001f)
                for (int c = 0; c < chans; ++c)
                {
                    auto& f = focus[(size_t) c];
                    auto* x = write[c] + at;
                    for (int i = 0; i < seg; ++i)
                        x[i] = f.scuff.process (scuffCoeffs, f.thump.process (thumpCoeffs, x[i]));
                }

            sub.process (write, chans, at, seg);
            pos += seg;
        }

        juce::dsp::AudioBlock<float> block (write, (size_t) chans, (size_t) start, (size_t) n);
        analog.process (block, { clarity, transient, p.footstep ? steps.getConfidence() : 0.0f });
    }
}
