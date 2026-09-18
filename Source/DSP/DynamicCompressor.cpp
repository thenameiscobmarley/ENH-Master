#include "DynamicCompressor.h"
#include "DspMath.h"

#include <cmath>
#include <algorithm>

namespace enh::dsp
{
    static inline float toDb (float x) noexcept { return 20.0f * std::log10 (std::max (1.0e-7f, x)); }
    static inline float fromDb (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

    void DynamicCompressor::prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        channels = std::max (1, numChannels);
        scHp = BiquadCoeffs::highPass (sr, 90.0, 0.7071);
        reset();
    }

    void DynamicCompressor::reset()
    {
        fastRms = slowRms = peakEnv = 1.0e-6f;
        loudEstimateDb = -18.0f;
        tilt = 0.0f;
        lowEnergy = highEnergy = lowState = highState = 0.0f;
        onsetRate = lastFlux = 0.0f;
        thresholdDb = -20.0f;
        ratio = 2.0f;
        gainDb = makeupDb = slowDb = fastDb = 0.0f;
        for (auto& st : scState) st.reset();
        controlPhase = 0.0f;
        readout = {};
    }

    /** One control tick (~1.5 kHz): everything the user does not have a knob for. */
    void DynamicCompressor::updateDetector (float peak, float rms, const Settings& s) noexcept
    {
        const float dt = 1.0f / 1500.0f;
        const float resp = std::clamp (s.response, 0.0f, 1.0f);

        // Two loudness windows: "now" and "the last few seconds". The slow one is what the
        // threshold rides on, so a quiet passage does not drag the threshold down with it.
        const float fastTau = 0.030f, slowTau = 1.2f - 0.7f * resp;
        fastRms += (rms - fastRms) * std::min (1.0f, dt / fastTau);
        slowRms += (rms - slowRms) * std::min (1.0f, dt / slowTau);
        peakEnv = peak > peakEnv ? peak : peakEnv + (peak - peakEnv) * std::min (1.0f, dt / 0.25f);

        const float fastDb = toDb (fastRms), slowDb = toDb (slowRms);

        // Percentile-style tracker: creeps up quickly toward loud material and decays slowly,
        // so it ends up sitting on the loud part of the programme rather than its average.
        const float up = 1.0f - std::exp (-dt / (0.15f + 0.25f * (1.0f - resp)));
        const float down = 1.0f - std::exp (-dt / (3.0f - 1.5f * resp));
        loudEstimateDb += (fastDb > loudEstimateDb ? up : down) * (fastDb - loudEstimateDb);
        loudEstimateDb = std::clamp (loudEstimateDb, -70.0f, 6.0f);

        // Crest: peaky material keeps its transients, dense material gets held down harder.
        const float crest = std::clamp (peakEnv / std::max (1.0e-6f, fastRms), 1.0f, 12.0f);

        // Spectral tilt from the two one-pole splits, -1 (dark) .. +1 (bright)
        const float lowDb = toDb (std::sqrt (std::max (0.0f, lowEnergy)));
        const float highDb = toDb (std::sqrt (std::max (0.0f, highEnergy)));
        tilt += (std::clamp ((highDb - lowDb) / 24.0f, -1.0f, 1.0f) - tilt) * std::min (1.0f, dt / 0.35f);

        // Transient density: how busy the material is, which is what release has to keep up with.
        onsetRate += (lastFlux - onsetRate) * std::min (1.0f, dt / 0.5f);
        lastFlux *= 0.995f;

        // --- the threshold itself -------------------------------------------------------
        // Sit under the loud part of the programme by an amount that depends on how peaky and
        // how busy it is. Peaky -> sit higher (catch only the peaks); dense -> sit lower.
        const float crestOffset = 2.0f + 1.6f * (crest - 1.0f);           // 2 .. ~20 dB
        const float densityOffset = -3.0f * std::clamp (onsetRate * 8.0f, 0.0f, 1.0f);
        const float tiltOffset = -2.0f * std::max (0.0f, -tilt);          // dark material: a touch lower
        const float depth = 3.0f + 9.0f * resp;                           // how far under the programme
        const float target = loudEstimateDb - depth + crestOffset + densityOffset + tiltOffset;

        const float follow = 1.0f - std::exp (-dt / (0.35f - 0.25f * resp));
        const float moved = target - thresholdDb;
        thresholdDb += follow * moved;
        thresholdDb = std::clamp (thresholdDb, -60.0f, 0.0f);

        // Ratio and knee follow the same picture: gentle and wide on peaky material,
        // firmer and tighter on dense material.
        const float denseness = std::clamp (1.0f - (crest - 1.0f) / 6.0f, 0.0f, 1.0f);
        const float targetRatio = 1.4f + (1.2f + 4.0f * resp) * denseness;
        ratio += (targetRatio - ratio) * std::min (1.0f, dt / 0.5f);
        kneeDb = 10.0f - 5.0f * denseness;

        // Ballistics. Attack keeps hold of transients on peaky material; release follows the
        // programme so busy material recovers quickly and sustained material does not pump.
        const float attackMs = (14.0f - 11.0f * resp) * (0.5f + 0.5f * crest / 6.0f);
        const float releaseMs = (420.0f - 260.0f * resp) * (1.0f - 0.45f * std::clamp (onsetRate * 8.0f, 0.0f, 1.0f));
        attackCoeff = std::exp (-1.0f / (0.001f * std::max (0.5f, attackMs) * (float) sr));
        releaseCoeff = std::exp (-1.0f / (0.001f * std::max (20.0f, releaseMs) * (float) sr));
        slowAttackCoeff = std::exp (-1.0f / (0.001f * std::max (40.0f, attackMs * 20.0f) * (float) sr));
        slowReleaseCoeff = std::exp (-1.0f / (0.001f * std::max (50.0f, releaseMs * 2.5f) * (float) sr));
        fastReleaseCoeff = std::exp (-1.0f / (0.050f * (float) sr));

        readout.thresholdDb = thresholdDb;
        readout.ratio = ratio;
        readout.crest = crest;
        readout.inputDb = fastDb;
        readout.adaptivity = std::clamp (std::abs (moved) * 0.35f, 0.0f, 1.0f);

        (void) slowDb;
    }

    void DynamicCompressor::process (float* const* data, int numChannels, int numSamples, const Settings& s,
                                     const float* const* key) noexcept
    {
        const int n = numSamples;
        const int ch = std::min (channels, numChannels);
        if (n <= 0 || ch <= 0)
            return;

        if (! s.active || s.mix <= 0.0001f)
        {
            // Keep the detector warm so switching back in does not jump
            readout.gainReductionDb += (0.0f - readout.gainReductionDb) * 0.2f;
            return;
        }

        const float mix = std::clamp (s.mix, 0.0f, 1.0f);
        const float controlStep = (float) (1500.0 / sr);

        float outPeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // Detector input: the loudest channel, so a hard-panned hit still triggers it
            float mono = 0.0f, peak = 0.0f;
            for (int c = 0; c < ch; ++c)
            {
                const float x = scState[(size_t) std::min (c, 1)].process (scHp, key != nullptr ? key[c][i] : data[c][i]);
                mono += x;
                peak = std::max (peak, std::abs (x));
            }
            mono /= (float) ch;

            // Cheap two-band split for tilt, and a flux measure for transient density
            lowState += (mono - lowState) * 0.02f;             // ~150 Hz at 48k
            const float high = mono - lowState;
            lowEnergy += (lowState * lowState - lowEnergy) * 0.001f;
            highEnergy += (high * high - highEnergy) * 0.001f;
            highState = high;

            const float rms = std::sqrt (std::max (1.0e-12f, lowEnergy + highEnergy));
            const float flux = std::max (0.0f, peak - peakEnv);
            lastFlux = std::max (lastFlux, flux);

            controlPhase += controlStep;
            if (controlPhase >= 1.0f)
            {
                controlPhase -= 1.0f;
                updateDetector (peak, rms, s);
            }

            // Gain computer, dB domain, soft knee
            const float levelDb = toDb (std::max (peak, rms * 1.4f));
            const float over = levelDb - thresholdDb;
            float targetGainDb = 0.0f;

            if (over > 0.5f * kneeDb)
                targetGainDb = -(over - over / ratio);
            else if (over > -0.5f * kneeDb && kneeDb > 0.0f)
            {
                const float t = over + 0.5f * kneeDb;
                targetGainDb = -((1.0f - 1.0f / ratio) * t * t) / (2.0f * kneeDb);
            }

            // Ballistics: fast down, programme-dependent up
            if (dualRelease)
            {
                // Slow: the average reduction. Fast: only what a transient needs beyond it, released quickly.
                slowDb = targetGainDb + (slowDb - targetGainDb) * (targetGainDb < slowDb ? slowAttackCoeff : slowReleaseCoeff);
                const float residual = std::min (0.0f, targetGainDb - slowDb);
                fastDb = residual + (fastDb - residual) * (residual < fastDb ? attackCoeff : fastReleaseCoeff);
                gainDb = slowDb + fastDb;
            }
            else
            {
                const float coeff = targetGainDb < gainDb ? attackCoeff : releaseCoeff;
                gainDb = targetGainDb + (gainDb - targetGainDb) * coeff;
            }

            // Auto make-up: give back most of what is being taken, so MIX is level-matched
            const float wantedMakeup = -gainDb * 0.65f;
            makeupDb += (wantedMakeup - makeupDb) * 0.0005f;

            const float wetGain = fromDb (gainDb + makeupDb);

            for (int c = 0; c < ch; ++c)
            {
                const float dry = data[c][i];
                const float wet = dry * wetGain;
                const float y = dry + (wet - dry) * mix;
                data[c][i] = y;
                outPeak = std::max (outPeak, std::abs (y));
            }
        }

        readout.gainReductionDb = -gainDb;
        readout.outputDb = toDb (outPeak);
    }
}
