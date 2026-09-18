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
          WARMTH   envelope-normalised 2nd/3rd harmonics of the low mids, plus a gentle triode curve on
                   everything above 120 Hz, driven at a fixed level (envelope-normalised): the same
                   warmth whether the music is quiet or loud, and the sub-bass passes clean (it used to
                   drive the whole signal at its own level - loud bass was crushed).
          BODY     low-mid fullness at 180 Hz, only as much as the source is thin.
          TAPE     pre-emphasised soft saturation: rounds harsh transients without dulling.
          SUB      heaven for the low end: a clean low shelf at 80 Hz, a warm envelope-normalised 2nd
                   harmonic of the bass (so it is felt on small speakers too), and a soft mono bloom
                   that swells in the gaps after bass notes. All of it backs off as the bass gets loud,
                   so a big low end is never pushed into the output limiter.
          AUTO     loudness-matched output (K-weighted), OUTPUT trims on top.
    */
    class SilkStage
    {
    public:
        struct Settings
        {
            float smooth = 4.0f, air = 4.0f, warmth = 3.0f, body = 2.0f;   // 0..10 on the knobs (up to 30 with MULTIPLY)
            float sub = 0.0f;        // SUB: heaven for the low end (0..10)
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
        float getMatchDb() const noexcept     { return autoDb; }   // MATCH: the loudness-match gain it is applying

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
            SvfState triSplit;            // the triode curve acts above 120 Hz only
            BiquadState subShelf;         // SUB
            SvfState subLow, glowBand;
            float glowEnv = 1.0e-4f;
            float triEnv = 1.0e-3f;       // ... on the envelope-normalised signal
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
        SvfCoeffs airBand, airPost, airTop, warmBand, warmPost, warmTop, triSplitCoeffs;
        float triEnvAtt = 0.0f, triEnvRel = 0.0f;
        BiquadCoeffs detectHpCoeffs;
        BiquadState detectHp1, detectHp2;

        // SUB
        BiquadCoeffs subShelf;
        SvfCoeffs subLowCoeffs, glowBandCoeffs;
        float subShelfDb = 1000.0f, glowMix = 0.0f, bloomMix = 0.0f, bassPower = 0.0f, bassFast = 0.0f, bassSlow = 0.0f;
        float bassK = 0.0f, bassFastAtt = 0.0f, bassFastRel = 0.0f, bassSlowK = 0.0f, bloomLpK = 0.0f;
        BiquadState bassLp1, bassLp2;
        BiquadCoeffs bassLpCoeffs;
        std::array<std::vector<float>, 2> bloomLine;
        std::array<int, 2> bloomPos {}, bloomLen {};
        std::array<float, 2> bloomLp {};
        float bloomIn = 0.0f;
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
          SHIMMER  pitch-shifted copies of the tail fed back into the network: an octave up, and an
                   octave and a fifth up, quieter - the tail rises like a choir.
          MOD      slow delay modulation for a lusher tail, and a slow drift of the whole tail around
                   the stereo field (~20 s a cycle), so the space moves.
          TONE     dark (warm plate) .. airy (bright hall).
          DUCK     the tail sits ~10 dB lower while the programme is busy and blooms in the gaps,
                   so dialogue, clicks and footsteps stay dry and precise. Keyed above 200 Hz, so
                   bass coming and going does not pump the tail.
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

        // Shimmer: two two-tap delay pitch shifters, +12 st and +19 st (an octave and a fifth above)
        Delay shimmerLine;
        float shimmerPhase = 0.0f, shimmerWindow = 2400.0f, shimmerHpK = 0.0f, shimmerHpState = 0.0f, shimmerFeed = 0.0f;
        float fifthPhase = 0.0f, fifthWindow = 2400.0f;
        float driftPhase = 0.0f;   // the tail's slow drift around the stereo field (MOD)

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

        /** HEAVEN: what the unit does with the level of the sound it has made.

            SERAPH changes the programme's loudness as a side effect of what it does - space and
            air add level, smoothing takes it away, and how much depends on the material. This
            holds that steady. In STABLE it measures what came in and what is going out and
            works the output back toward the input, so the effect is loud enough to hear and
            never louder than the music. In LIFT + STABLE it first adds gain, then holds *that*
            steady, which is what you want when the source is quiet to begin with.

            `amount` is how firmly it holds (0 = leave the level alone); `lift` is the extra
            gain in LIFT mode. Both are one knob on the panel, with a button to swap modes. */
        struct HeavenSettings
        {
            float amount = 0.0f;     // 0..1
            float lift = 0.0f;       // 0..1 extra gain, LIFT + STABLE only
            bool liftMode = false;
            float strength = 1.0f;
            bool autoHeaven = false; // AUTO: the unit tunes its own heaven to the programme
            float autoAmount = 0.5f; // HEAVEN: how far AUTO moves the knobs toward what it chose (0..1)
        };

        /** What AUTO has chosen for this programme right now (display, tests). */
        struct AutoChoice { float space = 0, decayS = 0, shimmer = 0, tone = 0, width = 1, air = 0, sub = 0; };
        const AutoChoice& getAutoChoice() const noexcept { return autoChoice; }

        struct Settings
        {
            int mode = off;   // the plugin parameter defaults to HEAVEN; engine users opt in
            SilkStage::Settings silk {};
            HaloStage::Settings halo {};
            HeavenSettings heaven {};
        };

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            silk.prepare (sampleRate);
            halo.prepare (sampleRate);
            limiterRelease = onePole (0.080, sampleRate);
            // LOUDNESS measures K-weighted, like a loudness meter: bass counts for little, so a bass
            // note coming and going does not swing the level it holds
            kHp = BiquadCoeffs::highPass (sr, 150.0, 0.7071);   // run twice: 24 dB/oct
            kShelf = BiquadCoeffs::highShelf (sr, 1500.0, 0.7071, 4.0);
            reset();
        }

        void reset()
        {
            silk.reset();
            halo.reset();
            limiterGain = 1.0f;
            dryLevel = wetLevel = 1.0e-8f;
            for (auto* st : { &dryHp, &dryHp2, &dryShelf, &wetHp, &wetHp2, &wetShelf }) st->reset();
            heavenGain = 1.0f;
            heavenDb = 0.0f;

            // AUTO starts from a neutral programme, not from nothing
            autoBlend = 0.0f;
            crestDb = 12.0f; widthRatio = 0.2f; brightDb = -14.0f; bassBalanceDb = 0.0f;
            aFast = 1.0e-8f; aPeak = crestAcc = lp8 = lp4 = lp1 = lpB1 = lpB2 = 0.0f;
            choose();
        }

        void process (float* const* channels, int numChannels, int numSamples, const Settings& in) noexcept
        {
            // What came in, before SERAPH touches it: the reference the level is held against
            measure (channels, numChannels, numSamples, dryLevel, dryHp, dryHp2, dryShelf);

            // AUTO: listen to the programme and move the knobs toward the heaven it calls for
            analyse (channels, numChannels, numSamples);
            Settings s = in;
            const float a = in.heaven.autoHeaven && in.mode != off ? std::clamp (in.heaven.autoAmount, 0.0f, 1.0f) : 0.0f;
            autoBlend += (a - autoBlend) * (1.0f - std::exp (-(float) numSamples / (0.5f * (float) sr)));
            if (autoBlend > 1.0e-4f)
            {
                auto toward = [b = autoBlend] (float user, float chosen) { return user + (chosen - user) * b; };
                s.halo.space   = toward (in.halo.space, autoChoice.space);
                s.halo.decayS  = toward (in.halo.decayS, autoChoice.decayS);
                s.halo.shimmer = toward (in.halo.shimmer, autoChoice.shimmer);
                s.halo.tone    = toward (in.halo.tone, autoChoice.tone);
                s.halo.width   = toward (in.halo.width, autoChoice.width);
                s.silk.air     = toward (in.silk.air, autoChoice.air);
                s.silk.sub     = toward (in.silk.sub, autoChoice.sub);
            }

            silk.process (channels, numChannels, numSamples, s.silk, s.mode >= silkOnly ? 1.0f : 0.0f);
            halo.process (channels, numChannels, numSamples, s.halo, s.mode == heaven ? 1.0f : 0.0f);
            applyHeaven (channels, numChannels, numSamples, s.heaven);

            // (No limiter here any more: the engine's output limiter, after this unit, looks after full
            // scale with lookahead - this one grabbed every bass cycle and crushed loud bass.)
        }

        float getLimiterDb() const noexcept { return gainReductionDb; }
        float getHeavenDb() const noexcept { return heavenDb; }

        const SilkStage& getSilk() const noexcept { return silk; }
        float getAutoBlend() const noexcept { return autoBlend; }   // how far AUTO has taken the knobs (0..1)
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
        /** AUTO's ears: crest factor (percussive / sustained), stereo width, brightness and the low end,
            each averaged over ~4 s, turned into the settings a heavenly mix of this programme wants. */
        void analyse (float* const* ch, int numChannels, int n) noexcept
        {
            const int chans = std::min (numChannels, 2);
            const float fs = (float) sr;
            const float kFast = 1.0f - std::exp (-1.0f / (0.05f * fs)), kPeak = std::exp (-1.0f / (0.30f * fs));
            const float kLp8 = 1.0f - std::exp (-2.0f * 3.14159265f * 8000.0f / fs), kLp4 = 1.0f - std::exp (-2.0f * 3.14159265f * 4000.0f / fs);
            const float kLp1 = 1.0f - std::exp (-2.0f * 3.14159265f * 1200.0f / fs), kLpB = 1.0f - std::exp (-2.0f * 3.14159265f * 120.0f / fs);
            double hf = 0, pres = 0, bass = 0, mid = 0, side = 0, mono = 0;
            for (int i = 0; i < n; ++i)
            {
                const float l = ch[0][i], r = chans == 2 ? ch[1][i] : l;
                const float m = 0.5f * (l + r), sd = 0.5f * (l - r);
                aFast += (m * m - aFast) * kFast;
                aPeak = std::max (std::abs (m), aPeak * kPeak);
                lp8 += (m - lp8) * kLp8; lp4 += (m - lp4) * kLp4; lp1 += (m - lp1) * kLp1;
                lpB1 += (m - lpB1) * kLpB; lpB2 += (lpB1 - lpB2) * kLpB;
                const float h = m - lp8, p = lp4 - lp1, md = lp1 - lpB2;
                hf += h * h; pres += p * p; bass += lpB2 * lpB2; mid += md * md; side += sd * sd; mono += m * m;
                crestAcc += aPeak / std::sqrt (aFast + 1.0e-12f);
            }
            const float k = 1.0f - std::exp (-(float) n / (4.0f * fs));
            auto db = [] (double p) { return (float) (10.0 * std::log10 (p + 1.0e-20)); };
            if (mono / std::max (1, n) < 1.0e-8)   // silence teaches it nothing
            {
                crestAcc = 0.0f;
                return;
            }
            crestDb += (20.0f * std::log10 (std::max (1.0f, crestAcc / (float) n)) - crestDb) * k;
            crestAcc = 0.0f;
            widthRatio += ((float) (side / (mono + 1.0e-20)) - widthRatio) * k;
            brightDb += (db (hf) - db (pres) - brightDb) * k;
            bassBalanceDb += (db (bass) - db (mid) - bassBalanceDb) * k;
            choose();
        }

        /** AUTO's choice from what it has heard so far (before anything: a neutral, typical programme). */
        void choose() noexcept
        {
            auto sat = [] (float x) { return std::clamp (x, 0.0f, 1.0f); };
            const float sustained = sat ((13.0f - crestDb) / 6.0f);    // pads, strings, drones
            const float sparse = sat ((crestDb - 14.0f) / 8.0f);       // single hits with room between them
            const float mononess = sat (1.0f - widthRatio / 0.3f);
            const float dull = sat ((-14.0f - brightDb) / 14.0f);
            const float thin = sat ((-2.0f - bassBalanceDb) / 10.0f);

            autoChoice.space   = 0.16f + 0.22f * sustained + 0.08f * sparse;
            autoChoice.decayS  = 1.6f + 2.8f * sustained;
            autoChoice.shimmer = 0.04f + 0.36f * sustained;
            autoChoice.tone    = 0.45f + 0.35f * dull;
            autoChoice.width   = 1.1f + 0.5f * mononess;
            autoChoice.air     = 3.0f + 4.0f * dull;
            autoChoice.sub     = 1.0f + 5.0f * thin;
        }

        /** K-weighted mean power over ~2 s, time-based (the same whatever the host's block size):
            it follows a passage, not a note. */
        void measure (float* const* channels, int numChannels, int numSamples, float& level, BiquadState& hp, BiquadState& hp2, BiquadState& shelf) noexcept
        {
            const int chans = std::min (numChannels, 2);
            double sum = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                float m = 0.0f;
                for (int c = 0; c < chans; ++c)
                    m += channels[c][i];
                const float k = shelf.process (kShelf, hp2.process (kHp, hp.process (kHp, m / (float) std::max (1, chans))));
                sum += (double) k * k;
            }
            const float power = (float) (sum / std::max (1, numSamples));
            level += (power - level) * (1.0f - std::exp (-(float) numSamples / (2.0f * (float) sr)));
        }

        void applyHeaven (float* const* channels, int numChannels, int numSamples, const HeavenSettings& h) noexcept
        {
            const float amount = std::clamp (h.amount, 0.0f, 1.0f) * std::clamp (h.strength, 0.0f, 5.0f);
            if (amount <= 1.0e-4f)
            {
                heavenGain += (1.0f - heavenGain) * 0.02f;
                heavenDb = 20.0f * std::log10 (std::max (1.0e-3f, heavenGain));
                return;
            }

            measure (channels, numChannels, numSamples, wetLevel, wetHp, wetHp2, wetShelf);

            // Where the output should sit: back at the input's loudness, plus the lift if asked.
            // (Levels are powers: the ratio's square root is the gain; the wet level is measured
            // after this gain, so divide it back out.)
            const float liftDb = h.liftMode ? 12.0f * std::clamp (h.lift, 0.0f, 1.0f) : 0.0f;
            const float target = std::sqrt (std::max (1.0e-10f, dryLevel)) * std::pow (10.0f, liftDb * 0.05f);
            const float made = std::sqrt (std::max (1.0e-10f, wetLevel)) / std::max (1.0e-3f, heavenGain);
            const float wanted = std::clamp (target / std::max (1.0e-5f, made), 0.25f, 5.6f);

            // Hold it only as firmly as the knob asks, and move slowly (~3 s): this is a level policy,
            // not a compressor, so it must never breathe with the music.
            const float blended = 1.0f + (wanted - 1.0f) * std::min (1.0f, amount);
            heavenGain += (blended - heavenGain) * (1.0f - std::exp (-(float) numSamples / (3.0f * (float) sr)));
            heavenDb = 20.0f * std::log10 (std::max (1.0e-3f, heavenGain));

            const int chans = std::min (numChannels, 2);
            for (int c = 0; c < chans; ++c)
                for (int i = 0; i < numSamples; ++i)
                    channels[c][i] *= heavenGain;
        }

        SilkStage silk;
        HaloStage halo;
        AutoChoice autoChoice { 0.197f, 2.07f, 0.10f, 0.45f, 1.27f, 3.0f, 1.0f };   // = choose() on a neutral programme
        float autoBlend = 0.0f, aFast = 1.0e-8f, aPeak = 0.0f, crestAcc = 0.0f, crestDb = 12.0f, widthRatio = 0.2f;
        float brightDb = -14.0f, bassBalanceDb = 0.0f, lp8 = 0, lp4 = 0, lp1 = 0, lpB1 = 0, lpB2 = 0;

        double sr = 48000.0;
        BiquadCoeffs kHp, kShelf;
        BiquadState dryHp, dryHp2, dryShelf, wetHp, wetHp2, wetShelf;
        float dryLevel = 1.0e-8f, wetLevel = 1.0e-8f, heavenGain = 1.0f, heavenDb = 0.0f;
        float limiterGain = 1.0f, limiterRelease = 0.99f, gainReductionDb = 0.0f;
    };
}
