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
        fadeLength = std::max (1, (int) std::lround (0.030 * sr));
        rmsWindowCoeff = 1.0f - (float) std::exp (-1.0 / (0.050 * sr));
        kwtShelf = BiquadCoeffs::highShelf (sr, 1500.0, 0.7071, 4.0);
        scHp150 = BiquadCoeffs::highPass (sr, 150.0, 0.7071);
        cellHeatK = 1.0f - (float) std::exp (-1.0 / (1.5 * sr));
        reset();
    }

    void DynamicCompressor::reset()
    {
        fastRms = slowRms = peakEnv = 1.0e-6f;
        loudEstimateDb = -18.0f;
        tilt = 0.0f;
        lowEnergy = highEnergy = 0.0f;
        lowState = {};
        onsetRate = lastFlux = 0.0f;
        thresholdDb = -20.0f;
        ratio = 2.0f;
        gainDb = makeupDb = slowDb = fastDb = singleDb = 0.0f;
        cellHeat = 0.0f;
        cellLag = {};
        rmsWindowEnergy = kwtEnergy = optoDb = 0.0f;
        for (auto& k : kwtState) k.reset();
        for (auto& st : scState150) st.reset();
        fadingSideChain = fadingGain = -1;
        sideChainFadeLeft = gainFadeLeft = 0;
        fadingDetector = fadingSmoothing = -1;
        detectorFadeLeft = smoothingFadeLeft = 0;
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
        // A gap is not programme: while the material is 20 dB or more under what this tracker has
        // learned, it holds instead of falling. It used to walk all the way down through a pause and
        // then clamp the music by 20 dB for a second or two when it came back in.
        const float up = 1.0f - std::exp (-dt / (0.15f + 0.25f * (1.0f - resp)));
        const float down = 1.0f - std::exp (-dt / (3.0f - 1.5f * resp));
        const bool inGap = fastDb < loudEstimateDb - 20.0f;
        if (fastDb > loudEstimateDb)
            loudEstimateDb += up * (fastDb - loudEstimateDb);
        else if (! inGap)
            loudEstimateDb += down * (fastDb - loudEstimateDb);
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

    //==============================================================================
    // Stage 1, detector: the level the gain computer sees
    float DynamicCompressor::detectLevel (int method, float peak, float rms) const noexcept
    {
        switch (method)
        {
            case detectorRms: return std::sqrt (rmsWindowEnergy) * 1.4f;  // RMS: 50 ms power only (a sine reads as PKR does)
            case detectorKwt: return std::sqrt (kwtEnergy) * 1.4f;        // KWT: the same, K-weighted
            default:          return std::max (peak, rms * 1.4f);         // PKR: whichever is higher, peak or RMS
        }
    }

    // Stage 3, smoothing: target gain -> applied gain
    // Stage 2, gain computer: level -> target gain (dB, <= 0), soft knee
    float DynamicCompressor::gainFor (int method, float levelDb) const noexcept
    {
        float r = ratio, knee = kneeDb;
        if (method == gainSoft)      { r = 1.0f + (ratio - 1.0f) * 0.6f; knee = kneeDb + 6.0f; }
        else if (method == gainHard) { r = 1.0f + (ratio - 1.0f) * 1.5f; knee = kneeDb * 0.33f; }

        const float over = levelDb - thresholdDb;
        float targetGainDb = 0.0f;
        if (over > 0.5f * knee)
            targetGainDb = -(over - over / r);
        else if (over > -0.5f * knee && knee > 0.0f)
        {
            const float t = over + 0.5f * knee;
            targetGainDb = -((1.0f - 1.0f / r) * t * t) / (2.0f * knee);
        }
        return targetGainDb;
    }

    float DynamicCompressor::smooth (int method, float targetGainDb) noexcept
    {
        if (method == smoothingOpto)
        {
            // An optical cell: quick back from light reduction, slower and slower from deep reduction
            const float depth = std::clamp (-optoDb / 8.0f, 0.0f, 1.0f);
            const float rel = releaseCoeff + (slowReleaseCoeff - releaseCoeff) * depth;
            optoDb = targetGainDb + (optoDb - targetGainDb) * (targetGainDb < optoDb ? attackCoeff : rel);
            return optoDb;
        }
        if (method == smoothingSrl)
        {
            const float coeff = targetGainDb < singleDb ? attackCoeff : releaseCoeff;
            singleDb = targetGainDb + (singleDb - targetGainDb) * coeff;
            return singleDb;
        }

        // DRL. Slow: the average reduction. Fast: only what a transient needs beyond it, released quickly.
        slowDb = targetGainDb + (slowDb - targetGainDb) * (targetGainDb < slowDb ? slowAttackCoeff : slowReleaseCoeff);
        const float residual = std::min (0.0f, targetGainDb - slowDb);
        fastDb = residual + (fastDb - residual) * (residual < fastDb ? attackCoeff : fastReleaseCoeff);
        return slowDb + fastDb;
    }

    // A method coming in starts from the gain being applied, so the switch does not jump
    void DynamicCompressor::seedSmoothing (int method, float fromGainDb) noexcept
    {
        if (method == smoothingSrl)
            singleDb = fromGainDb;
        else if (method == smoothingOpto)
            optoDb = fromGainDb;
        else
        {
            slowDb = fromGainDb;
            fastDb = 0.0f;
        }
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

        // A method changed: the old one fades out over 30 ms while the new one fades in
        const int wantDetector = std::clamp (s.detector, 0, numDetectors - 1);
        const int wantSmoothing = dualRelease ? std::clamp (s.smoothing, 0, numSmoothings - 1) : (int) smoothingSrl;
        const int wantSideChain = std::clamp (s.sideChain, 0, numSideChains - 1);
        if (wantSideChain != sideChain)
        {
            if (wantSideChain == sideChain150)
                for (auto& st : scState150) st.reset();
            else if (wantSideChain == sideChain90)
                for (auto& st : scState) st.reset();
            fadingSideChain = sideChain;
            sideChain = wantSideChain;
            sideChainFadeLeft = fadeLength;
        }
        const int wantGain = std::clamp (s.gain, 0, numGains - 1);
        if (wantGain != gain)
        {
            fadingGain = gain;
            gain = wantGain;
            gainFadeLeft = fadeLength;
        }
        if (wantDetector != detector)
        {
            if (wantDetector == detectorRms && detector != detectorRms && fadingDetector != detectorRms)
                rmsWindowEnergy = std::max (0.0f, lowEnergy + highEnergy);   // starts from the level measured now
            if (wantDetector == detectorKwt && detector != detectorKwt && fadingDetector != detectorKwt)
            {
                kwtEnergy = std::max (0.0f, lowEnergy + highEnergy);
                for (auto& k : kwtState) k.reset();
            }
            fadingDetector = detector;
            detector = wantDetector;
            detectorFadeLeft = fadeLength;
        }
        if (wantSmoothing != smoothing)
        {
            fadingSmoothing = smoothing;
            smoothing = wantSmoothing;
            seedSmoothing (smoothing, gainDb);
            smoothingFadeLeft = fadeLength;
        }
        const float fadeStep = 1.0f / (float) fadeLength;

        const float mix = std::clamp (s.mix, 0.0f, 1.0f);
        const float controlStep = (float) (1500.0 / sr);

        float outPeak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // Detector input: the loudest channel, so a hard-panned hit still triggers it, through the
            // side-chain filter (crossfaded from the old one for 30 ms after a switch)
            // The level detectors (RMS, K-weighted, the tilt split) read each channel's own power and
            // average them, as loudness is measured (BS.1770 sums the channels' powers). The sum of the
            // channels read a wide mix 3 dB under a mono one at the same loudness - it was compressed
            // less - and anything out of phase not at all. A mono signal reads exactly as before.
            const int pc = std::min (ch, 2);
            std::array<float, 2> xs {}, oldXs {};
            float peak = 0.0f;
            auto sideChainInput = [&] (int method, std::array<float, 2>& out, float& peakOut)
            {
                for (int c = 0; c < ch; ++c)
                {
                    const float in = key != nullptr ? key[c][i] : data[c][i];
                    const float x = method == sideChain90 ? scState[(size_t) std::min (c, 1)].process (scHp, in)
                                  : method == sideChain150 ? scState150[(size_t) std::min (c, 1)].process (scHp150, in)
                                                           : in;
                    if (c < 2)
                        out[(size_t) c] = x;
                    peakOut = std::max (peakOut, std::abs (x));
                }
            };
            sideChainInput (sideChain, xs, peak);
            if (sideChainFadeLeft > 0)
            {
                float oldPeak = 0.0f;
                sideChainInput (fadingSideChain, oldXs, oldPeak);
                const float t = 1.0f - (float) --sideChainFadeLeft * fadeStep;
                for (int c = 0; c < pc; ++c)
                    xs[(size_t) c] = oldXs[(size_t) c] + (xs[(size_t) c] - oldXs[(size_t) c]) * t;
                peak = oldPeak + (peak - oldPeak) * t;
            }
            const float perChannel = 1.0f / (float) pc;

            // Cheap two-band split for tilt, and a flux measure for transient density
            float lowPower = 0.0f, highPower = 0.0f, power = 0.0f;
            for (int c = 0; c < pc; ++c)
            {
                auto& ls = lowState[(size_t) c];
                ls += (xs[(size_t) c] - ls) * 0.02f;             // ~150 Hz at 48k
                const float high = xs[(size_t) c] - ls;
                lowPower += ls * ls;
                highPower += high * high;
                power += xs[(size_t) c] * xs[(size_t) c];
            }
            lowEnergy += (lowPower * perChannel - lowEnergy) * 0.001f;
            highEnergy += (highPower * perChannel - highEnergy) * 0.001f;

            const float rms = std::sqrt (std::max (1.0e-12f, lowEnergy + highEnergy));
            if (detector == detectorRms || (detectorFadeLeft > 0 && fadingDetector == detectorRms))
                rmsWindowEnergy += (power * perChannel - rmsWindowEnergy) * rmsWindowCoeff;   // only while RMS is in use
            if (detector == detectorKwt || (detectorFadeLeft > 0 && fadingDetector == detectorKwt))
            {
                float kPower = 0.0f;
                for (int c = 0; c < pc; ++c)
                {
                    const float k = kwtState[(size_t) c].process (kwtShelf, xs[(size_t) c]);
                    kPower += k * k;
                }
                kwtEnergy += (kPower * perChannel - kwtEnergy) * rmsWindowCoeff;
            }
            const float flux = std::max (0.0f, peak - peakEnv);
            lastFlux = std::max (lastFlux, flux);

            controlPhase += controlStep;
            if (controlPhase >= 1.0f)
            {
                controlPhase -= 1.0f;
                updateDetector (peak, rms, s);
            }

            // 1. Detector
            float levelDb = toDb (detectLevel (detector, peak, rms));
            if (detectorFadeLeft > 0)
            {
                const float old = toDb (detectLevel (fadingDetector, peak, rms));
                levelDb = old + (levelDb - old) * (1.0f - (float) --detectorFadeLeft * fadeStep);
            }

            // 2. Gain computer (ADT), dB domain, soft knee
            float targetGainDb = gainFor (gain, levelDb);
            if (gainFadeLeft > 0)
            {
                const float old = gainFor (fadingGain, levelDb);
                targetGainDb = old + (targetGainDb - old) * (1.0f - (float) --gainFadeLeft * fadeStep);
            }

            // 3. Smoothing: fast down, programme-dependent up
            gainDb = smooth (smoothing, targetGainDb);
            if (smoothingFadeLeft > 0)
            {
                const float old = smooth (fadingSmoothing, targetGainDb);
                gainDb = old + (gainDb - old) * (1.0f - (float) --smoothingFadeLeft * fadeStep);
            }

            // Auto make-up: give back most of what is being taken, so MIX is level-matched
            const float wantedMakeup = -gainDb * (s.makeup == 1 ? 0.90f : s.makeup == 2 ? 0.0f : 0.65f);
            makeupDb += (wantedMakeup - makeupDb) * 0.0005f;

            const float wetGain = fromDb (gainDb + makeupDb);

            // The gain cell is not a perfect multiplier: the harder it works the more it bends the signal
            // (odd, gentle: a few tenths of a percent at heavy reduction), and after working hard for a while
            // it bends a little more for a moment (heat). With no gain reduction it is exact.
            const float grN = std::clamp (-gainDb / 12.0f, 0.0f, 1.0f);
            cellHeat += (grN - cellHeat) * cellHeatK;
            const float bend = 0.020f * grN * (0.7f + 0.6f * cellHeat);

            for (int c = 0; c < ch; ++c)
            {
                const float dry = data[c][i];
                float wet = dry * wetGain;
                if (bend > 0.0f)
                {
                    // (and it responds a hair late to steep edges: a touch of lag at heavy reduction only)
                    auto& lag = cellLag[(size_t) std::min (c, 1)];
                    lag += (wet - lag) * (1.0f - 0.25f * grN);
                    wet = lag - bend * lag * lag * lag / (1.0f + lag * lag);
                }
                else
                    cellLag[(size_t) std::min (c, 1)] = wet;
                const float y = dry + (wet - dry) * mix;
                data[c][i] = y;
                outPeak = std::max (outPeak, std::abs (y));
            }
        }

        readout.gainReductionDb = -gainDb;
        readout.outputDb = toDb (outPeak);
    }
}
