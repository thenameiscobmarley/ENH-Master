#include "AnalogStage.h"

namespace enh::dsp
{
    void AnalogStage::ExciterCoeffs::design (double osr, float hz) noexcept
    {
        pre  = SvfCoeffs::make (osr, hz, 1.3);
        post = SvfCoeffs::make (osr, 1.45 * hz, 0.7071);
        top  = SvfCoeffs::make (osr, std::min (4.2 * hz, 19000.0), 0.7071);
    }

    AnalogStage::Weights AnalogStage::weightsFor (int method) noexcept
    {
        switch (method)
        {
            case 1:  return { 0.90f, 0.08f, 0.80f, 0.12f };   // EVN: warm, round
            case 2:  return { 0.35f, 0.70f, 0.25f, 0.80f };   // ODD: edgy, forward
            default: return { 0.75f, 0.35f, 0.55f, 0.50f };   // CHB: body mostly even, definition even + odd
        }
    }

    inline float AnalogStage::excite (Exciter& e, const ExciterCoeffs& c, float in, float attack, float release, float settle,
                                      float w2, float w3) noexcept
    {
        const float x = e.pre2.process (c.pre, e.pre1.process (c.pre, in).band).band;

        // Peak envelope of the partial
        const float m = std::abs (x);
        e.env = (m > e.env ? attack : release) * (e.env - m) + m;
        const float amp = e.env + 1.0e-6f;

        // How new the partial is: 1 while it is starting (the slow envelope still below the fast one), 0 once
        // it has settled. Harmonics are what makes an attack cut through; on a held note or a steady tone they
        // are simply distortion - at ADD 3 - 4 a sustained 1 kHz tone came out with 13 - 28 % THD. So a settled
        // partial keeps a quarter of them, and every onset (a footstep, a consonant, a pick) keeps them all.
        e.settled = e.env > e.settled ? settle * (e.settled - e.env) + e.env : e.env;
        const float onset = std::clamp ((e.env - e.settled) / amp * 4.0f, 0.0f, 1.0f);
        const float keep = 0.25f + 0.75f * onset;

        // Normalised partial, soft-limited, and held to +-1 where the Chebyshev polynomials are meant to work:
        // a steady partial reaches ~0.83 here, but at an attack the envelope lags and v runs up to ~5, where
        // T3 is ~100x too big - the harmonics burst out at +66 % of the signal (heard as a crack on every hit)
        const float v = x / amp;
        const float u = std::clamp (v / (1.0f + 0.2f * std::abs (v)), -1.0f, 1.0f);
        const float h = (w2 * (2.0f * u * u - 1.0f) + w3 * (4.0f * u * u - 3.0f) * u) * amp;

        // Keep only what lies above the source: DC, the fundamental and IM products below go
        const float hp = e.post2.process (c.post, e.post1.process (c.post, h).high).high;
        const float gate = e.env / (e.env + 1.0e-4f);   // nothing on silence (~ -80 dBFS)
        return e.top.process (c.top, hp).low * gate * keep;
    }

    void AnalogStage::prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        sr = sampleRate;
        const int chans = std::clamp (numChannels, 1, maxChannels);

        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) chans, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, true);
        oversampling->initProcessing ((size_t) std::max (1, maxBlockSize));

        const double osr = sr * 2.0;

        kHp       = BiquadCoeffs::highPass (sr, 200.0, 0.7071); // ignore sub region: SUB is a deliberate lift
        kShelf    = BiquadCoeffs::highShelf (sr, 1500.0, 0.7071, 4.0);
        subsonic  = BiquadCoeffs::highPass (sr, 18.0, 0.7071);

        depthCoeffs.w2 = 0.75f;   depthCoeffs.w3 = 0.35f;    // body: mostly even
        clarityCoeffs.w2 = 0.55f; clarityCoeffs.w3 = 0.50f;  // definition: even + odd
        depthHz = clarityHz = 0.0f;
        weightsFrom = weightsTo = weightsFor (0);
        harmonicsMethod = 0;
        glideLen = std::max (1, (int) std::lround (0.030 * osr));
        glidePos = glideLen;

        envAttack = onePole (0.001, osr);
        envRelease = onePole (0.060, osr);
        envSettle = onePole (0.080, osr);
        msCoeff = onePole (0.4, sr);
        dcCoeff = (float) std::exp (-2.0 * pi * 8.0 / osr);
        reset();
    }

    void AnalogStage::reset()
    {
        if (oversampling != nullptr)
            oversampling->reset();

        inputK = outputK = KWeighting {};

        for (auto& b : base) b = BaseChannel {};
        for (auto& o : os)   o = OsChannel {};

        autoGainDb = 0.0f;
        inRefDb = -60.0f;
        appliedGain = 1.0f;
        depthMix = clarityMix = 0.0f;
        peakDb = -100.0f;
    }

    int AnalogStage::getLatencySamples() const noexcept
    {
        return oversampling != nullptr ? (int) std::lround (oversampling->getLatencyInSamples()) : 0;
    }

    void AnalogStage::measureInput (const float* const* ch, int numChannels, int n) noexcept
    {
        const int chans = std::min (numChannels, maxChannels);
        const float norm = 1.0f / (float) std::max (1, chans);

        for (int i = 0; i < n; ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < chans; ++c)
                mono += ch[c][i];

            const float k = inputK.shelf.process (kShelf, inputK.hp.process (kHp, mono * norm));
            inputK.meanSquare = msCoeff * inputK.meanSquare + (1.0f - msCoeff) * k * k;
        }
    }

    void AnalogStage::process (juce::dsp::AudioBlock<float> block, const Settings& s) noexcept
    {
        const int n = (int) block.getNumSamples();
        const int chans = std::min ((int) block.getNumChannels(), maxChannels);
        if (n == 0 || chans == 0)
            return;

        // Auto gain: slow loop driving output loudness toward input loudness
        const float inDb = powerToDb (inputK.meanSquare), outDb = powerToDb (outputK.meanSquare);
        // While the SPECTRAL LIMITER is handling a localised spike, loudness matching does not chase it:
        // it used to turn the whole mix down every time the sub enhancer lifted a bass hit.
        // A pause is not a new level to match: while the input is 12 dB or more under what it is when it
        // plays, the loop holds. Otherwise it walked up through every gap and the next note arrived loud.
        const float refK = inDb > inRefDb ? 1.0f - std::exp (-(float) n / (float) sr / 1.0f)
                                          : 1.0f - std::exp (-(float) n / (float) sr / 8.0f);
        inRefDb += refK * (inDb - inRefDb);
        if (inDb > -60.0f && ! s.holdLevel && inDb > inRefDb - 20.0f)
        {
            const float k = 1.0f - std::exp (-(float) n / (float) sr / 1.5f);
            // Match loudness (NORM); in ADD mode let full enhancement sit ~2 dB up so it is heard
            const float allowance = 2.0f * s.boost;
            autoGainDb = std::clamp (autoGainDb + (inDb + allowance - outDb) * k, -9.0f, 9.0f);
        }

        const float targetGain = dbToGain (autoGainDb);
        const float gainStep = (targetGain - appliedGain) / (float) n;

        for (int c = 0; c < chans; ++c)
        {
            auto* x = block.getChannelPointer ((size_t) c);
            auto& b = base[(size_t) c];
            float g = appliedGain;

            for (int i = 0; i < n; ++i)
            {
                g += gainStep;
                x[i] = b.subsonic.process (subsonic, x[i]) * g;
            }
        }

        appliedGain = targetGain;

        // --- 2x oversampled non-linear stage ---------------------------------------------
        const double osr = sr * 2.0;
        if (std::abs (s.depth.hz - depthHz) > 0.002f * depthHz)
        {
            depthCoeffs.design (osr, s.depth.hz);
            depthHz = s.depth.hz;
        }
        if (std::abs (s.clarityBand.hz - clarityHz) > 0.002f * clarityHz)
        {
            clarityCoeffs.design (osr, s.clarityBand.hz);
            clarityHz = s.clarityBand.hz;
        }

        auto sub = block.getSubsetChannelBlock (0, (size_t) chans);
        auto up = oversampling->processSamplesUp (sub);
        const int un = (int) up.getNumSamples();

        // Amounts: ADD x how useful the planner found harmonics there
        // ADD mode only; a floor keeps it audible even where the planner is unsure
        const float strength = std::clamp (s.strength, 0.0f, 5.0f);
        const float depthTarget = std::min (4.0f, 1.1f * s.boost * (0.5f + 0.5f * s.depth.amount) * strength);
        const float clarityTarget = std::min (4.0f, 1.05f * s.boost * (0.5f + 0.5f * s.clarityBand.amount) * (0.8f + 0.4f * s.transient)
                                                    * (1.0f + 0.4f * s.footstep) * strength);
        const float depthStep = (depthTarget - depthMix) / (float) un;
        const float clarityStep = (clarityTarget - clarityMix) / (float) un;
        const bool exciting = depthTarget + depthMix + clarityTarget + clarityMix > 1.0e-5f;

        // HARMONICS changed: glide from wherever the weights are now
        const int wantHarmonics = std::clamp (s.harmonics, 0, 2);
        if (wantHarmonics != harmonicsMethod)
        {
            const float t = (float) std::min (glidePos, glideLen) / (float) glideLen;
            weightsFrom = { weightsFrom.depthW2 + (weightsTo.depthW2 - weightsFrom.depthW2) * t,
                            weightsFrom.depthW3 + (weightsTo.depthW3 - weightsFrom.depthW3) * t,
                            weightsFrom.clarityW2 + (weightsTo.clarityW2 - weightsFrom.clarityW2) * t,
                            weightsFrom.clarityW3 + (weightsTo.clarityW3 - weightsFrom.clarityW3) * t };
            weightsTo = weightsFor (wantHarmonics);
            harmonicsMethod = wantHarmonics;
            glidePos = 0;
        }
        const bool gliding = glidePos < glideLen;
        const int glideStart = glidePos;
        glidePos = std::min (glideLen, glidePos + un);

        // Transformer / valve colour, scaled by STRENGTH (0 = none). No ceiling here: in floating point a
        // mid-chain clipper protects nothing, it only distorts loud bass. The final limiter at the end of
        // the chain looks after full scale; the guard below only keeps the colour curve from folding over.
        const float colour = std::clamp (s.strength, 0.0f, 2.0f);
        const float c2 = 0.03f * colour, c3 = 0.015f * colour;
        constexpr float knee = 1.8f, ceiling = 2.4f;

        for (int c = 0; c < chans; ++c)
        {
            auto* x = up.getChannelPointer ((size_t) c);
            auto& o = os[(size_t) c];
            float dMix = depthMix, cMix = clarityMix;
            float dW2 = weightsTo.depthW2, dW3 = weightsTo.depthW3, cW2 = weightsTo.clarityW2, cW3 = weightsTo.clarityW3;

            for (int i = 0; i < un; ++i)
            {
                const float in = x[i];
                float y = in;

                if (exciting)
                {
                    dMix += depthStep;
                    cMix += clarityStep;
                    if (gliding)
                    {
                        const float t = std::min (1.0f, (float) (glideStart + i) / (float) glideLen);
                        dW2 = weightsFrom.depthW2 + (weightsTo.depthW2 - weightsFrom.depthW2) * t;
                        dW3 = weightsFrom.depthW3 + (weightsTo.depthW3 - weightsFrom.depthW3) * t;
                        cW2 = weightsFrom.clarityW2 + (weightsTo.clarityW2 - weightsFrom.clarityW2) * t;
                        cW3 = weightsFrom.clarityW3 + (weightsTo.clarityW3 - weightsFrom.clarityW3) * t;
                    }
                    y += dMix * excite (o.depth, depthCoeffs, in, envAttack, envRelease, envSettle, dW2, dW3)
                       + cMix * excite (o.clarity, clarityCoeffs, in, envAttack, envRelease, envSettle, cW2, cW3);
                }

                // Transformer / valve colour
                y = y + c2 * y * y - c3 * y * y * y;

                // DC blocker (removes the even-order offset)
                const float dc = y - o.dcX + dcCoeff * o.dcY;
                o.dcX = y;
                o.dcY = dc;
                y = dc;

                // Soft ceiling
                const float a = std::abs (y);
                if (a > knee)
                    y = std::copysign (knee + (ceiling - knee) * std::tanh ((a - knee) / (ceiling - knee)), y);

                x[i] = y;
            }
        }

        depthMix = depthTarget;
        clarityMix = clarityTarget;

        oversampling->processSamplesDown (sub);

        // (No base-rate clip: full scale is the final limiter's job, at the end of the chain.)

        // --- output loudness + peak --------------------------------------------------------
        const float norm = 1.0f / (float) chans;
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < chans; ++c)
            {
                const float v = sub.getSample (c, i);
                mono += v;
                peak = std::max (peak, std::abs (v));
            }

            const float k = outputK.shelf.process (kShelf, outputK.hp.process (kHp, mono * norm));
            outputK.meanSquare = msCoeff * outputK.meanSquare + (1.0f - msCoeff) * k * k;
        }

        peakDb = std::max (20.0f * std::log10 (peak + 1.0e-9f), peakDb - 0.5f);
    }
}
