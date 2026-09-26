#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include "DspMath.h"

namespace enh::dsp
{
    /** LUNCHBOX: a small side rack of three 500-series style modules, in this order:

          CLASS-A EQ   a model of the classic British console channel EQ of the 1970s (the 1073 era):
                       an 18 dB/oct high-pass, the inductor low shelf with its resonant bump and the dip
                       above it, a broad LC mid bell whose peak sharpens as it is boosted (HI Q as on the
                       later units), a gentle first-order 12 kHz shelf, and IRON: the input and output
                       transformers and the class-A output stage around it.
          DE-HARSH     a split-band dynamic de-esser: it compares one band's level (peak-RMS) with the rest
                       of the sound and, only while the band sticks out, cuts that band alone with a dynamic
                       bell (soft knee, 1 ms attack, program-dependent release). Idle, it is a wire.
          CROSSFEED    the Bauer stereophonic-to-binaural network as in bs2b: each ear gets the other side
                       low-passed (700 Hz / 4.5 dB at full AMOUNT, the lighter canonical presets below it)
                       about 0.27 ms late, and the direct side's high-boost compensation; the outputs are
                       then normalised by the network's own mono response, so centred sound is untouched.

        Every section is bit-for-bit bypassed while its switch is off, and fades in and out over 20 ms
        when switched. The EQ's stepped switches (HPF, LOW and MID frequency, HI Q) act like switches, not
        sweeps: the EQ at the new position fades in over the old one in 20 ms. Knobs glide (~50 ms). All
        filters run as TPT state-variable filters (any biquad can be put in that form), which stay clean
        while their coefficients move. Zero latency, real-time safe after prepare() (no allocation, no
        locks), 1 or 2 channels. The first process() after prepare()/reset() starts at the settings. */
    class Lunchbox
    {
    public:
        struct Settings
        {
            // 1. CLASS-A EQ
            bool eqIn = false;
            int   hpf = 0;              // 0 off, 1: 50 Hz, 2: 80 Hz, 3: 160 Hz, 4: 300 Hz (18 dB/oct)
            int   lowFreq = 1;          // 0: 35, 1: 60, 2: 110, 3: 220 Hz (low shelf)
            float lowGainDb = 0.0f;     // -16 .. +16
            int   midFreq = 2;          // 0: 360, 1: 700, 2: 1600, 3: 3200, 4: 4800, 5: 7200 Hz (bell)
            float midGainDb = 0.0f;     // -18 .. +18
            bool  midHighQ = false;     // HI Q: a narrower bell
            float highGainDb = 0.0f;    // -16 .. +16, a 12 kHz high shelf
            bool  iron = false;         // IRON: transformers + class-A stage (level-neutral)
            // 2. DE-HARSH
            bool  harshIn = false;
            float harshAmount = 5.0f;   // 0 .. 10: how deep it may cut (10 = up to 9 dB) and how firmly
            int   harshFreq = 1;        // 0: 2.5 kHz, 1: 4 kHz, 2: 6.5 kHz
            float harshSpeedMs = 30.0f; // release, 10 .. 200 ms (attack ~1 ms)
            // 3. CROSSFEED
            bool  feedIn = false;
            float feedAmount = 5.0f;    // 0 .. 10: 10 = 700 Hz / 4.5 dB, 7 = 700 Hz / 6 dB, 4 = 650 Hz / 9.5 dB
        };

        static constexpr std::array<float, 4> hpfHzSteps  { 50.0f, 80.0f, 160.0f, 300.0f };
        static constexpr std::array<float, 4> lowHzSteps  { 35.0f, 60.0f, 110.0f, 220.0f };
        static constexpr std::array<float, 6> midHzSteps  { 360.0f, 700.0f, 1600.0f, 3200.0f, 4800.0f, 7200.0f };
        static constexpr std::array<float, 3> harshHzSteps { 2500.0f, 4000.0f, 6500.0f };
        static constexpr float highShelfHz = 12000.0f;

        void prepare (double sampleRate, int /*maxBlock*/)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            const double ctl = sr / controlEvery;

            glideK = onePole (0.012, ctl);                        // knobs and stepped switches: ~50 ms to settle
            fadeStep = (float) (1.0 / (0.020 * sr));              // IN / OUT crossfades: 20 ms

            // IRON: the cores' flux is the voltage integrated (a leaky integrator); they saturate on flux
            ironP = (float) std::exp (-2.0 * pi * ironFluxHz / sr);
            ironInv = 1.0f / (1.0f - ironP);
            ironDcR = (float) std::exp (-2.0 * pi * 5.0 / sr);
            ironEvenLp = 1.0f - (float) std::exp (-2.0 * pi * 2000.0 / sr);
            {
                const double g = std::tan (pi * ironLfHz / sr);
                ironLfK = 1.0 / ironLfQ;
                ironLfA1 = 1.0 / (1.0 + g * (g + ironLfK));
                ironLfA2 = g * ironLfA1;
                ironLfA3 = g * ironLfA2;
            }
            ironHfC = designTransformerTop();

            // The high shelf's second (helper) pole: the one that lets a digital biquad follow the
            // analogue first-order shelf best up to 20 kHz at this rate (a small search, once)
            highP2 = searchHighShelfPole();

            // DE-HARSH: the reference the band is compared with - the rest of the sound from 150 Hz to
            // 12 kHz (sub-bass does not mask a piercing peak)
            refHp = SvfCoeffs::make (sr, 150.0, 0.7071);
            refLp = SvfCoeffs::make (sr, 12000.0, 0.7071);
            detUp = 1.0f - onePole (0.001, sr);                   // peak-RMS: the power rises in ~1 ms,
            detDown = 1.0f - onePole (0.005, sr);                 // and falls back in ~5 ms
            harshAtt = onePole (0.001, ctl);
            harshSlowAtt = onePole (0.150, ctl);

            reset();
        }

        void reset()
        {
            eqW = harshW = feedW = ironW = 0.0f;
            resetEq();
            resetHarsh();
            resetFeed();
            harshCut = 0.0f;
            switching = false;
            switchMix = 0.0f;
            harshCutMeter.store (0.0f, std::memory_order_relaxed);
            outPeak = 0.0f;
            outPeakMeter.store (0.0f, std::memory_order_relaxed);
            first = true;
        }

        void process (float* const* ch, int numChannels, int numSamples, const Settings& s) noexcept
        {
            const int nc = std::clamp (numChannels, 0, 2);
            if (nc == 0 || numSamples <= 0 || ch == nullptr)
                return;

            const Target t = targetsFrom (s);
            if (first)
            {
                // Start where the settings are: no fades, no glides
                first = false;
                eqW = t.eq; harshW = t.harsh; feedW = t.feed; ironW = t.iron;
                snapEq (t);
                snapHarsh (t);
                snapFeed (t);
            }

            for (int start = 0; start < numSamples; start += controlEvery)
            {
                const int n = std::min (controlEvery, numSamples - start);
                if (nc == 2) processChunk<2> (ch, start, n, t);
                else         processChunk<1> (ch, start, n, t);
            }

            // The module meter: the output's peak, held and falling back over ~300 ms
            float pk = 0.0f;
            for (int c = 0; c < nc; ++c)
                for (int i = 0; i < numSamples; ++i)
                    pk = std::max (pk, std::abs (ch[c][i]));
            outPeak = std::max (std::isfinite (pk) ? pk : 0.0f, outPeak * (float) std::exp (-(double) numSamples / (0.3 * sr)));
            outPeakMeter.store (outPeak, std::memory_order_relaxed);
            harshCutMeter.store (harshCut * harshW, std::memory_order_relaxed);
        }

        /** How much DE-HARSH is cutting its band right now, in dB (>= 0). */
        float getHarshReductionDb() const noexcept { return harshCutMeter.load (std::memory_order_relaxed); }

        /** The output's peak (linear), held and falling back over ~300 ms. */
        float getOutputPeak() const noexcept { return outPeakMeter.load (std::memory_order_relaxed); }

        int getLatencySamples() const noexcept { return 0; }

        //==============================================================================
        // The analogue target curves (magnitude in dB), public so the tests can hold the digital
        // filters against them.

        /** LOW: the inductor shelf. A second-order shelf whose poles (at the switch frequency) and zeros
            (sqrt(K) above it) both get more resonant as the knob moves from 0 (Q 0.707, a plain shelf) to
            16 dB (pole Q 1.21, zero Q 1.12). The target, at +16 dB: the plateau (+16 dB) below ~0.3 x the
            frequency, a resonant overshoot of +2.0 dB above it at ~0.77 x (+18 dB at 46 Hz on the 60 Hz
            step), back through 0 dB at ~2.4 x and a dip of -1.4 dB at ~3.5 x (210 Hz on the 60 Hz step),
            flat again by ~10 x. At +8 dB: +0.5 / -0.3 dB. Cut is the exact mirror (the network sits in a
            feedback loop): the extra dip below, the small bump above. */
        static double lowShelfAnalogDb (double hz, double switchHz, double gainDb) noexcept
        {
            const double K = std::pow (10.0, std::abs (gainDb) / 20.0), t = std::min (1.0, std::abs (gainDb) / 16.0);
            const double qp = 0.7071 + (lowPoleQ - 0.7071) * t, qz = 0.7071 + (lowZeroQ - 0.7071) * t;
            const double x = hz / switchHz, r = std::sqrt (K);
            const double nRe = K - x * x, nIm = x * r / qz, dRe = 1.0 - x * x, dIm = x / qp;
            const double db = 10.0 * std::log10 ((nRe * nRe + nIm * nIm) / (dRe * dRe + dIm * dIm));
            return gainDb >= 0.0 ? db : -db;
        }

        /** MID: the LC bell. The tuned circuit's own Q stays put (1.4, HI Q 3.0) and the boost raises the
            numerator's damping (H = (s^2 + s K/Q + 1) / (s^2 + s/Q + 1)); cut is the reciprocal. So the
            peak sharpens as it grows (proportional Q: at +6 dB it is ~2 octaves wide 3 dB under the peak,
            at +18 dB it is Q 1.4, about an octave) while its skirts stay broad - the hardware's shape, not
            a constant-Q digital bell. */
        static double midBellAnalogDb (double hz, double centreHz, bool highQ, double gainDb) noexcept
        {
            const double K = std::pow (10.0, std::abs (gainDb) / 20.0), q = highQ ? midQHigh : midQNormal;
            const double x = hz / centreHz, re = 1.0 - x * x;
            const double db = 10.0 * std::log10 ((re * re + (x * K / q) * (x * K / q)) / (re * re + (x / q) * (x / q)));
            return gainDb >= 0.0 ? db : -db;
        }

        /** HIGH: a gentle first-order (6 dB/oct) shelf, as the capacitor network makes it, reaching half its
            dB at 12 kHz: at +16 dB it is +1.4 dB at 3 kHz, +3.1 at 5 kHz, +6.9 at 10 kHz, +8 at 12 kHz and
            +11.1 at 20 kHz, still rising (the full 16 dB lies far above the audio band). */
        static double highShelfAnalogDb (double hz, double gainDb) noexcept
        {
            const double K = std::pow (10.0, std::abs (gainDb) / 20.0), x = hz / highShelfHz;
            const double db = 10.0 * std::log10 (K * (K * x * x + 1.0) / (x * x + K));
            return gainDb >= 0.0 ? db : -db;
        }

        // Tuning, public for the tests
        static constexpr float harshQ = 2.0f;             // the watched (and cut) band: about 2/3 octave
        static constexpr float harshThresholdDb = -4.0f;  // band against the rest, where cutting starts
        static constexpr float harshKneeDb = 6.0f;        // soft knee, centred on the threshold

    private:
        static constexpr int controlEvery = 16;           // coefficients and gain computer, every 16 samples

        /** A value that glides (one-pole) toward its target at control rate and lands on it exactly. */
        struct Glide
        {
            float value = 0.0f, target = 0.0f;

            bool step (float k, float eps) noexcept
            {
                if (value == target)
                    return false;
                value = target + (value - target) * k;
                if (std::abs (value - target) < eps)
                    value = target;
                return true;
            }
            void snap() noexcept { value = target; }
        };

        /** The EQ's switch positions. A change of any of them crossfades to a second EQ at the new position. */
        struct EqPosition
        {
            int hpf = 0, low = 1, mid = 2;
            bool highQ = false;
            bool operator!= (const EqPosition& o) const noexcept { return hpf != o.hpf || low != o.low || mid != o.mid || highQ != o.highQ; }
        };

        /** The settings, cleaned up (ranges, NaN) and turned into what the sound needs. */
        struct Target
        {
            float eq = 0, iron = 0, harsh = 0, feed = 0;
            EqPosition pos;
            float lowDb = 0, midDb = 0, highDb = 0;
            float harshOct = 0, harshMaxCut = 0, harshSlope = 1, harshRelease = 0.03f;
            float feedAmount = 0;
        };

        static float finite (float v, float fallback) noexcept { return std::isfinite (v) ? v : fallback; }

        Target targetsFrom (const Settings& s) const noexcept
        {
            Target t;
            t.eq = s.eqIn ? 1.0f : 0.0f;
            t.iron = s.iron ? 1.0f : 0.0f;
            t.harsh = s.harshIn ? 1.0f : 0.0f;
            t.feed = s.feedIn ? 1.0f : 0.0f;

            t.pos.hpf = s.hpf >= 1 && s.hpf <= 4 ? s.hpf : 0;
            t.pos.low = std::clamp (s.lowFreq, 0, 3);
            t.pos.mid = std::clamp (s.midFreq, 0, 5);
            t.pos.highQ = s.midHighQ;
            t.lowDb = std::clamp (finite (s.lowGainDb, 0.0f), -16.0f, 16.0f);
            t.midDb = std::clamp (finite (s.midGainDb, 0.0f), -18.0f, 18.0f);
            t.highDb = std::clamp (finite (s.highGainDb, 0.0f), -16.0f, 16.0f);

            // DE-HARSH: AMOUNT sets how deep it may cut (0.9 dB a step) and the ratio of the relative
            // compression above the threshold (2:1 at 0, limiting - infinity:1 - at 10)
            const float amount = std::clamp (finite (s.harshAmount, 5.0f), 0.0f, 10.0f);
            t.harshOct = std::log2 (harshHzSteps[(size_t) std::clamp (s.harshFreq, 0, 2)]);
            t.harshMaxCut = 0.9f * amount;
            t.harshSlope = 0.5f + 0.05f * amount;
            t.harshRelease = std::clamp (finite (s.harshSpeedMs, 30.0f), 10.0f, 200.0f) * 0.001f;

            t.feedAmount = std::clamp (finite (s.feedAmount, 5.0f), 0.0f, 10.0f);
            return t;
        }

        static float approach (float v, float target, float step) noexcept
        {
            return v < target ? std::min (target, v + step) : std::max (target, v - step);
        }

        //==============================================================================
        template <int nc>
        void processChunk (float* const* ch, int start, int n, const Target& t) noexcept
        {
            const float move = fadeStep * (float) n;

            // Which sections run in this chunk: one that is OUT and not fading does nothing at all
            const bool eqRun = ! (eqW == 0.0f && t.eq == 0.0f);
            const bool harshRun = ! (harshW == 0.0f && t.harsh == 0.0f);
            const bool feedRun = nc == 2 && ! (feedW == 0.0f && t.feed == 0.0f);
            if (! eqRun && ! harshRun && ! feedRun)
            {
                harshCut = 0.0f;
                return;
            }

            // Coming back IN from OUT: that section starts fresh at its settings (the fade-in covers it)
            if (eqRun && eqW == 0.0f)       { resetEq(); snapEq (t); ironW = t.iron; }
            if (harshRun && harshW == 0.0f) { resetHarsh(); snapHarsh (t); }
            if (feedRun && feedW == 0.0f)   { resetFeed(); snapFeed (t); }

            const float eqFrom = eqW;
            eqW = approach (eqW, t.eq, move);
            harshW = approach (harshW, t.harsh, move);
            feedW = approach (feedW, t.feed, move);

            // EQ: the knobs' glides, a switch's crossfade, IRON's own fade
            const float ironFrom = ironW, switchFrom = switchMix;
            if (eqRun)
            {
                // IRON, switched on: its transformers start from rest (the fade covers it)
                if (ironW == 0.0f && t.iron > 0.0f)
                    for (auto& st : iron) st = {};
                ironW = approach (ironW, t.iron, move);
                updateEq (t);
                if (switching)
                    switchMix = approach (switchMix, 1.0f, move);
            }
            const int nextVoice = 1 - cur;

            // DE-HARSH: the gain computer (on what was heard up to now) and the band's glide
            if (harshRun)
            {
                if (harshHz.target = t.harshOct; harshHz.step (glideK, 1.0e-4f))
                    harshC = SvfCoeffs::make (sr, std::exp2 ((double) harshHz.value), harshQ);

                // How far the band sticks out of the rest of the sound, on the side where it sticks out
                // most (one cut for both sides: the stereo image does not move)
                float excess = -100.0f;
                for (int c = 0; c < nc; ++c)
                    if (refPow[(size_t) c] > 1.0e-10f)   // under -100 dB: silence, nothing sticks out
                        excess = std::max (excess, powerToDb (bandPow[(size_t) c]) - powerToDb (refPow[(size_t) c]) - harshThresholdDb);
                const float want = std::min (t.harshMaxCut, t.harshSlope * softRamp (excess, harshKneeDb));

                // Program-dependent release: a fast part that lets go at SPEED, and a slow part that only
                // builds up under sustained cutting (150 ms) and lets go at twice SPEED - short peaks
                // recover quickly, long harsh passages do not flutter
                const float rel = onePole (t.harshRelease, sr / controlEvery);
                harshFast = want + (harshFast - want) * (want > harshFast ? harshAtt : rel);
                const float slowWant = 0.5f * harshFast;
                harshSlow = slowWant + (harshSlow - slowWant) * (slowWant > harshSlow ? harshSlowAtt : onePole (2.0f * t.harshRelease, sr / controlEvery));
                if (harshFast < 1.0e-4f) harshFast = 0.0f;
                if (harshSlow < 1.0e-4f) harshSlow = 0.0f;
                harshCut = std::max (harshFast, harshSlow);
            }
            else
                harshCut = 0.0f;
            // The cut as a share of the band taken away (y = x - c * band): a dynamic bell whose depth is
            // linear in c, so a moving cut is perfectly smooth, and c = 0 is exactly the input
            const float shareTo = (1.0f - dbToGain (-harshCut)) * harshW;

            // CROSSFEED: the AMOUNT glides; the network is redesigned while it moves
            if (feedRun)
            {
                feedAmt.target = t.feedAmount;
                if (feedAmt.step (glideK, 1.0e-3f))
                    designFeed();
            }
            const float feedMixFrom = feedMixEff;
            const float feedMixTo = feedW * std::min (1.0f, feedAmt.value);

            const float inv = 1.0f / (float) n;
            for (int i = 0; i < n; ++i)
            {
                const float r = (float) (i + 1) * inv;
                std::array<float, 2> v { ch[0][start + i], nc > 1 ? ch[1][start + i] : 0.0f };

                if (eqRun)
                {
                    const float w = eqFrom + (eqW - eqFrom) * r;
                    const float iw = ironFrom + (ironW - ironFrom) * r;
                    const float sw = switchFrom + (switchMix - switchFrom) * r;
                    for (int c = 0; c < nc; ++c)
                    {
                        const float x = v[(size_t) c];
                        float y = x;
                        if (iw > 0.0f)
                            y = ironInput (iron[(size_t) c], y, iw);
                        const float a = voiceSample (voices[(size_t) cur], c, y);
                        y = switching ? a + (voiceSample (voices[(size_t) nextVoice], c, y) - a) * sw : a;
                        if (iw > 0.0f)
                            y = ironOutput (iron[(size_t) c], y, iw);
                        v[(size_t) c] = w == 1.0f ? y : x + (y - x) * w;
                    }
                }

                if (harshRun)
                {
                    const float share = shareFromEff + (shareTo - shareFromEff) * r;
                    for (int c = 0; c < nc; ++c)
                    {
                        const float x = v[(size_t) c];
                        const float band = harshState[(size_t) c].process (harshC, x).band;
                        // The rest of the sound: the band notched out (x - band), 150 Hz .. 12 kHz
                        const float ref = refLpState[(size_t) c].process (refLp, refHpState[(size_t) c].process (refHp, x - band).high).low;
                        const float b2 = band * band, r2 = ref * ref;
                        bandPow[(size_t) c] += (b2 > bandPow[(size_t) c] ? detUp : detDown) * (b2 - bandPow[(size_t) c]);
                        refPow[(size_t) c] += (r2 > refPow[(size_t) c] ? detUp : detDown) * (r2 - refPow[(size_t) c]);
                        if (share != 0.0f)
                            v[(size_t) c] = x - share * band;
                    }
                }

                if (feedRun)
                {
                    // Only the side (difference) signal is changed: S' = S (D - X) / (D + X), where D and X
                    // are bs2b's direct and cross filters. Per ear that is exactly bs2b's network followed by
                    // 1 / (D + X), the network's mono response - so centred sound (S = 0) passes as it was.
                    const float mix = feedMixFrom + (feedMixTo - feedMixFrom) * r;
                    const float side = 0.5f * (v[0] - v[1]);
                    const float shaped = feedState.process (feedC, side);
                    const float d = mix * (shaped - side);
                    v[0] += d;
                    v[1] -= d;
                }

                ch[0][start + i] = v[0];
                if (nc > 1)
                    ch[1][start + i] = v[1];
            }
            shareFromEff = shareTo;
            feedMixEff = feedMixTo;
            if (switching && switchMix >= 1.0f)
            {
                cur = nextVoice;   // the new position has taken over
                switching = false;
                switchMix = 0.0f;
            }

            flushDenormals();
        }

        //==============================================================================
        // Filter design: every band is designed as a digital biquad and run as a TPT SVF

        struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

        /** Any stable digital biquad as the TPT SVF's mix form (the same response exactly): through the
            inverse bilinear map, its poles give the SVF's g and k, its zeros the three output weights. */
        static SvfEqCoeffs toSvf (const Biquad& q) noexcept
        {
            const double A2 = 1.0 - q.a1 + q.a2, A1 = 2.0 * (1.0 - q.a2), A0 = 1.0 + q.a1 + q.a2;
            const double B2 = q.b0 - q.b1 + q.b2, B1 = 2.0 * (q.b0 - q.b2), B0 = q.b0 + q.b1 + q.b2;
            const double g = std::sqrt (std::max (1.0e-18, A0 / A2)), k = A1 / (A2 * g);
            const double m0 = B2 / A2, m1 = (B1 / A2 - m0 * k * g) / g, m2 = (B0 / A2 - m0 * g * g) / (g * g);
            const double a1 = 1.0 / (1.0 + g * (g + k));
            return { (float) a1, (float) (g * a1), (float) (g * g * a1), (float) m0, (float) m1, (float) m2 };
        }

        static SvfEqCoeffs flat (SvfEqCoeffs c) noexcept { c.m0 = 1.0f; c.m1 = c.m2 = 0.0f; return c; }

        static Biquad inverse (const Biquad& q) noexcept
        {
            return { 1.0 / q.b0, q.a1 / q.b0, q.a2 / q.b0, q.b1 / q.b0, q.b2 / q.b0 };
        }

        /** Matched-z poles of an analogue pole pair (Hz, Q). */
        void matchedPoles (double hz, double q, double& a1, double& a2) const noexcept
        {
            const double w = 2.0 * pi * hz / sr, z = 0.5 / q;
            const double e = std::exp (-z * w);
            a1 = z <= 1.0 ? -2.0 * e * std::cos (std::sqrt (1.0 - z * z) * w) : -2.0 * e * std::cosh (std::sqrt (z * z - 1.0) * w);
            a2 = e * e;
        }

        /** Magnitude-matched zeros (after Vicanek, "Matched second order digital filters"): given the
            poles, the numerator whose magnitude equals the analogue target (squared: t0 at DC, tn at
            Nyquist, tm at wm) - taken minimum-phase. This is what keeps a bell's or shelf's analogue shape
            up to Nyquist instead of the bilinear transform's cramping. */
        static Biquad fitZeros (double a1, double a2, double t0, double tm, double tn, double wm, bool* ok = nullptr) noexcept
        {
            const double A0 = (1.0 + a1 + a2) * (1.0 + a1 + a2), A1 = (1.0 - a1 + a2) * (1.0 - a1 + a2), A2 = -4.0 * a2;
            const double p1 = std::sin (0.5 * wm) * std::sin (0.5 * wm), p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
            const double B0 = t0 * A0, B1 = tn * A1;
            const double B2 = (tm * (A0 * p0 + A1 * p1 + A2 * p2) - B0 * p0 - B1 * p1) / p2;
            const double W = 0.5 * (std::sqrt (B0) + std::sqrt (B1)), disc = W * W + B2;
            const double b0 = 0.5 * (W + std::sqrt (std::max (0.0, disc)));
            const double b2 = -B2 / (4.0 * b0);
            if (ok != nullptr)
                *ok = disc >= 0.0 && std::abs (b2) < 0.995 * b0;
            // (a failed fit is kept minimum-phase, so its inverse stays stable)
            return { b0, 0.5 * (std::sqrt (B0) - std::sqrt (B1)), std::clamp (b2, -0.995 * b0, 0.995 * b0), a1, a2 };
        }

        static double biquadDb (const Biquad& q, double w) noexcept
        {
            const double c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);
            const double nr = q.b0 + q.b1 * c1 + q.b2 * c2, ni = -(q.b1 * s1 + q.b2 * s2);
            const double dr = 1.0 + q.a1 * c1 + q.a2 * c2, di = -(q.a1 * s1 + q.a2 * s2);
            return 10.0 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di));
        }

        //------------------------------------------------------------------------------
        /** LOW: the analogue shelf through the bilinear transform, prewarped at its poles (at 35-220 Hz
            there is nothing to cramp). Written straight into the SVF's form. */
        SvfEqCoeffs designLow (double hz, double db) const noexcept
        {
            const double K = std::pow (10.0, std::abs (db) / 20.0), t = std::min (1.0, std::abs (db) / 16.0);
            const double qp = 0.7071 + (lowPoleQ - 0.7071) * t, qz = 0.7071 + (lowZeroQ - 0.7071) * t;
            const double r = std::sqrt (K);
            double g = std::tan (pi * BiquadCoeffs::clampHz (sr, hz) / sr), k, m1, m2;
            if (db >= 0.0) { k = 1.0 / qp; m1 = r / qz - 1.0 / qp;       m2 = K - 1.0; }
            else           { g *= r; k = 1.0 / qz; m1 = 1.0 / (qp * r) - 1.0 / qz; m2 = 1.0 / K - 1.0; }
            const double a1 = 1.0 / (1.0 + g * (g + k));
            SvfEqCoeffs c { (float) a1, (float) (g * a1), (float) (g * g * a1), 1.0f, (float) m1, (float) m2 };
            return db == 0.0 ? flat (c) : c;
        }

        /** MID: matched poles at the centre, zeros fitted to the analogue bell at DC, the centre and
            Nyquist; cut is the boost's exact inverse (as the hardware's). */
        SvfEqCoeffs designMid (double hz, bool highQ, double db) const noexcept
        {
            const double K = std::pow (10.0, std::abs (db) / 20.0), q = highQ ? midQHigh : midQNormal;
            double a1, a2;
            matchedPoles (hz, q, a1, a2);
            if (db == 0.0)
                return flat (toSvf ({ 1.0, a1, a2, a1, a2 }));
            const double x = 0.5 * sr / hz, re = 1.0 - x * x;
            const double tn = (re * re + (x * K / q) * (x * K / q)) / (re * re + (x / q) * (x / q));
            const Biquad boost = fitZeros (a1, a2, 1.0, K * K, tn, 2.0 * pi * hz / sr);
            return toSvf (db > 0.0 ? boost : inverse (boost));
        }

        /** HIGH: the first-order analogue shelf as a biquad: its matched pole, a helper pole (chosen in
            prepare() for this rate), and zeros fitted at DC, 12 kHz (or 0.3 x the rate) and Nyquist. Error
            against the analogue curve up to 20 kHz: about 0.1 dB at 44.1 / 48 kHz, nothing at 96 kHz. */
        Biquad designHighBoost (double K, double p2) const noexcept
        {
            const double p1 = std::exp (-2.0 * pi * highShelfHz * std::sqrt (K) / sr);
            const double a1 = -(p1 + p2), a2 = p1 * p2;
            auto t = [K] (double hz) { const double x = hz / highShelfHz; return K * (K * x * x + 1.0) / (x * x + K); };
            const double fm = std::min ((double) highShelfHz, 0.3 * sr);
            return fitZeros (a1, a2, 1.0, t (fm), t (0.5 * sr), 2.0 * pi * fm / sr);
        }

        SvfEqCoeffs designHigh (double db) const noexcept
        {
            const Biquad boost = designHighBoost (std::pow (10.0, std::abs (db) / 20.0), highP2);
            if (db == 0.0)
                return flat (toSvf (boost));
            return toSvf (db > 0.0 ? boost : inverse (boost));
        }

        double searchHighShelfPole() const noexcept
        {
            double best = 0.0, bestErr = 1.0e9;
            for (int i = 0; i <= 72; ++i)
            {
                const double p2 = -0.8 + 0.0125 * i;
                double worst = 0.0;
                for (int gdb = 2; gdb <= 16 && worst < bestErr; gdb += 2)
                {
                    const double K = std::pow (10.0, gdb / 20.0);
                    const double p1 = std::exp (-2.0 * pi * highShelfHz * std::sqrt (K) / sr);
                    const double fm = std::min ((double) highShelfHz, 0.3 * sr);
                    auto t = [K] (double hz) { const double x = hz / highShelfHz; return K * (K * x * x + 1.0) / (x * x + K); };
                    bool ok = false;
                    fitZeros (-(p1 + p2), p1 * p2, 1.0, t (fm), t (0.5 * sr), 2.0 * pi * fm / sr, &ok);
                    if (! ok)
                    {
                        worst = 1.0e9;
                        break;
                    }
                    const Biquad b = designHighBoost (K, p2);
                    for (double hz = 1000.0; hz <= std::min (20000.0, 0.46 * sr); hz *= 1.08)
                        worst = std::max (worst, std::abs (biquadDb (b, 2.0 * pi * hz / sr) - highShelfAnalogDb (hz, gdb)));
                }
                if (worst < bestErr) { bestErr = worst; best = p2; }
            }
            return best;
        }

        /** IRON's top end: the output transformer's resonance, well above the band (55 kHz, Q 0.8): +0.03 dB
            at 10 kHz, +0.18 dB at 20 kHz. Up to 50 kHz rates as a magnitude-fitted minimum-phase
            three-tap filter, above as a biquad fitted at DC, 20 kHz and Nyquist. */
        SvfEqCoeffs designTransformerTop() const noexcept
        {
            auto t = [] (double hz) { const double x = hz / ironHfHz, re = 1.0 - x * x; return 1.0 / (re * re + (x / ironHfQ) * (x / ironHfQ)); };
            double a1 = 0.0, a2 = 0.0, fm = 0.25 * sr;
            if (sr > 50000.0)
            {
                // Bilinear poles, prewarped at 20 kHz
                fm = 20000.0;
                const double c = 2.0 * pi * fm / std::tan (pi * fm / sr), w = 2.0 * pi * ironHfHz / c;
                const double d0 = 1.0 + w / ironHfQ + w * w;
                a1 = 2.0 * (w * w - 1.0) / d0;
                a2 = (1.0 - w / ironHfQ + w * w) / d0;
            }
            return toSvf (fitZeros (a1, a2, 1.0, t (fm), t (0.5 * sr), 2.0 * pi * fm / sr));
        }

        //==============================================================================
        // CLASS-A EQ

        struct EqState
        {
            float hp1 = 0.0f;
            SvfState hp2 {};
            SvfEqState low {}, mid {}, high {};
        };

        /** One EQ at one switch position: its coefficients and both channels' filters. */
        struct EqVoice
        {
            EqPosition pos;
            float hp1G = 0.0f;
            SvfCoeffs hp2C {};
            SvfEqCoeffs lowC {}, midC {}, highC {};
            std::array<EqState, 2> st {};
        };

        /** One channel through one EQ: HPF, low shelf, mid bell, high shelf. */
        static inline float voiceSample (EqVoice& e, int c, float x) noexcept
        {
            auto& st = e.st[(size_t) c];
            float v = x;

            // HPF: third order, 0.5 dB Chebyshev (a first-order and a resonant second-order section),
            // -3 dB at the switch's frequency: 18 dB/oct with the slight lift just above the corner that
            // gives the console filter its firm, not thin, low end. It runs even while off, so its state
            // is ready when a switch fades it in.
            {
                const float hv = (v - st.hp1) * e.hp1G;
                const float lp = hv + st.hp1;
                st.hp1 = lp + hv;
                const float h = st.hp2.process (e.hp2C, v - lp).high;
                if (e.pos.hpf > 0)
                    v = h;
            }

            v = st.low.process (e.lowC, v);
            v = st.mid.process (e.midC, v);
            return st.high.process (e.highC, v);
        }

        //------------------------------------------------------------------------------
        // IRON: input transformer -> (EQ) -> class-A output stage -> output transformer

        /** 2x polyphase IIR halfband (two chains of first-order allpasses in z^2; 6 coefficients, 0.06
            transition: flat to 0.38 x the base rate, ~85 dB image / alias rejection). Minimum phase - no
            latency - with ~1.5 samples (at the base rate) of group delay each way at low frequencies. Only
            the class-A stage's distortion residual goes through it; the dry signal never does. */
        static constexpr size_t hbN = 6;
        static constexpr std::array<float, hbN> hbCoef { 0.054217525755190787f, 0.1967979698143128f, 0.38308732729094908f,
                                                         0.57313641114809133f, 0.74872094443642634f, 0.91429370968501777f };
        struct Halfband
        {
            std::array<float, hbN> x {}, y {};

            inline void stages (float& s0, float& s1) noexcept
            {
                for (size_t i = 0; i < hbN; i += 2)
                {
                    const float t0 = (s0 - y[i]) * hbCoef[i] + x[i];
                    x[i] = s0; y[i] = t0; s0 = t0;
                    const float t1 = (s1 - y[i + 1]) * hbCoef[i + 1] + x[i + 1];
                    x[i + 1] = s1; y[i + 1] = t1; s1 = t1;
                }
            }
            inline void up (float in, float& o0, float& o1) noexcept { o0 = in; o1 = in; stages (o0, o1); }
            inline float down (float i0, float i1) noexcept { float s0 = i1, s1 = i0; stages (s0, s1); return 0.5f * (s0 + s1); }
        };

        struct Transformer { float flux = 0.0f, prevRes = 0.0f, evenX = 0.0f, evenY = 0.0f, e1 = 0.0f, e2 = 0.0f; };

        struct IronState
        {
            Transformer in, out;
            double lf1 = 0.0, lf2 = 0.0; // the transformers' low-frequency corner (a 7 Hz SVF: in double,
                                         // where float would round its tiny steps away at 96 kHz)
            SvfEqState hf {};            // the output transformer's high resonance
            Halfband hbUp, hbDown;       // the class-A stage's oversampling
            float dcX = 0.0f, dcY = 0.0f; // its residual's DC blocker
        };

        /** One transformer core. The flux (the voltage through a leaky integrator, 20 Hz) meets a soft
            saturation: -a u^3 / (1 + u^2), odd, so mostly 3rd harmonic, plus a trace of even order (the
            core's remanence). Only the difference it makes goes back through the integrator's exact
            inverse and is added to the signal - so the harmonics grow with level and fall with frequency
            (the flux falls as 1/f): the lows thicken, the top stays clean, as with real iron. */
        template <bool withEven>
        inline float core (Transformer& st, float x, float odd, float even) noexcept
        {
            st.flux = x + ironP * (st.flux - x);
            const float u = st.flux, u2 = u * u, soft = u2 / (1.0f + u2);
            float res = -odd * u * soft;
            if constexpr (withEven)
            {
                // The even part sees the flux through a 2 kHz low-pass (two poles): at the lows, where
                // the core's even order lives, it changes nothing, and it cannot fold high tones' 2nd
                // harmonics back under Nyquist
                st.e1 += ironEvenLp * (u - st.e1);
                st.e2 += ironEvenLp * (st.e1 - st.e2);
                const float ue = st.e2;
                const float evenRaw = even * ue * ue / (1.0f + ue * ue);
                const float e = evenRaw - st.evenX + ironDcR * st.evenY;       // without its DC
                st.evenX = evenRaw;
                st.evenY = e;
                res += e;
            }
            const float back = (res - ironP * st.prevRes) * ironInv;           // the integrator, undone
            st.prevRes = res;
            return x + back;
        }

        inline float ironInput (IronState& st, float x, float mix) noexcept
        {
            const float y = core<true> (st.in, x, ironOddIn, ironEvenIn);
            const double v3 = (double) y - st.lf2;
            const double v1 = ironLfA1 * st.lf1 + ironLfA2 * v3;
            const double v2 = st.lf2 + ironLfA2 * st.lf1 + ironLfA3 * v3;
            st.lf1 = 2.0 * v1 - st.lf1;
            st.lf2 = 2.0 * v2 - st.lf2;
            const float lf = (float) ((double) y - ironLfK * v1 - v2);
            return x + mix * (lf - x);
        }

        /** The class-A output stage (single-ended: mostly 2nd harmonic, a little 3rd) at twice the rate,
            only its residual decimated back; then the output transformer and its top end. */
        inline float ironOutput (IronState& st, float x, float mix) noexcept
        {
            float u0, u1;
            st.hbUp.up (x, u0, u1);
            auto stage = [] (float v) noexcept
            {
                v = std::clamp (v, -4.0f, 4.0f);
                return classA2 * v * v - classA3 * v * v * v;
            };
            const float resRaw = st.hbDown.down (stage (u0), stage (u1));
            const float res = resRaw - st.dcX + ironDcR * st.dcY;
            st.dcX = resRaw;
            st.dcY = res;
            float y = core<false> (st.out, x + res, ironOddOut, 0.0f);
            y = st.hf.process (ironHfC, y);
            return x + mix * (y - x);
        }

        //------------------------------------------------------------------------------
        /** An EQ's coefficients: the HPF's (its position only), the bands' (position and the gliding gains). */
        void designHpf (EqVoice& e) const noexcept
        {
            // The Chebyshev prototype's two sections, scaled so the whole is -3 dB at the switch frequency
            const double fc = hpfHzSteps[(size_t) std::max (0, e.pos.hpf - 1)];
            const double g = std::tan (pi * BiquadCoeffs::clampHz (sr, fc * hp1Scale) / sr);
            e.hp1G = (float) (g / (1.0 + g));
            e.hp2C = SvfCoeffs::make (sr, fc * hp2Scale, hp2Q);
        }

        void designBands (EqVoice& e, bool low, bool mid, bool high) const noexcept
        {
            if (low)  e.lowC = designLow (lowHzSteps[(size_t) e.pos.low], lowDb.value);
            if (mid)  e.midC = designMid (midHzSteps[(size_t) e.pos.mid], e.pos.highQ, midDb.value);
            if (high) e.highC = designHigh (highDb.value);
        }

        void updateEq (const Target& t) noexcept
        {
            lowDb.target = t.lowDb; midDb.target = t.midDb; highDb.target = t.highDb;
            const bool low = lowDb.step (glideK, 1.0e-3f);
            const bool mid = midDb.step (glideK, 1.0e-3f);
            const bool high = highDb.step (glideK, 1.0e-3f);

            // A switch moved: a second EQ, at the new position, starts from the current one's filter
            // states (so it is already "playing") and fades in over it. A further change waits for this
            // fade to end (20 ms), then fades to wherever the switches are by then.
            if (! switching && t.pos != voices[(size_t) cur].pos)
            {
                auto& next = voices[(size_t) (1 - cur)];
                next.st = voices[(size_t) cur].st;
                next.pos = t.pos;
                designHpf (next);
                designBands (next, true, true, true);
                switching = true;
                switchMix = 0.0f;
            }
            if (low || mid || high)
            {
                designBands (voices[(size_t) cur], low, mid, high);
                if (switching)
                    designBands (voices[(size_t) (1 - cur)], low, mid, high);
            }
        }

        void snapEq (const Target& t) noexcept
        {
            lowDb.target = t.lowDb; midDb.target = t.midDb; highDb.target = t.highDb;
            for (auto* g : { &lowDb, &midDb, &highDb })
                g->snap();
            auto& e = voices[(size_t) cur];
            e.pos = t.pos;
            designHpf (e);
            designBands (e, true, true, true);
            switching = false;
            switchMix = 0.0f;
        }

        void resetEq() noexcept
        {
            for (auto& e : voices)
                for (auto& st : e.st) st = {};
            for (auto& st : iron) st = {};
        }

        //==============================================================================
        void snapHarsh (const Target& t) noexcept
        {
            harshHz.target = t.harshOct;
            harshHz.snap();
            harshC = SvfCoeffs::make (sr, std::exp2 ((double) harshHz.value), harshQ);
        }

        void resetHarsh() noexcept
        {
            for (auto* s : { &harshState, &refHpState, &refLpState })
                for (auto& st : *s) st.reset();
            bandPow = {};
            refPow = {};
            harshCut = harshFast = harshSlow = 0.0f;
            shareFromEff = 0.0f;
        }

        //==============================================================================
        // CROSSFEED

        /** bs2b's network for the gliding AMOUNT. The canonical presets as anchors - 10: 700 Hz / 4.5 dB
            (bs2b default), 7: 700 Hz / 6 dB (C. Moy), 4: 650 Hz / 9.5 dB (J. Meier), 1: 650 Hz / 13.5 dB
            - linear between them; under 1 the network fades out. From bs2b (B. Mikhaylov): the cross
            filter is a one-pole low-pass at fc with gain GB_lo = -5/6 level - 3 dB, the direct filter a
            one-pole high boost of GB_hi = level/6 - 3 dB at the lows, turning over at fc x 2^((GB_lo -
            20 log10 G_hi) / 12). Its poles and zeros are bs2b's own (impulse-invariant) ones. */
        void designFeed() noexcept
        {
            static constexpr float at[4] { 1.0f, 4.0f, 7.0f, 10.0f }, lvl[4] { 13.5f, 9.5f, 6.0f, 4.5f }, fc[4] { 650.0f, 650.0f, 700.0f, 700.0f };
            const float a = std::clamp (feedAmt.value, 1.0f, 10.0f);
            const int k = a < 4.0f ? 0 : a < 7.0f ? 1 : 2;
            const float f = (a - at[k]) / (at[k + 1] - at[k]);
            const double level = lvl[k] + (lvl[k + 1] - lvl[k]) * f, fcLo = fc[k] + (fc[k + 1] - fc[k]) * f;

            const double gbLo = level * -5.0 / 6.0 - 3.0, gbHi = level / 6.0 - 3.0;
            const double gLo = std::pow (10.0, gbLo / 20.0), gHi = 1.0 - std::pow (10.0, gbHi / 20.0);
            const double fcHi = fcLo * std::pow (2.0, (gbLo - 20.0 * std::log10 (gHi)) / 12.0);
            const double xl = std::exp (-2.0 * pi * fcLo / sr), xh = std::exp (-2.0 * pi * fcHi / sr);
            const double a0l = gLo * (1.0 - xl), b1l = xl;                    // X(z) = a0l / (1 - b1l z^-1)
            const double a0h = 1.0 - gHi * (1.0 - xh), a1h = -xh, b1h = xh;   // D(z) = (a0h + a1h z^-1) / (1 - b1h z^-1)

            // S' / S = (D - X) / (D + X), over the common denominator
            const double c0 = a0h + a0l, c1 = a1h - a0h * b1l - a0l * b1h, c2 = -a1h * b1l;
            const double d0 = a0h - a0l, d1 = a1h - a0h * b1l + a0l * b1h, d2 = -a1h * b1l;
            feedC = toSvf ({ d0 / c0, d1 / c0, d2 / c0, c1 / c0, c2 / c0 });
        }

        void snapFeed (const Target& t) noexcept
        {
            feedAmt.target = t.feedAmount;
            feedAmt.snap();
            designFeed();
            feedMixEff = feedW * std::min (1.0f, feedAmt.value);
        }

        void resetFeed() noexcept
        {
            feedState.reset();
            feedMixEff = 0.0f;
        }

        /** Silence after sound: filter states decay toward zero and would end in the CPU's slow
            denormal range. Anything under 1e-20 (-400 dB) is zero - a filter's states together (zeroing
            one alone would leave the other stuck: an SVF's low-pass state only decays through its band
            state). */
        void flushDenormals() noexcept
        {
            auto f = [] (float& v) { if (std::abs (v) < 1.0e-20f) v = 0.0f; };
            auto pair = [] (auto& a, auto& b)
            {
                if (std::abs (a) < 1.0e-20f && std::abs (b) < 1.0e-20f)
                    a = b = 0;
            };
            for (auto& e : voices)
                for (auto& st : e.st)
                {
                    f (st.hp1); pair (st.hp2.ic1, st.hp2.ic2);
                    for (auto* b : { &st.low, &st.mid, &st.high }) pair (b->ic1, b->ic2);
                }
            for (auto& st : iron)
            {
                for (auto* t : { &st.in, &st.out })
                {
                    pair (t->flux, t->prevRes); pair (t->evenX, t->evenY); pair (t->e1, t->e2);
                }
                pair (st.lf1, st.lf2); pair (st.hf.ic1, st.hf.ic2); pair (st.dcX, st.dcY);
                for (auto* h : { &st.hbUp, &st.hbDown })
                    for (size_t i = 0; i < hbN; ++i) pair (h->x[i], h->y[i]);
            }
            for (auto* s : { &harshState, &refHpState, &refLpState })
                for (auto& st : *s) pair (st.ic1, st.ic2);
            for (auto& p : bandPow) f (p);
            for (auto& p : refPow) f (p);
            pair (feedState.ic1, feedState.ic2);
        }

        //==============================================================================
        // HPF: 3rd-order Chebyshev, 0.5 dB ripple, normalised to -3 dB at 1 (worked out once):
        // real pole 0.6265, pair 0.3132 +- j1.0229; the -3 dB point of that prototype is at 1.1677.
        // As a high-pass the sections' corners are the inverse: 1.864 x and 1.0915 x the -3 dB frequency.
        static constexpr double hp1Scale = 1.1677 / 0.6265;
        static constexpr double hp2Scale = 1.1677 / 1.0698;
        static constexpr double hp2Q = 1.708;
        static constexpr double lowPoleQ = 1.21, lowZeroQ = 1.12;     // the low shelf's resonances at 16 dB
        static constexpr double midQNormal = 1.4, midQHigh = 3.0;     // the mid tank's own Q (and HI Q)

        // IRON (0 dBFS = the top of the unit's comfortable range)
        static constexpr double ironFluxHz = 20.0;                    // the cores' flux integrator corner
        static constexpr double ironLfHz = 7.0, ironLfQ = 0.8;        // low end: +0.17 dB at 20 Hz, -0.1 at 10 Hz
        static constexpr double ironHfHz = 55000.0, ironHfQ = 0.8;    // top end: +0.18 dB at 20 kHz
        static constexpr float ironOddIn = 0.09f, ironOddOut = 0.07f, ironEvenIn = 0.012f;
        static constexpr float classA2 = 0.0014f, classA3 = 0.0004f;

        double sr = 48000.0;
        float glideK = 0.9f, fadeStep = 0.001f;
        bool first = true;

        // Section fades
        float eqW = 0, harshW = 0, feedW = 0, ironW = 0;

        // EQ
        std::array<EqVoice, 2> voices {};
        int cur = 0;
        bool switching = false;
        float switchMix = 0.0f;
        Glide lowDb, midDb, highDb;
        double highP2 = -0.5;
        std::array<IronState, 2> iron {};
        double ironLfA1 = 1.0, ironLfA2 = 0.0, ironLfA3 = 0.0, ironLfK = 1.25;
        SvfEqCoeffs ironHfC {};
        float ironP = 0.997f, ironInv = 380.0f, ironDcR = 0.999f, ironEvenLp = 0.2f;

        // DE-HARSH
        Glide harshHz;
        SvfCoeffs harshC {}, refHp {}, refLp {};
        std::array<SvfState, 2> harshState {}, refHpState {}, refLpState {};
        std::array<float, 2> bandPow {}, refPow {};
        float detUp = 0.02f, detDown = 0.003f, harshAtt = 0.7f, harshSlowAtt = 0.99f;
        float harshCut = 0.0f, harshFast = 0.0f, harshSlow = 0.0f, shareFromEff = 0.0f;

        // CROSSFEED
        Glide feedAmt;
        SvfEqCoeffs feedC {};
        SvfEqState feedState {};
        float feedMixEff = 0.0f;

        // Meters
        float outPeak = 0.0f;
        std::atomic<float> harshCutMeter { 0.0f }, outPeakMeter { 0.0f };
    };
}
