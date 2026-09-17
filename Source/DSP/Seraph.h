#pragma once

#include <juce_dsp/juce_dsp.h>
#include "DspMath.h"

namespace enh::dsp
{
    /** What one process is changing on each channel right now: energy of the change relative to
        that channel's own input (dB, -60 = doing nothing). Drives SERAPH's live L/R display. */
    struct ActivityMeter
    {
        std::array<double, 2> change {}, input {};
        std::array<float, 2> db { -60.0f, -60.0f };

        inline void add (int c, float in, float delta) noexcept
        {
            input[(size_t) c] += (double) in * in;
            change[(size_t) c] += (double) delta * delta;
        }

        void publish (float smoothing, double extraDb = 0.0) noexcept
        {
            for (size_t c = 0; c < 2; ++c)
            {
                const float target = input[c] > 1.0e-10 && change[c] > 0.0
                                       ? (float) std::clamp (10.0 * std::log10 (change[c] / input[c]) + extraDb, -60.0, 6.0)
                                       : -60.0f;
                db[c] = target + (db[c] - target) * smoothing;
                change[c] = input[c] = 0.0;
            }
        }

        void clear() noexcept { change = {}; input = {}; db = { -60.0f, -60.0f }; }
    };

    //==============================================================================
    /** SILK: tone & texture. Everything here listens to the programme first.

          SMOOTH   28 detection bands (150 Hz - 16 kHz, 1/4 octave). A band that suddenly sticks out
                   of its own spectral neighbourhood (a ringing resonance, a harsh whistle, a boxy
                   build-up) gets a narrow dynamic dip, only as deep and only as long as it sticks
                   out. PROTECT leaves the first 15 ms of every onset alone and halves the dips in
                   1-4.5 kHz, so footsteps and attacks keep their bite.
          AIR      a smooth high shelf plus freshly generated 10-20 kHz harmonics from the 4-8 kHz
                   region. Scales with how dull the source is and backs off while SMOOTH is busy.
          WARMTH   envelope-normalised 2nd/3rd harmonics of the low mids, plus a gentle triode curve.
          BODY     low-mid fullness at 180 Hz, only as much as the source is thin.
          TAPE     pre-emphasised soft saturation: rounds harsh transients without dulling.
          AUTO     loudness-matched output (K-weighted), OUTPUT trims on top.
    */
    class SilkStage
    {
    public:
        struct Settings
        {
            float smooth = 4.0f, air = 4.0f, warmth = 3.0f, body = 2.0f;   // 0..10 on the knobs (up to 30 with MULTIPLY)
            float outputDb = 0.0f;
            bool protect = true, tape = false, autoGain = true;
            float strength = 1.0f;   // STRENGTH: 0 = no effect, 1 = as set, up to 5 = five times the effect
        };

        void prepare (double sampleRate);
        void reset();
        void process (float* const* channels, int numChannels, int numSamples, const Settings&, float blendTarget) noexcept;

        float getSmoothingDb() const noexcept { return smoothingDb; }
        float getBlend() const noexcept       { return blend; }
        float getLevelDb() const noexcept     { return 20.0f * std::log10 (std::max (1.0e-4f, outGain)); }

        /** Current dip per detection band (dB, <= 0) and the bands' centre frequencies. */
        const std::array<float, 28>& getDips() const noexcept   { return cutDb; }

        ActivityMeter smoothMeter, airMeter, warmthMeter, bodyMeter, tapeMeter;

        static constexpr int numBands = 28;

    private:
        void controlTick (const Settings&) noexcept;

        struct Exciter { SvfState band1, band2, post1, post2, top; float env = 0.0f; };

        struct Channel
        {
            std::array<BiquadState, numBands> dip {};
            BiquadState airShelf, bodyPeak, tapePre, tapePost;
            Exciter air, warm;
            float triodeDcX = 0.0f, triodeDcY = 0.0f;
        };

        double sr = 48000.0;
        int controlInterval = 32, toTick = 0;

        // Detection (mono)
        std::array<BiquadCoeffs, numBands> detect {};
        std::array<BiquadState, numBands> detectState {};
        std::array<float, numBands> fast {}, slow {}, cutDb {}, bandHz {};
        std::array<int, numBands> onsetHold {};
        std::array<PeakingDesigner, numBands> designers {};
        std::array<BiquadCoeffs, numBands> dipCoeffs {};
        std::array<float, numBands> designedDb {};
        float fastAtt = 0.0f, fastRel = 0.0f, slowK = 0.0f, longK = 0.0f;
        float cutAtt = 0.0f, cutRel = 0.0f;
        std::array<float, numBands> longTerm {};

        std::array<Channel, 2> ch {};
        BiquadCoeffs airShelf, bodyPeak, tapePre, tapePost;
        SvfCoeffs airBand, airPost, airTop, warmBand, warmPost, warmTop;
        float airShelfDb = 1000.0f, bodyDb = 1000.0f, airMix = 0.0f, warmMix = 0.0f, triodeK = 0.6f, triodeMix = 1.0f;
        float envAtt = 0.0f, envRel = 0.0f, dcCoeff = 0.999f;

        // Loudness match
        BiquadCoeffs kHp, kShelf;
        BiquadState inHp, inShelf, outHp, outShelf;
        float inMs = 0.0f, outMs = 0.0f, msCoeff = 0.0f, autoDb = 0.0f, outGain = 1.0f;

        float smoothingDb = 0.0f, blend = 0.0f, blendCoeff = 0.0f, meterSmoothing = 0.0f;
        bool tapeOn = false;
    };

    //==============================================================================
    /** HALO: space & width.

          WIDTH    mid/side. Below 120 Hz the side is removed (BASS MONO). Above, the side is scaled
                   and, when the source is nearly mono, a decorrelated copy of the mid is added to
                   the side only - the mono sum is untouched, so it can never collapse in mono.
          SPACE    a dense 8-line feedback-delay-network reverb (Hadamard mixing, per-line damping,
                   slow line modulation with MOD, 4-stage input diffusion, 18 ms pre-delay), fed from
                   the mid above 200 Hz so it never muddies the low end.
          DECAY    tail length, 0.3 - 8 s.
          SHIMMER  an octave-up pitch shift of the tail fed back into the network.
          TONE     dark (warm plate) .. airy (bright hall).
          DUCK     the tail sits ~10 dB lower while the programme is busy and blooms in the gaps,
                   so dialogue, clicks and footsteps stay dry and precise.
    */
    class HaloStage
    {
    public:
        struct Settings
        {
            float width = 1.2f;      // 0..2 (1 = unchanged)
            float space = 0.25f;     // 0..1
            float decayS = 2.2f;
            float shimmer = 0.15f;   // 0..1
            float tone = 0.6f;       // 0..1
            bool duck = true, bassMono = true, mod = true;
            float strength = 1.0f;   // STRENGTH: scales width change, tail level and shimmer
        };

        void prepare (double sampleRate);
        void reset();
        void process (float* const* channels, int numChannels, int numSamples, const Settings&, float blendTarget) noexcept;

        /** Tail level relative to the dry programme (dB). */
        float getHaloDb() const noexcept { return haloDb; }
        float getBlend() const noexcept  { return blend; }

        ActivityMeter widthMeter, spaceMeter, shimmerMeter;

    private:
        static constexpr int numLines = 8;

        struct Delay
        {
            std::vector<float> buffer;
            int mask = 0, write = 0;

            void prepare (int maxSamples)
            {
                int size = 1;
                while (size < maxSamples + 4) size <<= 1;
                buffer.assign ((size_t) size, 0.0f);
                mask = size - 1;
                write = 0;
            }

            void clear() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); }
            inline void push (float x) noexcept { buffer[(size_t) write] = x; write = (write + 1) & mask; }

            inline float read (float delaySamples) const noexcept
            {
                const float pos = (float) write - delaySamples;
                const int i0 = (int) std::floor (pos);
                const float f = pos - (float) i0;
                const float a = buffer[(size_t) (i0 & mask)], b = buffer[(size_t) ((i0 + 1) & mask)];
                return a + (b - a) * f;
            }
        };

        struct Allpass
        {
            Delay line;
            float delay = 1.0f, gain = 0.5f;

            inline float process (float x) noexcept
            {
                const float d = line.read (delay);
                const float v = x + gain * d;
                line.push (v);
                return d - gain * v;
            }
        };

        double sr = 48000.0;

        // Width
        BiquadCoeffs sideLpCoeffs, sideHpCoeffs;
        BiquadState sideLp1, sideLp2, sideHp1, sideHp2;
        float midHpK = 0.0f, midHpState = 0.0f;
        std::array<Allpass, 3> decorrelate {};
        float monoPowerM = 0.0f, monoPowerS = 0.0f, powerK = 0.0f;

        // Reverb
        Delay preDelay;
        std::array<Allpass, 4> diffusers {};
        std::array<Delay, numLines> lines {};
        std::array<float, numLines> lineLength {}, lineGain {}, damp {}, lfoPhase {}, lfoRate {}, lfoOffset {};
        float dampK = 0.0f, inHpK = 0.0f, inHpState = 0.0f, inLpK = 0.0f, inLpState = 0.0f;
        float decayDesigned = -1.0f, toneDesigned = -1.0f;

        // Shimmer (two-tap delay pitch shifter, +12 st)
        Delay shimmerLine;
        float shimmerPhase = 0.0f, shimmerWindow = 2400.0f, shimmerHpK = 0.0f, shimmerHpState = 0.0f, shimmerFeed = 0.0f;

        // Early reflections (stereo taps before the dense tail)
        Delay early;
        std::array<float, 8> earlyTap {};

        // Ducking + meters
        float dryFast = 0.0f, drySlow = 0.0f, duckGain = 1.0f, fastAtt = 0.0f, fastRel = 0.0f, slowK = 0.0f, duckK = 0.0f;
        float wetPower = 0.0f, dryPower = 0.0f, meterK = 0.0f;
        float haloDb = -60.0f, blend = 0.0f, blendCoeff = 0.0f, widthSmoothed = 1.0f, spaceSmoothed = 0.0f, paramK = 0.0f;
        double shimmerEnergy = 0.0, networkInputEnergy = 0.0;
        float meterSmoothing = 0.0f;
    };

    //==============================================================================
    /** SERAPH: the purple unit above ENH Master. OFF / SILK / HEAVEN (SILK + HALO).
        Zero latency; true bypass (bit-exact) when OFF and settled. */
    class Seraph
    {
    public:
        enum Mode { off = 0, silkOnly = 1, heaven = 2 };

        struct Settings
        {
            int mode = off;   // the plugin parameter defaults to HEAVEN; engine users opt in
            SilkStage::Settings silk {};
            HaloStage::Settings halo {};
        };

        void prepare (double sampleRate)
        {
            silk.prepare (sampleRate);
            halo.prepare (sampleRate);
            limiterRelease = onePole (0.080, sampleRate);
            reset();
        }

        void reset()
        {
            silk.reset();
            halo.reset();
            limiterGain = 1.0f;
        }

        void process (float* const* channels, int numChannels, int numSamples, const Settings& s) noexcept
        {
            silk.process (channels, numChannels, numSamples, s.silk, s.mode >= silkOnly ? 1.0f : 0.0f);
            halo.process (channels, numChannels, numSamples, s.halo, s.mode == heaven ? 1.0f : 0.0f);

            if (silk.getBlend() <= 0.0f && halo.getBlend() <= 0.0f)
            {
                limiterGain = 1.0f;
                return;   // OFF stays bit-exact
            }

            // Output limiter: instant gain-down on peaks above the ceiling, smooth 80 ms recovery, then a
            // soft clip for whatever the zero-latency detector cannot catch. Heavy settings stay clean.
            constexpr float ceiling = 0.92f;
            const int chans = std::min (numChannels, 2);
            for (int i = 0; i < numSamples; ++i)
            {
                float peak = 0.0f;
                for (int c = 0; c < chans; ++c)
                    peak = std::max (peak, std::abs (channels[c][i]));

                const float needed = peak > ceiling ? ceiling / peak : 1.0f;
                limiterGain = needed < limiterGain ? needed : needed + (limiterGain - needed) * limiterRelease;

                for (int c = 0; c < chans; ++c)
                {
                    float y = channels[c][i] * limiterGain;
                    const float a = std::abs (y);
                    if (a > 0.97f)
                        y = std::copysign (0.97f + 0.025f * std::tanh ((a - 0.97f) / 0.025f), y);
                    channels[c][i] = y;
                }
            }

            gainReductionDb = -20.0f * std::log10 (std::max (1.0e-3f, limiterGain));
        }

        float getLimiterDb() const noexcept { return gainReductionDb; }

        const SilkStage& getSilk() const noexcept { return silk; }
        const HaloStage& getHalo() const noexcept { return halo; }

        /** Live per-channel activity, in display order. */
        enum Activity { smoothAct = 0, airAct, warmthAct, bodyAct, tapeAct, widthAct, spaceAct, shimmerAct, numActivities };

        float getActivityDb (int activity, int ch) const noexcept
        {
            const auto c = (size_t) std::clamp (ch, 0, 1);
            switch (activity)
            {
                case smoothAct:  return silk.smoothMeter.db[c];
                case airAct:     return silk.airMeter.db[c];
                case warmthAct:  return silk.warmthMeter.db[c];
                case bodyAct:    return silk.bodyMeter.db[c];
                case tapeAct:    return silk.tapeMeter.db[c];
                case widthAct:   return halo.widthMeter.db[c];
                case spaceAct:   return halo.spaceMeter.db[c];
                case shimmerAct: return halo.shimmerMeter.db[c];
                default:         return -60.0f;
            }
        }

    private:
        SilkStage silk;
        HaloStage halo;
        float limiterGain = 1.0f, limiterRelease = 0.99f, gainReductionDb = 0.0f;
    };
}
