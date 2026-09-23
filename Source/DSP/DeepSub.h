#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include "DspMath.h"

namespace enh::dsp
{
    /** DEEP SUB - a sub-harmonic synthesiser and a resonant hull, for low end that is felt as much as heard:
        deep, dark and huge, like standing inside a steel hull under water.

        1. SUB (DEPTH): the bass line is tracked note by note (zero crossings of the band below ~110 Hz,
           with hysteresis and a confidence that only builds on a steady pitch) and a new tone is generated
           an octave below it, following the bass's own envelope. Notes whose octave-down would fall under
           ~30 Hz (where nothing reproduces it) are reinforced at their own pitch instead, blended smoothly.
        2. HULL: a bank of six long-ringing low resonances at the inharmonic ratios of a steel shell
           (1, 1.52, 2.03, 2.71, 3.39, 4.43 x a base of 22 - 48 Hz set by SIZE), struck by the bass and the
           generated sub. Each mode drifts very slowly (a fraction of a percent, as a hull under pressure),
           and a dark rumble breathes under it while it rings. MATERIAL sets how long it rings.
        3. PRESSURE: weight you can feel on small speakers too - a low shelf on the programme and a soft
           saturation of what is generated, which adds the upper harmonics that make a sub audible.

        Safe by construction: everything generated is mono, high-passed at 18 Hz, and turned down when the
        programme itself is already near full scale, so it never drives the limiters into pumping. At DEPTH,
        HULL and PRESSURE 0 the audio is not touched at all. Zero latency. Real-time safe after prepare().
    */
    class DeepSub
    {
    public:
        struct Settings
        {
            float depth = 0.0f;      // 0..1  the octave-down sub
            float hull = 0.0f;       // 0..1  the hull's resonance
            float size = 0.5f;       // 0..1  small boat .. vast hull (the resonances' base 48 .. 22 Hz)
            float pressure = 0.0f;   // 0..1  weight: low shelf and harmonic saturation
            bool active = false;
            int shape = 0;           // methods (MethodRegistry.h): SUB SHAPE 0 sine, 1 warm, 2 growl
            int tracking = 0;        // TRACKING 0 standard (30 ms glide), 1 fast (8 ms), 2 stable (90 ms)
            int material = 0;        // HULL MATERIAL 0 steel, 1 iron, 2 cavern
        };

        static constexpr int numModes = 6;

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            hullK = onePole (0.010, sr);
            bandLp = BiquadCoeffs::lowPass (sr, 150.0, 0.7071);
            bandHp = BiquadCoeffs::highPass (sr, 25.0, 0.7071);
            trackLp = BiquadCoeffs::lowPass (sr, 110.0, 0.7071);
            rumbleLp = BiquadCoeffs::lowPass (sr, 55.0, 0.7071);
            subsonic = BiquadCoeffs::highPass (sr, 18.0, 0.7071);
            envAttack = coef (0.005);
            envRelease = coef (0.120);
            slowEnvK = coef (0.600);
            peakRelease = coef (0.200);
            genMeterK = coef (0.300);
            fadeLength = std::max (1, (int) std::lround (0.030 * sr));
            designedPressure = -1.0f;
            reset();
        }

        void reset()
        {
            for (auto* s : { &lp1, &lp2, &hp, &track, &rumble, &sub1, &subsonicState })
                s->reset();
            for (auto& c : shelf) c.reset();
            modes = {};
            env = slowEnv = peakEnv = genMeter = 0.0f;
            hullNow = 0.0f;
            freq = targetFreq = 55.0f;
            confidence = 0.0f;
            phaseFull = phaseHalf = 0.0f;
            armed = false;
            sinceCrossing = 0;
            wet = 0.0f;
            shape = 0; fadingShape = -1; shapeFadeLeft = 0;
            modeTick = 0;
            lfoPhase = {};
            noise = 12345u;
        }

        void process (float* const* ch, int numChannels, int n, const Settings& s) noexcept
        {
            const int chans = std::min (2, numChannels);
            if (chans <= 0 || n <= 0)
                return;

            const float depth = std::clamp (s.depth, 0.0f, 1.0f), hullAmount = std::clamp (s.hull, 0.0f, 1.0f),
                        pressure = std::clamp (s.pressure, 0.0f, 1.0f);
            const float wetTarget = s.active && (depth > 0.0f || hullAmount > 0.0f || pressure > 0.0f) ? 1.0f : 0.0f;
            if (wetTarget == 0.0f && wet == 0.0f)
            {
                genMeter = 0.0f;
                return;   // out, or nothing set: not touched at all
            }

            // Methods
            if (const int want = std::clamp (s.shape, 0, 2); want != shape)
            {
                fadingShape = shape;
                shape = want;
                shapeFadeLeft = fadeLength;
            }
            const float glideS = s.tracking == 1 ? 0.008f : s.tracking == 2 ? 0.090f : 0.030f;
            const float kGlide = coef (glideS);
            const float decayScale = s.material == 1 ? 0.55f : s.material == 2 ? 0.32f : 1.0f;   // steel rings longest
            const float brightness = s.material == 1 ? 0.75f : s.material == 2 ? 1.25f : 1.0f;   // cavern: denser upper modes

            // PRESSURE's low shelf on the programme (redesigned when the knob moves)
            if (std::abs (pressure - designedPressure) > 0.002f)
            {
                shelfCoeffs = BiquadCoeffs::lowShelf (sr, 55.0, 0.7071, 6.0 * pressure * wetTarget);
                designedPressure = pressure;
            }

            const float baseHz = 48.0f * std::pow (22.0f / 48.0f, std::clamp (s.size, 0.0f, 1.0f));
            constexpr std::array<float, numModes> ratio { 1.00f, 1.52f, 2.03f, 2.71f, 3.39f, 4.43f };
            constexpr std::array<float, numModes> modeGain { 1.00f, 0.80f, 0.62f, 0.45f, 0.33f, 0.22f };
            constexpr std::array<float, numModes> decayS { 3.2f, 2.8f, 2.3f, 1.9f, 1.5f, 1.1f };
            constexpr std::array<float, numModes> lfoHz { 0.050f, 0.071f, 0.093f, 0.117f, 0.139f, 0.161f };
            const float depthGain = 1.4f * depth * std::sqrt (depth);
            const float fadeStep = 1.0f / (float) fadeLength;
            const float wetStep = 1.0f / (float) fadeLength;
            const float drive = 1.0f + 4.0f * pressure;
            const float twoPi = 6.28318531f;

            for (int i = 0; i < n; ++i)
            {
                const float dryL = ch[0][i], dryR = chans == 2 ? ch[1][i] : dryL;
                const float mono = 0.5f * (dryL + dryR);
                wet += std::clamp (wetTarget - wet, -wetStep, wetStep);

                // The bass band, its envelope and the programme's peak
                const float b = hp.process (bandHp, lp2.process (bandLp, lp1.process (bandLp, mono)));
                const float a = std::abs (b);
                env += (a - env) * (a > env ? envAttack : envRelease);
                slowEnv += (env - slowEnv) * slowEnvK;
                const float pk = std::max (std::abs (dryL), std::abs (dryR));
                peakEnv = pk > peakEnv ? pk : peakEnv + (pk - peakEnv) * peakRelease;

                // --- 1. track the bass note: positive-going zero crossings, with hysteresis
                const float t = track.process (trackLp, b);
                const float h = 0.15f * env + 1.0e-5f;
                ++sinceCrossing;
                if (! armed && t < -h)
                    armed = true;
                else if (armed && t > h)
                {
                    armed = false;
                    const float f = (float) sr / (float) std::max (1, sinceCrossing);
                    sinceCrossing = 0;
                    if (f >= 25.0f && f <= 180.0f)
                    {
                        const bool steady = std::abs (f - targetFreq) < 0.15f * targetFreq;
                        confidence += ((steady ? 1.0f : 0.35f) - confidence) * 0.35f;
                        targetFreq = f;
                    }
                    else
                        confidence *= 0.6f;
                }
                if (sinceCrossing > (int) (0.08 * sr))
                    confidence *= 0.9995f;   // no note: it fades rather than holding a stale pitch
                freq += (targetFreq - freq) * kGlide;

                // --- the sub: an octave below, or the note itself when that would be too deep to hear
                const float subHz = 0.5f * freq;
                const float octaveDown = smoothstep (26.0f, 34.0f, subHz);
                phaseHalf += twoPi * subHz / (float) sr;
                phaseFull += twoPi * freq / (float) sr;
                if (phaseHalf > twoPi) phaseHalf -= twoPi;
                if (phaseFull > twoPi) phaseFull -= twoPi;
                auto voice = [&] (int shapeIndex)
                {
                    return octaveDown * shapeOf (shapeIndex, phaseHalf) + (1.0f - octaveDown) * shapeOf (shapeIndex, phaseFull);
                };
                float tone = voice (shape);
                if (shapeFadeLeft > 0)
                {
                    const float old = voice (fadingShape);
                    tone = old + (tone - old) * (1.0f - (float) --shapeFadeLeft * fadeStep);
                }
                const float subOut = sub1.process (bandLp, tone * env * confidence * depthGain);   // no edges

                // --- 2. the hull: long-ringing modes struck by the bass and the sub
                if (--modeTick <= 0)
                {
                    modeTick = 32;   // coefficients (and their slow drift) every 32 samples
                    for (int m = 0; m < numModes; ++m)
                    {
                        auto& md = modes[(size_t) m];
                        lfoPhase[(size_t) m] += twoPi * lfoHz[(size_t) m] * 32.0f / (float) sr;
                        if (lfoPhase[(size_t) m] > twoPi) lfoPhase[(size_t) m] -= twoPi;
                        const float hz = baseHz * ratio[(size_t) m] * (1.0f + 0.004f * std::sin (lfoPhase[(size_t) m]));
                        const float decay = decayS[(size_t) m] * decayScale * (0.6f + 0.8f * std::clamp (s.size, 0.0f, 1.0f));
                        const float r = std::exp (-6.9077553f / (decay * (float) sr));
                        md.a1 = 2.0f * r * std::cos (twoPi * hz / (float) sr);
                        md.a2 = r * r;
                        md.b0 = 0.5f * (1.0f - r * r);
                    }
                }
                const float strike = b + 0.6f * subOut;
                float hull = 0.0f;
                for (int m = 0; m < numModes; ++m)
                {
                    auto& md = modes[(size_t) m];
                    const float y = md.b0 * (strike - md.x2) + md.a1 * md.y1 - md.a2 * md.y2;
                    md.x2 = md.x1; md.x1 = strike;
                    md.y2 = md.y1; md.y1 = y;
                    hull += y * modeGain[(size_t) m] * (m >= 3 ? brightness : 1.0f);
                }
                // ... and a dark rumble breathing under it while it rings
                noise = noise * 1664525u + 1013904223u;
                const float white = (float) (noise >> 8) / 8388608.0f - 1.0f;
                const float rumbleOut = rumble.process (rumbleLp, white) * slowEnv * 0.9f;
                // HULL glides (10 ms): turned down in one go it used to cut the ringing off (a click)
                hullNow = hullAmount + (hullNow - hullAmount) * hullK;
                const float hullOut = (5.0f * hull + rumbleOut) * hullNow;

                // --- 3. PRESSURE: harmonics that make the generated low end audible on small speakers
                float gen = subOut + hullOut;
                if (pressure > 0.0f)
                    gen = gen + (std::tanh (gen * drive) / drive - gen) * 0.6f + pressure * 0.35f * std::tanh (3.0f * gen) * std::abs (gen);

                // Safety: mono, above 18 Hz, and less of it when the programme is already near full scale
                const float room = std::clamp ((0.9f - peakEnv) / 0.45f, 0.35f, 1.0f);
                gen = subsonicState.process (subsonic, gen) * room * wet;
                genMeter += (gen * gen - genMeter) * genMeterK;

                ch[0][i] = shelf[0].process (shelfCoeffs, dryL) * 1.0f + gen;
                if (chans == 2)
                    ch[1][i] = shelf[1].process (shelfCoeffs, dryR) + gen;
            }

            if (wetTarget == 0.0f && wet == 0.0f)
                reset();   // fully out: start clean next time
        }

        /** What it is adding, RMS (dBFS), for the meter. */
        float getGeneratedDb() const noexcept { return 10.0f * std::log10 (std::max (1.0e-12f, genMeter)); }
        /** The bass note it is tracking (Hz), and how sure it is (0..1). */
        float getPitchHz() const noexcept     { return freq; }
        float getConfidence() const noexcept  { return confidence; }

    private:
        struct Mode { float a1 = 0.0f, a2 = 0.0f, b0 = 0.0f, x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f; };

        float coef (double seconds) const noexcept { return 1.0f - (float) std::exp (-1.0 / (seconds * sr)); }
        static float smoothstep (float e0, float e1, float x) noexcept
        {
            const float t = std::clamp ((x - e0) / (e1 - e0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
        /** SUB SHAPE: sine; warm (soft-saturated: 3rd harmonic, audible on small speakers); growl (near-square). */
        static float shapeOf (int shapeIndex, float phase) noexcept
        {
            const float s = std::sin (phase);
            switch (shapeIndex)
            {
                case 1:  return std::tanh (1.8f * s) * 1.0564f;    // / tanh (1.8)
                case 2:  return std::tanh (5.0f * s) * 1.0001f;    // / tanh (5)
                default: return s;
            }
        }

        double sr = 48000.0;
        BiquadCoeffs bandLp, bandHp, trackLp, rumbleLp, subsonic, shelfCoeffs;
        BiquadState lp1, lp2, hp, track, rumble, sub1, subsonicState;
        std::array<BiquadState, 2> shelf {};
        std::array<Mode, numModes> modes {};
        std::array<float, numModes> lfoPhase {};
        float envAttack = 0.0f, envRelease = 0.0f, slowEnvK = 0.0f, peakRelease = 0.0f, genMeterK = 0.0f;
        float env = 0.0f, slowEnv = 0.0f, peakEnv = 0.0f, genMeter = 0.0f;
        float freq = 55.0f, targetFreq = 55.0f, confidence = 0.0f, phaseFull = 0.0f, phaseHalf = 0.0f;
        float wet = 0.0f, designedPressure = -1.0f;
        float hullNow = 0.0f, hullK = 0.999f;   // HULL, gliding
        bool armed = false;
        int sinceCrossing = 0, modeTick = 0, fadeLength = 1440;
        int shape = 0, fadingShape = -1, shapeFadeLeft = 0;
        unsigned int noise = 12345u;
    };
}
