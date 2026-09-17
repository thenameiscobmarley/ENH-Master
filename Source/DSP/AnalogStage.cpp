#include "AnalogStage.h"

namespace enh::dsp
{
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
        bump      = BiquadCoeffs::lowShelf (sr, 45.0, 0.8, 0.6);
        air       = BiquadCoeffs::highShelf (sr, 11000.0, 0.7071, 0.0);
        airDb     = 0.0f;

        osHi      = BiquadCoeffs::highPass (osr, 2800.0, 0.7071);
        osHiPost  = BiquadCoeffs::highPass (osr, 2400.0, 0.7071);
        osLm      = BiquadCoeffs::bandPass (osr, 190.0, 0.9);
        osLmPost  = BiquadCoeffs::bandPass (osr, 420.0, 0.9);

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
        appliedGain = 1.0f;
        hiAmount = lmAmount = 0.0f;
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

        // --- base-rate colour ------------------------------------------------------------
        const float wantedAir = 2.5f * s.clarity;
        if (std::abs (wantedAir - airDb) > 0.02f)
        {
            air = BiquadCoeffs::highShelf (sr, 11000.0, 0.7071, wantedAir);
            airDb = wantedAir;
        }

        // Auto gain: slow loop driving output loudness toward input loudness
        const float inDb = powerToDb (inputK.meanSquare), outDb = powerToDb (outputK.meanSquare);
        if (inDb > -60.0f)
        {
            const float k = 1.0f - std::exp (-(float) n / (float) sr / 1.5f);
            // Match loudness, but let full CLARITY sit ~1.5 dB up: enhancement should be heard
            const float allowance = 1.5f * s.clarity;
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
                float y = b.subsonic.process (subsonic, x[i]);
                y = b.bump.process (bump, y);
                y = b.air.process (air, y);
                g += gainStep;
                x[i] = y * g;
            }
        }

        appliedGain = targetGain;

        // --- 2x oversampled non-linear stage ---------------------------------------------
        auto sub = block.getSubsetChannelBlock (0, (size_t) chans);
        auto up = oversampling->processSamplesUp (sub);
        const int un = (int) up.getNumSamples();

        const float hiTarget = s.clarity * (0.35f + 0.60f * s.transient) + 0.30f * s.footstep;
        const float lmTarget = s.clarity * 0.30f;
        const float hiStep = (hiTarget - hiAmount) / (float) un;
        const float lmStep = (lmTarget - lmAmount) / (float) un;

        constexpr float hiPre = 8.0f, hiBias = 0.10f, lmPre = 4.0f, lmBias = 0.08f;
        const float hiBiasOut = std::tanh (hiBias), lmBiasOut = std::tanh (lmBias);
        constexpr float c2 = 0.05f, c3 = 0.03f;
        constexpr float knee = 0.85f, ceiling = 0.977f;

        for (int c = 0; c < chans; ++c)
        {
            auto* x = up.getChannelPointer ((size_t) c);
            auto& o = os[(size_t) c];
            float hiAmt = hiAmount, lmAmt = lmAmount;

            for (int i = 0; i < un; ++i)
            {
                const float in = x[i];

                // Exciter: saturate isolated bands, keep only the generated harmonics
                const float hi = o.hi2.process (osHi, o.hi1.process (osHi, in));
                const float hiSat = (std::tanh (hiPre * hi + hiBias) - hiBiasOut) / hiPre;
                const float hiHarm = o.hiPost.process (osHiPost, hiSat - hi);

                const float lm = o.lm1.process (osLm, in);
                const float lmSat = (std::tanh (lmPre * lm + lmBias) - lmBiasOut) / lmPre;
                const float lmHarm = o.lmPost.process (osLmPost, lmSat - lm);

                hiAmt += hiStep;
                lmAmt += lmStep;
                float y = in + hiHarm * hiAmt * 6.0f + lmHarm * lmAmt * 5.0f;

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

        hiAmount = hiTarget;
        lmAmount = lmTarget;

        oversampling->processSamplesDown (sub);

        // Final base-rate safety: the down-sampling filter can overshoot the 2x-rate ceiling
        for (int c = 0; c < chans; ++c)
        {
            auto* x = sub.getChannelPointer ((size_t) c);
            for (int i = 0; i < n; ++i)
            {
                const float a = std::abs (x[i]);
                if (a > 0.94f)
                    x[i] = std::copysign (0.94f + 0.055f * std::tanh ((a - 0.94f) / 0.055f), x[i]);
            }
        }

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
