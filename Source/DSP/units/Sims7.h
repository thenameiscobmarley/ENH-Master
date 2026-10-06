#pragma once

#include "Sims6.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <juce_dsp/juce_dsp.h>

/*  VOCAL IDENTITY PROCESSOR (3.8.1.1): makes a voice sound like another person - not a chipmunk, a robot or a
    vocoder. */
namespace enh::dsp::units
{
    // ------------------------------------------------------------------------------------------------
    /** VOCAL IDENTITY PROCESSOR (VIP-12): the voice's pitch, its vocal tract and its glottal source, each changed
        on its own.
          - Pitch: PSOLA. The voice is cut into grains, one per glottal pulse (pitch marks one period apart, on the
            pulse), and the grains are laid down closer together (higher) or further apart (lower). Each grain
            keeps its own spectrum, so the formants stay where they were: the same person, singing higher.
          - Vocal tract: each grain is resampled (read faster: a shorter tract, formants up; slower: a longer
            one), which moves the formants and leaves the pitch alone. Age and gender are mostly this.
          - Glottal source: jitter (period to period) and shimmer (pulse to pulse), vibrato, an elderly tremor,
            vocal fry (low, irregular, alternating pulses as the level falls at the ends of phrases), and
            breath: noise shaped by the new voice's own spectral envelope (cepstrum), pulsing with the glottis.
            WHISPER replaces the pulses with that noise entirely.
        Unvoiced sounds (s, f, sh, t, k) are never pitch-shifted: they are rebuilt grain for grain where they
        were (only their tract moves), so they stay noise, never a tone; a fresh transient is never doubled.
        Only voice is changed: anything else (music, a game) passes as it came (delayed by the same latency).
        The level follows the voice that came in (within 0.5 dB); its peaks are held to the input's +3 dB; a
        feedback howl building up (a live mic near speakers) mutes it and lights a lamp.
        LIVE: about 22 ms; HQ (longer grains, centred pitch analysis): about 79 ms. Reported to the host only
        while it is on (RackUnit::latencySamples).
        State: [0] input pitch (Hz/1000), [1] output pitch, [2] voiced, [3] voice active, [4] de-ess reduction
        (0..1 of 12 dB), [5] howl muted, [6] latency (ms/100), [7] tract scale (0.5..1.5 -> 0..1), [8] F1 in
        (kHz/2), [9] F2 in (kHz/4), [10] F1 out, [11] F2 out, [12..43] the input's envelope (32 bands, 0..1 of
        72 dB), [44..75] the output's, [76..107] pitch history in, [108..139] out (Hz/500). */
    class VoiceEngine : public RackUnit
    {
    public:
        /** What a voice unit asks of the engine (each unit maps its own knobs to this: VoiceIdentity, PitchCorrector,
            VocalStation). The VOCAL IDENTITY PROCESSOR's knobs, in its units, plus the pitch correction. */
        struct Knobs
        {
            float character = 0, mode = 0, pitch = 0, formant = 0, age = 0, gender = 0, breath = 0, fry = 0, rough = 0, vibrato = 0,
                  gate = -60, deess = 0, mix = 100, output = 0, multiply = 1, strength = 100;
            // pitch correction: tune 0..1 (how much of the way to the note), the key 0..11 (C..B), the scale
            // (Scale), speed in ms (0: instant), humanize 0..10; autoTune: AUTO (key and scale found, amount from
            // how far off, formants follow a big pitch move, level kept)
            float tune = 0, key = 0, scale = 1, speedMs = 40, humanize = 3; bool autoTune = false;
            float smooth = 5;   // SMOOTH 0..10
        };
        enum Scale { chromatic = 0, major, minor, harmonicMinor, majorPentatonic, minorPentatonic, blues, numScales };
        /** The key AUTO found (0..23: C major .. B major, then C minor .. B minor; -1: not sure yet). */
        int foundKey() const noexcept { return keyFound; }
        enum Character { custom = 0, maleToFemale, femaleToMale, child, teen, elderlyMan, elderlyWoman, bigMan, smallPerson, announcer, husky, whisper, numCharacters };
        /** What each character moves: pitch (semitones), tract (octaves of formant shift), breath, fry,
            roughness, vibrato, tremor (0..10) and whisper (0..1). The knobs add to it. */
        /** A character: where the voice goes. targetHz / targetCm (0: none - a relative move): the pitch and vocal tract
            of the person it becomes - the unit learns the speaker's own and moves them THERE, so a voice already high
            is not pushed into a squeak (pitchSt / tractOct: the move until it has learnt them, and for relative ones);
            intonation: how much the new person's pitch rises and falls (1: as the speaker's). */
        struct Shape { float pitchSt, tractOct, breath, fry, rough, vibrato, tremor, whisper, targetHz, targetCm, intonation; };
        static constexpr std::array<Shape, numCharacters> shapes {{
            {  0.0f,  0.000f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,   0.0f,  0.0f, 1.00f },   // CUSTOM: the knobs alone
            {  9.0f,  0.227f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 210.0f, 14.6f, 1.15f },   // M TO F: a woman - 210 Hz, a 14.6 cm tract, livelier
            { -9.0f, -0.234f, 0.0f, 2.0f, 1.0f, 0.0f, 0.0f, 0.0f, 112.0f, 17.3f, 0.90f },   // F TO M: a man - 112 Hz, 17.3 cm
            { 11.0f,  0.400f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 280.0f, 12.0f, 1.25f },   // CHILD: 280 Hz, 12 cm, sing-song
            {  3.0f,  0.111f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,   0.0f,  0.0f, 1.05f },   // TEEN (relative: a little younger)
            { -1.0f, -0.044f, 4.0f, 1.0f, 6.0f, 0.0f, 5.0f, 0.0f, 125.0f, 17.6f, 0.90f },   // ELDERLY MAN: rough, breathy, a tremor
            {  4.0f,  0.111f, 4.0f, 1.0f, 5.0f, 0.0f, 5.0f, 0.0f, 180.0f, 14.8f, 0.95f },   // ELDERLY WOMAN
            { -7.0f, -0.322f, 1.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f,  88.0f, 19.5f, 0.85f },   // BIG MAN: 88 Hz, a 19.5 cm tract, flat
            {  6.0f,  0.322f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 240.0f, 12.8f, 1.15f },   // SMALL PERSON
            { -3.0f, -0.105f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,  98.0f, 18.0f, 1.10f },   // ANNOUNCER: deep and expressive
            { -2.0f, -0.059f, 6.0f, 3.0f, 4.0f, 0.0f, 0.0f, 0.0f,   0.0f,  0.0f, 0.95f },   // HUSKY (relative)
            {  0.0f,  0.000f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,   0.0f,  0.0f, 1.00f } }};// WHISPER (relative)

        /** What it has learnt of the speaker: their usual pitch (Hz), vocal tract (cm), and how sure (0..1). */
        float learnedPitchHz() const noexcept { return spkPitchHz; }
        float learnedTractCm() const noexcept { return spkTractCm; }
        float learnedConfidence() const noexcept { return spkConf; }

        int latencySamples() const noexcept override { return latency; }
        int maxLatencySamples() const noexcept override { return configFor (1, sr).latency + 8; }
        bool strengthInside() const noexcept override { return true; }
        /** Whether a feedback howl muted it (the lamp). */
        bool howlMuted() const noexcept { return howl; }

        int displayState (float* o, int max) const noexcept override
        {
            if (max < 145) return 0;
            o[0] = f0In / 1000.0f; o[1] = f0Out / 1000.0f; o[2] = voicedNow ? 1.0f : 0.0f; o[3] = vadGain;
            o[4] = std::clamp (-deessDb / 12.0f, 0.0f, 1.0f); o[5] = howl ? 1.0f : 0.0f;
            o[6] = (float) latency / (float) sr * 10.0f; o[7] = std::clamp (alphaNow - 0.5f, 0.0f, 1.0f);
            o[8] = formIn[0] / 2000.0f; o[9] = formIn[1] / 4000.0f; o[10] = formOut[0] / 2000.0f; o[11] = formOut[1] / 4000.0f;
            for (int b = 0; b < 32; ++b) { o[12 + b] = envIn[(size_t) b]; o[44 + b] = envOut[(size_t) b]; }
            for (int i = 0; i < 32; ++i)
            {
                const int k = (histPos + i) % 32;
                o[76 + i] = histIn[(size_t) k] / 500.0f; o[108 + i] = histOut[(size_t) k] / 500.0f;
            }
            o[140] = (float) std::max (0, keyFound) / 23.0f; o[141] = (float) scaleUsed / 6.0f;
            o[142] = tuneNote / 127.0f; o[143] = std::clamp (0.5f + tuneDevCents / 100.0f, 0.0f, 1.0f); o[144] = tuneAmt > 0.0f ? 1.0f : 0.0f;
            return 145;
        }

        /** The timing of a mode at a rate (samples): LIVE 0, HQ 1. */
        struct Config { int maxHalf, ahead, markLag, rMax, latency, yinWin, hop; };
        static Config configFor (int mode, double rate) noexcept
        {
            Config c {};
            auto s = [rate] (double sec) { return (int) std::lround (sec * rate); };
            const int pMax = s (1.0 / 60.0);
            c.hop = s (mode == 0 ? 0.00267 : 0.004);   // (HQ: longer windows, centred and median-smoothed - every 4 ms is plenty)
            if (mode == 0)
            {
                c.maxHalf = s (0.008); c.ahead = s (0.0075); c.yinWin = s (0.012); c.markLag = 0;   // (ahead: room to blend each grain from its two pulses)
                c.rMax = c.maxHalf + c.ahead + s (0.002);
            }
            else
            {
                c.maxHalf = s (0.0175); c.ahead = s (0.0085); c.yinWin = s (0.025);
                c.markLag = (c.yinWin + pMax) / 2 + 2 * c.hop;
                c.rMax = c.markLag + (5 * pMax) / 4 + c.ahead;
            }
            c.latency = c.rMax + c.maxHalf;
            return c;
        }

    protected:
        virtual Knobs knobsFrom (const float* q) const noexcept = 0;

    private:
        // --- rings, indexed by absolute time & mask -------------------------------------------------
        static constexpr int ringBits = 15, ringSize = 1 << ringBits, ringMask = ringSize - 1;
        std::vector<float> inMid, inL, inR, inLow, lpRing, acc, wsum, pulse;
        sim::Svf lowSplit, lowSplit2;
        float voicing = 0.0f, lastAp = 1.0f;   // (how voice-like the sound is now: breath and whisper only as much)
        std::vector<long long> lastVoicedRing;
        std::vector<float> levelRing;   // the input's level (dB) as it came, for fry
        long long t = 0, nextS = 0;
        int mode = -1, latency = 0;
        Config cfg {};

        // --- pitch: YIN on a decimated, low-passed copy ------------------------------------------------
        int dec = 4; float decAcc = 0.0f; int decCount = 0;
        std::vector<float> decRing; int decPos = 0; static constexpr int decSize = 4096;
        std::vector<float> yinD, yinX, yinE;
        static constexpr int hannTableSize = 2048;   // (half a Hann window, 0 .. pi)
        std::array<float, hannTableSize + 2> hannTable {};
        float pitchDev = 0.0f, prevPeriod = 0.0f; int voicedRun = 0, unvoicedRun = 100;   // (how much the pitch wobbles, cents per frame: a voice never holds still)
        std::array<float, 16> wobble {}; int wobblePos = 0, wobbleCount = 0;
        bool prevGrainVoiced = false;
        sim::Svf pitchLp, markLp, decLp;
        int sinceHop = 0;
        struct Frame { long long centre; float period; float ap; bool voiced; };
        static constexpr int numFrames = 64;
        std::array<Frame, numFrames> frames {};
        std::array<float, numFrames> spanBuf {};   // (the last quarter second's periods, sorted for the pitch span)
        int frameCount = 0;
        bool lastFrameVoiced = false, voiceLike = false; float voiceEvidence = 0.0f;
        float levelFastPow = 0.0f, levelDb = -120.0f;

        // --- pitch marks ------------------------------------------------------------------------------
        struct Mark { long long pos; float period; bool voiced; };
        static constexpr int numMarks = 2048;
        std::vector<Mark> marks = std::vector<Mark> ((size_t) numMarks);
        long long markHead = 0;         // marks written
        long long markRead = 0;         // the synthesis' search starts here
        long long lastMarkPos = 0, markWait = 0, lastVoicedMarkPos = -1000000; bool lastMarkVoiced = false; float markSign = 1.0f, lastVoicedPeriod = 0.0f;

        // --- transients ------------------------------------------------------------------------------
        float envFast = 0.0f, envSlow = 0.0f; long long lastTransient = -1000000;

        // --- the source -------------------------------------------------------------------------------
        unsigned rng = 0x2545f491u;
        float uni() noexcept { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) / 16777216.0f; }
        float gauss() noexcept { return (uni() + uni() + uni() - 1.5f) * 2.0f; }   // (about unit variance)
        double tremPhase = 0.0; float tremRate = 5.5f;
        bool fryOdd = false;
        float alphaNow = 1.0f, ratioNow = 1.0f, speechDb = -30.0f, intonation = 1.0f;
        float smoothNow = 0.5f, markStepAvg = 0.0f;
        float vqPresence = 1.0f, vqAir = 1.0f, vqWarm = 1.0f, vqLowCut = 0.0f, vqPresenceT = 1.0f, vqAirT = 1.0f, vqWarmT = 1.0f, vqLowCutT = 0.0f;
        sim::Svf vqPresenceSvf, vqAirSvf, vqWarmSvf, vqHp;   // (SMOOTH 0..1; the marks' running period)
        sim::Svf hfIn, hfOut; float hfInEnv = 0.0f, hfOutEnv = 0.0f, hfGain = 1.0f;

        // --- breath & whisper: noise shaped by the output's envelope -----------------------------------
        int fftOrder = 9, nFft = 512, nHop = 128;
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> envRing, fftBuf, cep, logEnv, hann, noiseOla;
        int envPos = 0, sinceNoise = 0, olaPos = 0;
        float noiseRmsFast = 0.0f, wetRmsFast = 0.0f;
        sim::Svf breathHp;
        bool noiseNeeded = false;
        const bool howlDebug = std::getenv ("VOICE_HOWL_DEBUG") != nullptr, vadDebug = std::getenv ("VOICE_VAD_DEBUG") != nullptr, grainDebug = std::getenv ("VOICE_GRAIN_DEBUG") != nullptr,
                   noFry = std::getenv ("VOICE_NO_FRY") != nullptr, fryDebug = std::getenv ("VOICE_FRY_DEBUG") != nullptr;
        long long fryGrains = 0, allGrains = 0;
        // (VOICE_PROFILE=1: where the time goes, printed when the unit is destroyed - a development aid)
        const bool profile = std::getenv ("VOICE_PROFILE") != nullptr;
        double profYin = 0, profMarks = 0, profGrain = 0, profOut = 0, profNoise = 0, profAll = 0; long long profGrains = 0, profSamples = 0;
        static double nowS() noexcept { return (double) std::chrono::steady_clock::now().time_since_epoch().count() * 1.0e-9; }
    public:
        ~VoiceEngine() override
        {
            if (fryDebug && allGrains > 0) std::printf ("VOICE_FRY %lld of %lld voiced grains with fry (speech level %.1f dB)\n", fryGrains, allGrains, speechDb);
            if (profile && profAll > 0.0)
                std::printf ("VOICE_PROFILE %.2f s audio: all %.3f s | yin %.3f marks %.3f grains %.3f (%.1f grain samples per sample) noise %.3f out %.3f\n",
                             (double) profSamples / sr, profAll, profYin, profMarks, profGrain, (double) profGrains / std::max (1LL, profSamples), profNoise, profOut);
        }
    private:
        int howlCount = 0; std::array<float, 8> howlPast {}; float howlScoreS = 0.0f, howlQuietS = 0.0f; bool howl = false; float howlGain = 1.0f;

        // --- the output stage -------------------------------------------------------------------------
        sim::Svf deessSplit;
        float deessEnvHp = 0.0f, deessEnvAll = 0.0f, deessGain = 1.0f, deessDb = 0.0f;
        float dryPow = 1.0e-6f, wetPow = 1.0e-6f, matchGain = 1.0f;
        float peakHold = 0.0f; float limGain = 1.0f;
        std::array<float, 1024> peakBlocks {}; float peakBlockNow = 0.0f, peakBlocksMax = 0.0f; int peakBlockPos = 0, peakBlockFill = 0;
        float vadGain = 0.0f;
        float outGain = 1.0f;

        // --- display ----------------------------------------------------------------------------------
        float f0In = 0.0f, f0Out = 0.0f; bool voicedNow = false;
        std::array<float, 2> formIn {}, formOut {};
        std::array<float, 32> envIn {}, envOut {};
        std::array<float, 32> histIn {}, histOut {}; int histPos = 0, histTick = 0;
        std::vector<float> inEnvRing; int inEnvPos = 0;

        void prepareUnit (double s, int) override
        {
            for (auto* v : { &inMid, &inL, &inR, &inLow, &lpRing, &acc, &wsum, &pulse, &levelRing }) v->assign ((size_t) ringSize, 0.0f);
            lowSplit.set (s, 50.0, 0.7071); lowSplit2.set (s, 50.0, 0.7071);
            hfIn.set (s, 4000.0, 0.7071); hfOut.set (s, 4000.0, 0.7071);
            vqPresenceSvf.set (s, 2200.0, 0.6); vqAirSvf.set (s, 6000.0, 0.7071); vqWarmSvf.set (s, 180.0, 0.8); vqHp.set (s, 100.0, 0.7071);
            dec2 = std::max (1, (int) std::lround (s / 11025.0)); lpcAa.set (s, 0.42 * s / dec2, 0.7071);
            lastVoicedRing.assign ((size_t) ringSize, -1000000);
            dec = std::max (1, (int) std::lround (s / 6000.0));   // (pitch to 500 Hz: a 6 kHz copy is plenty, parabolic peaks)
            decRing.assign ((size_t) decSize, 0.0f);
            yinD.assign ((size_t) (s / dec / 55.0) + 4, 0.0f);
            yinX.assign ((size_t) (s / dec / 55.0 + 0.03 * s / dec) + 16, 0.0f);
            yinE.assign (yinX.size() + 1, 0.0f);
            howlEnv.assign ((size_t) nFftHowl (s), 0.0f);
            { const int order = s > 120000.0 ? 14 : s > 60000.0 ? 13 : 12; keyN = 1 << order; keyFft = std::make_unique<juce::dsp::FFT> (order);
              keyRing.assign ((size_t) keyN, 0.0f); keyBuf.assign ((size_t) (2 * keyN), 0.0f); }
            for (int i = 0; i < howlN; ++i) howlWin[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) howlN);
            for (int i = 0; i < hannTableSize + 2; ++i) hannTable[(size_t) i] = 0.5f + 0.5f * std::cos (3.14159265f * (float) i / (float) hannTableSize);
            pitchLp.set (s, 1000.0, 0.7); markLp.set (s, 900.0, 0.6); decLp.set (s, 0.3 * s / dec, 0.7);
            fftOrder = s > 60000.0 ? 10 : 9; nFft = 1 << fftOrder; nHop = nFft / 4;
            fft = std::make_unique<juce::dsp::FFT> (fftOrder);
            envRing.assign ((size_t) nFft, 0.0f); inEnvRing.assign ((size_t) nFft, 0.0f);
            fftBuf.assign ((size_t) (2 * nFft), 0.0f); cep.assign ((size_t) (2 * nFft), 0.0f);
            logEnv.assign ((size_t) (nFft / 2 + 1), 0.0f); hann.assign ((size_t) nFft, 0.0f);
            for (int i = 0; i < nFft; ++i) hann[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) nFft);
            noiseOla.assign ((size_t) (2 * nFft), 0.0f);
            breathHp.set (s, 1800.0, 0.6); deessSplit.set (s, 4500.0, 0.7);
            mode = -1; configure (0);
        }

        void configure (int m)
        {
            mode = m; cfg = configFor (m, sr); latency = cfg.latency;
            clearCore();
        }

        void clearCore() noexcept
        {
            tuneNote = 0.0f; tuneMidiSlow = -1.0f; tuneCorr = 0.0f; tuneDevCents = 0.0f;
            pitchHist.fill (0.0f); tractHist.fill (0.0f); pitchHistW = tractHistW = spkPitchHz = spkTractCm = spkConf = 0.0f;
            dec2Count = dec2Pos = sinceLpc = 0; dec2Ring.fill (0.0f); lpcAa.reset();
            chroma.fill (0.0f); chromaWeight = 0.0f; keyFound = -1; keyCandidate = -1; keyVotes = 0; keyPos = sinceKey = 0; sinceKeyEval = 0;
            std::fill (keyRing.begin(), keyRing.end(), 0.0f);
            for (auto* v : { &inMid, &inL, &inR, &inLow, &lpRing, &acc, &wsum, &pulse, &levelRing }) std::fill (v->begin(), v->end(), 0.0f);
            lowSplit.reset(); lowSplit2.reset(); voicing = 0.0f;
            hfIn.reset(); hfOut.reset(); hfInEnv = hfOutEnv = 0.0f; hfGain = 1.0f; markStepAvg = 0.0f;
            vqPresenceSvf.reset(); vqAirSvf.reset(); vqWarmSvf.reset(); vqHp.reset();
            std::fill (lastVoicedRing.begin(), lastVoicedRing.end(), -1000000);
            std::fill (decRing.begin(), decRing.end(), 0.0f);
            t = 0; nextS = 0; pitchDev = 0.0f; prevPeriod = 0.0f; voicedRun = 0; unvoicedRun = 100; wobble.fill (0.0f); wobblePos = wobbleCount = 0; prevGrainVoiced = false; voiceLike = false; voiceEvidence = 0.0f; decAcc = 0.0f; decCount = 0; decPos = 0; sinceHop = 0; frameCount = 0; lastFrameVoiced = false;
            levelFastPow = 0.0f; levelDb = -120.0f;
            markHead = 0; markRead = 0; lastMarkPos = 0; markWait = 0; lastVoicedMarkPos = -1000000; lastVoicedPeriod = 0.0f; lastMarkVoiced = false; markSign = 1.0f;
            envFast = envSlow = 0.0f; lastTransient = -1000000;
            tremPhase = 0.0; fryOdd = false; alphaNow = 1.0f; ratioNow = 1.0f; speechDb = -30.0f;
            pitchLp.reset(); markLp.reset(); decLp.reset(); breathHp.reset(); deessSplit.reset();
            std::fill (envRing.begin(), envRing.end(), 0.0f); std::fill (inEnvRing.begin(), inEnvRing.end(), 0.0f);
            std::fill (noiseOla.begin(), noiseOla.end(), 0.0f); std::fill (logEnv.begin(), logEnv.end(), 0.0f);
            envPos = inEnvPos = 0; sinceNoise = 0; olaPos = 0; noiseRmsFast = wetRmsFast = 0.0f;
            howlCount = 0; howlPast.fill (-120.0f); howlSmooth = -120.0f; std::fill (howlEnv.begin(), howlEnv.end(), 0.0f); howlPos = sinceHowl = 0;
            howlFreqs.fill (-1.0f); howlFreqPos = howlFreqCount = 0; howlGoneS = 0.0f; howlScoreS = howlQuietS = 0.0f; howl = false; howlGain = 1.0f;
            deessEnvHp = deessEnvAll = 0.0f; deessGain = 1.0f; deessDb = 0.0f;
            dryPow = wetPow = 1.0e-6f; matchGain = 1.0f; peakHold = 0.0f; limGain = 1.0f; peakBlocks.fill (0.0f); peakBlockNow = peakBlocksMax = 0.0f; peakBlockPos = peakBlockFill = 0; vadGain = 0.0f;
            f0In = f0Out = 0.0f; voicedNow = false; formIn = formOut = {}; envIn = envOut = {}; histIn = histOut = {};
        }

        void resetUnit() override { if (mode < 0) configure (0); else clearCore(); }

        // ---------------------------------------------------------------------------------------------
        float at (const std::vector<float>& r, long long i) const noexcept { return r[(size_t) (i & ringMask)]; }
        float& at (std::vector<float>& r, long long i) noexcept { return r[(size_t) (i & ringMask)]; }
        /** The input (mid) at a fractional time: cubic. */
        float readIn (double pos) const noexcept
        {
            const long long i = (long long) std::floor (pos); const float f = (float) (pos - (double) i);
            const float y0 = at (inMid, i - 1), y1 = at (inMid, i), y2 = at (inMid, i + 1), y3 = at (inMid, i + 2);
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }

        /** YIN on the newest decimated window: the period (full-rate samples) and its aperiodicity. */
        /** The newest decimated window's pitch: normalised cross-correlation (NCCF, as RAPT) - each lag's
            correlation divided by both windows' energy, so a voice getting louder or softer within the window
            (expressive speech does, fast) doesn't hide its period the way a plain difference function lets it.
            The earliest peak within 7 % of the best (no sub-octave slips), refined by a parabola; aperiodicity
            is 1 - its correlation. Period in full-rate samples. */
        void yin (float& period, float& ap) noexcept
        {
            const double fsd = sr / dec;
            const int tauMax = std::min ((int) yinD.size() - 2, (int) (fsd / 60.0)), tauMin = std::max (2, (int) (fsd / 500.0));
            const int w = std::max (8, cfg.yinWin / dec);
            const int len = w + tauMax + 1, start = decPos - len;   // (decRing positions, wrapped: copied out straight)
            for (int i = 0; i < len; ++i) yinX[(size_t) i] = decRing[(size_t) (((start + i) % decSize + decSize) % decSize)];
            const float* x = yinX.data();
            // energies of every w-long window (prefix sums of squares)
            yinE[0] = 0.0f;
            for (int i = 0; i < len; ++i) yinE[(size_t) i + 1] = yinE[(size_t) i] + x[i] * x[i];
            const float e0 = yinE[(size_t) w] - yinE[0];
            if (e0 < 1.0e-12f) { period = 0.0f; ap = 1.0f; return; }
            float rMax = -1.0f;
            for (int tau = tauMin - 1; tau <= tauMax + 1; ++tau)
            {
                float xy = 0.0f;
                for (int j = 0; j < w; ++j) xy += x[j] * x[j + tau];
                const float e1 = yinE[(size_t) (tau + w)] - yinE[(size_t) tau];
                const float r = xy / std::sqrt (e0 * e1 + 1.0e-20f);
                yinD[(size_t) tau] = r;
                if (tau >= tauMin && tau <= tauMax) rMax = std::max (rMax, r);
            }
            int best = -1;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (yinD[(size_t) tau] >= 0.93f * rMax && yinD[(size_t) tau] >= yinD[(size_t) (tau - 1)] && yinD[(size_t) tau] >= yinD[(size_t) (tau + 1)])
                    { best = tau; break; }
            if (best < 0 || rMax <= 0.0f) { period = 0.0f; ap = 1.0f; return; }
            float frac = 0.0f;
            {
                const float a = yinD[(size_t) (best - 1)], b2 = yinD[(size_t) best], c = yinD[(size_t) (best + 1)];
                const float den = a - 2.0f * b2 + c;
                if (std::abs (den) > 1.0e-9f) frac = std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f);
            }
            period = ((float) best + frac) * (float) dec;
            ap = 1.0f - std::clamp (yinD[(size_t) best], 0.0f, 1.0f);
        }

        /** The pitch at a time: LIVE the newest frame, HQ the median of the five frames around it. */
        Frame frameAt (long long pos) const noexcept
        {
            if (frameCount == 0) return { pos, 0.0f, 1.0f, false };
            int newest = (frameCount - 1) % numFrames;
            if (mode == 0) return frames[(size_t) newest];
            // the frame whose centre is nearest
            int idx = newest, n = std::min (frameCount, numFrames);
            for (int k = 0; k < n - 1; ++k)
            {
                const int prev = (idx - 1 + numFrames) % numFrames;
                if (std::abs (frames[(size_t) prev].centre - pos) < std::abs (frames[(size_t) idx].centre - pos)) idx = prev; else break;
            }
            std::array<float, 5> ps {}; int np = 0, nv = 0;
            const int age = (newest - idx + numFrames) % numFrames;   // (frames newer than idx)
            for (int d = -2; d <= 2; ++d)
            {
                if (d > age || age - d >= n) continue;   // (not written yet, or overwritten)
                const auto& f = frames[(size_t) ((idx + d + numFrames) % numFrames)];
                if (f.voiced) { ps[(size_t) np++] = f.period; ++nv; }
            }
            Frame r = frames[(size_t) idx];
            if (np >= 3)
            {
                std::sort (ps.begin(), ps.begin() + np);
                r.period = ps[(size_t) (np / 2)];
            }
            r.voiced = r.voiced && nv >= 2;
            return r;
        }

        void addMark (long long pos, float period, bool voiced) noexcept
        {
            marks[(size_t) (markHead % numMarks)] = { pos, period, voiced };
            ++markHead; lastMarkPos = pos; lastMarkVoiced = voiced;
        }

        /** Places pitch marks as far as the input (and, HQ, the centred pitch) allows. */
        void placeMarks() noexcept
        {
            const long long limit = t - cfg.markLag;
            if (limit < markWait) return;   // (the next mark can't be placed before then)
            for (int guard = 0; guard < 8; ++guard)
            {
                Frame f = frameAt (lastMarkPos + 64);
                // (a weak frame inside a syllable - up to 60 ms after a voiced one, the level still up - stays voiced at the
                //  last period: no snapping back to the old pitch for a few milliseconds)
                if (! (f.voiced && f.period > 8.0f) && lastMarkVoiced && lastMarkPos - lastVoicedMarkPos < (long long) (0.06 * sr)
                    && at (levelRing, lastMarkPos) > speechDb - 25.0f && lastVoicedPeriod > 8.0f)
                    { f.voiced = true; f.period = lastVoicedPeriod; }
                const bool voiced = f.voiced && f.period > 8.0f;
                if (voiced && f.period > 8.0f && frameAt (lastMarkPos + 64).voiced) { lastVoicedMarkPos = lastMarkPos; lastVoicedPeriod = f.period; }
                if (voiced)
                {
                    const float p = f.period;
                    long long from, to;
                    if (lastMarkVoiced && markHead > 0)
                    {
                        const long long pred = lastMarkPos + (long long) std::lround (p);
                        from = pred - (long long) (0.25f * p); to = pred + (long long) (0.25f * p);
                    }
                    else { from = lastMarkPos + 1; to = lastMarkPos + (long long) p; }
                    if (to > limit) { markWait = to; return; }
                    if (! lastMarkVoiced)
                    {
                        float mx = 0.0f, mn = 0.0f;
                        for (long long i = from; i <= to; ++i) { mx = std::max (mx, at (lpRing, i)); mn = std::min (mn, at (lpRing, i)); }
                        markSign = mx >= -mn ? 1.0f : -1.0f;
                    }
                    long long best = from; float bestV = -1.0e9f;
                    const float centre = 0.5f * (float) (from + to), span = std::max (1.0f, 0.5f * (float) (to - from));
                    for (long long i = from; i <= to; ++i)
                    {
                        const float v = markSign * at (lpRing, i) * (1.0f - 0.1f * std::abs ((float) i - centre) / span);
                        if (v > bestV) { bestV = v; best = i; }
                    }
                    addMark (best, p, true);
                }
                else
                {
                    const long long next = lastMarkPos + (long long) std::lround (sr * 0.0035 * (0.8 + 0.4 * uni()));
                    if (next > limit) { markWait = std::min (next, lastMarkPos + (long long) (0.0025 * sr)); return; }
                    addMark (next, 0.0f, false);
                }
            }
        }

        /** One synthesis grain at S. */
        void renderGrain (const Knobs& k, const Shape& sh, float strength) noexcept
        {
            const long long S = nextS;
            // the nearest mark that is known, no further ahead than `ahead`
            while (markRead + 1 < markHead && marks[(size_t) ((markRead + 1) % numMarks)].pos <= S) ++markRead;
            if (markHead - markRead > numMarks - 4) markRead = markHead - numMarks + 4;
            Mark m { S, 0.0f, false };
            if (markRead < markHead)
            {
                m = marks[(size_t) (markRead % numMarks)];
                if (markRead + 1 < markHead)
                {
                    const Mark n2 = marks[(size_t) ((markRead + 1) % numMarks)];
                    if (n2.pos - S < S - m.pos && n2.pos - S <= cfg.ahead) m = n2;
                }
                if (std::abs (m.pos - S) > (long long) std::max (m.period, (float) cfg.maxHalf)) m = { S, 0.0f, false };
            }
            const bool transient = S - lastTransient < (long long) (0.012 * sr) && S - lastTransient > -(long long) (0.004 * sr);
            // (a burst - t, k, p - is rebuilt where it was; a voiced onset - b, d, a vowel starting - is shifted like the rest)
            bool voiced = m.voiced && m.period > 8.0f;
            juce::ignoreUnused (transient);
            long long S2 = S;
            // voicing starts: the first grain sits on its mark (then each one a mark-to-mark step on), so with
            // nothing changed the pulses land exactly where they were
            if (voiced && ! prevGrainVoiced)
            {
                if (m.pos < S && markRead + 1 < markHead) { const Mark n2 = marks[(size_t) ((markRead + 1) % numMarks)]; if (n2.voiced && n2.pos - S <= cfg.ahead) m = n2; }
                if (m.pos >= S && m.pos - S <= cfg.ahead) S2 = m.pos; else voiced = false;
            }
            prevGrainVoiced = voiced;
            // (the next voiced mark after this one: the real period here)
            float markStep = m.period;
            for (long long k2 = markRead; k2 + 1 < markHead && k2 < markRead + 3; ++k2)
            {
                const Mark& a2 = marks[(size_t) (k2 % numMarks)];
                if (a2.pos == m.pos) { const Mark& b2 = marks[(size_t) ((k2 + 1) % numMarks)]; if (b2.voiced && b2.pos > m.pos) markStep = (float) (b2.pos - m.pos); break; }
            }
            if (markStep > 1.6f * m.period || markStep < 0.6f * m.period) markStep = m.period;
            // SMOOTH: the marks' own placement wobble averaged out (up to 60 % toward the running period; a voice's own
            // jitter is far slower than one mark's misplacement, and mostly stays)
            markStepAvg = markStepAvg <= 0.0f || std::abs (markStep - markStepAvg) > 0.2f * markStepAvg ? markStep : markStepAvg + 0.35f * (markStep - markStepAvg);
            markStep += 0.6f * smoothNow * (markStepAvg - markStep);

            // the source
            const float tex = strength * std::clamp (k.multiply, 0.25f, 3.0f);   // (MULTIPLY: the source's textures)
            const float rough = std::clamp ((sh.rough + k.rough) * tex, 0.0f, 10.0f) / 10.0f;
            const float vib = std::clamp ((sh.vibrato + k.vibrato) * tex, 0.0f, 10.0f) / 10.0f;
            const float trem = std::clamp ((sh.tremor + std::max (0.0f, k.age) * 0.5f) * tex, 0.0f, 10.0f) / 10.0f;
            const float fry = std::clamp ((sh.fry + k.fry) * tex, 0.0f, 10.0f) / 10.0f;
            float alpha = alphaNow, gain = 1.0f;
            double spacing;
            long long centre, centreB = 0; int hw; float blendB = 0.0f;   // (blendB: how much of the later pulse)
            if (voiced)
            {
                const double tS = (double) S / sr;
                float cents = vib * 45.0f * (float) std::sin (6.283185307179586 * 5.3 * tS)
                            + trem * 35.0f * (float) std::sin (6.283185307179586 * tremPhase);
                float ratio = ratioNow * std::exp2 (cents / 1200.0f);
                // the new person's intonation: their pitch rises and falls more (or less) round their usual pitch
                if (std::abs (intonation - 1.0f) > 1.0e-3f && spkPitchHz > 0.0f && strength > 0.0f)
                {
                    const float dev = std::log2 ((float) sr / markStep / spkPitchHz);
                    ratio *= std::exp2 (std::clamp (dev * (intonation - 1.0f) * std::min (1.0f, strength), -0.5f, 0.5f));
                }
                if (tuneAmt > 0.0f) ratio *= tuneRatio (markStep, k);   // (the pitch correction: to the scale's note)
                spacing = markStep / ratio;
                spacing *= 1.0 + 0.022 * rough * gauss();                         // jitter
                gain *= std::exp (0.22f * rough * gauss());                       // shimmer
                gain *= 1.0f + trem * 0.12f * (float) std::sin (6.283185307179586 * tremPhase);
                // fry: as the level falls below the phrase's, low irregular alternating pulses
                const float lvl = at (levelRing, S);
                // (real fry is where the voice trails off: from 6 dB under the phrase's level, fully 10 dB further down -
                //  never through the words themselves)
                const float fw = noFry ? 0.0f : fry * std::clamp ((speechDb - lvl - 6.0f) / 10.0f, 0.0f, 1.0f);
                if (fryDebug && fw > 0.0f) ++fryGrains;
                if (fryDebug) ++allGrains;
                if (fw > 0.0f)
                {
                    fryOdd = ! fryOdd;
                    spacing *= fryOdd ? 1.0 + 0.5 * fw : 1.0 + 0.1 * fw;
                    spacing *= 1.0 + 0.2 * fw * (uni() - 0.5);
                    if (! fryOdd) gain *= 1.0f - 0.45f * fw;
                }
                centre = m.pos;
                // GRAIN INTERPOLATION: the grain made from the two input pulses either side of it, weighted by where it
                // falls between them - so pulse-to-pulse differences glide instead of repeating in the pattern the pulses
                // are reused in (that pattern sits at the OLD pitch: a ghost of the old voice under the new one). Where the
                // later pulse has not arrived yet, the nearer one alone; on a mark (nothing shifted) it is that pulse, exactly.
                if (markRead + 1 < markHead)
                {
                    const Mark a2 = marks[(size_t) (markRead % numMarks)], b2 = marks[(size_t) ((markRead + 1) % numMarks)];
                    const long long readHalf = (long long) ((float) std::min ((float) cfg.maxHalf, markStep / alpha * (1.0f + 0.25f * smoothNow)) * alpha) + 4;
                    if (a2.voiced && b2.voiced && b2.pos > a2.pos && a2.pos <= S2 && S2 <= b2.pos && b2.pos + readHalf < t)
                    {
                        centre = a2.pos; centreB = b2.pos;
                        blendB = (float) (S2 - a2.pos) / (float) (b2.pos - a2.pos);
                    }
                }
                hw = (int) std::min ((float) cfg.maxHalf, markStep / alpha * (1.0f + 0.25f * smoothNow));   // (two periods of the input, whatever the tract; SMOOTH: a little longer, softer)
            }
            else
            {
                spacing = sr * 0.0035 * (0.8 + 0.4 * uni());
                centre = S;
                hw = (int) std::lround (spacing * 1.3);
            }
            hw = std::max (8, std::min ({ hw, cfg.maxHalf, (int) ((float) cfg.maxHalf / std::max (1.0f, alpha)) }));
            if (profile) profGrains += 2 * hw + 1;
            // the grain: Hann, the input read `alpha` times as fast (the tract)
            const long long Sg = S2;
            const float toTable = (float) hannTableSize / (float) hw;
            for (int j = -hw; j <= hw; ++j)
            {
                const float tp = (float) std::abs (j) * toTable; const int ti = (int) tp; const float tf = tp - (float) ti;
                const float w = hannTable[(size_t) ti] + tf * (hannTable[(size_t) ti + 1] - hannTable[(size_t) ti]);
                const float v = blendB > 0.0f ? (1.0f - blendB) * readIn ((double) centre + (double) j * alpha) + blendB * readIn ((double) centreB + (double) j * alpha)
                                              : readIn ((double) centre + (double) j * alpha);
                at (acc, Sg + j) += gain * w * v;
                at (wsum, Sg + j) += w;
                if (voiced) { const float w2 = w * w, w4 = w2 * w2; at (pulse, Sg + j) += w4 * w4; }
            }
            nextS = Sg + std::max<long long> (8, (long long) std::lround (spacing));
            if (grainDebug && voiced) std::printf ("grain S %.4f markStep %.1f period %.1f spacing %.1f hw %d\n", (double) Sg / sr, markStep, m.period, spacing, hw);
            f0Out = voiced ? (float) (sr / std::max (1.0, spacing)) : f0Out;
        }

        /** The noise for breath and whisper: white noise shaped by the output's cepstral envelope (one frame). */
        void noiseFrame (float whisperShare) noexcept
        {
            if (! noiseNeeded) return;
            // envelope of the newest nFft output samples
            for (int i = 0; i < nFft; ++i) fftBuf[(size_t) i] = envRing[(size_t) ((envPos + i) % nFft)] * hann[(size_t) i];
            std::fill (fftBuf.begin() + nFft, fftBuf.end(), 0.0f);
            fft->performRealOnlyForwardTransform (fftBuf.data(), true);
            const int half = nFft / 2;
            for (int k = 0; k <= half; ++k)
            {
                const float re = fftBuf[(size_t) (2 * k)], im = fftBuf[(size_t) (2 * k + 1)];
                cep[(size_t) k] = 0.5f * std::log (re * re + im * im + 1.0e-12f);
            }
            // real cepstrum -> lifter (keep the envelope, drop the pitch) -> back
            for (int k = 0; k <= half; ++k) { fftBuf[(size_t) k] = cep[(size_t) k]; if (k > 0 && k < half) fftBuf[(size_t) (nFft - k)] = cep[(size_t) k]; }
            std::fill (fftBuf.begin() + nFft, fftBuf.end(), 0.0f);
            fft->performRealOnlyForwardTransform (fftBuf.data(), true);
            const int qc = std::max (4, (int) (0.0012 * sr));
            for (int q = 0; q < nFft; ++q)
            {
                const int qq = std::min (q, nFft - q);
                const float lift = qq < qc ? 1.0f : qq < 2 * qc ? 0.5f + 0.5f * std::cos (3.14159265f * (float) (qq - qc) / (float) qc) : 0.0f;
                cep[(size_t) q] = fftBuf[(size_t) (2 * q)] * lift / (float) nFft;
            }
            std::copy_n (cep.data(), nFft, fftBuf.data());
            std::fill (fftBuf.begin() + nFft, fftBuf.end(), 0.0f);
            fft->performRealOnlyForwardTransform (fftBuf.data(), true);
            for (int k = 0; k <= half; ++k) logEnv[(size_t) k] = fftBuf[(size_t) (2 * k)];
            // noise with that envelope, random phases; breath leans to the top (aspiration), whisper is flat
            float energy = 0.0f;
            for (int k = 0; k <= half; ++k)
            {
                const float hz = (float) k * (float) sr / (float) nFft;
                const float tilt = whisperShare + (1.0f - whisperShare) * std::clamp (hz / 2200.0f, 0.12f, 1.3f);
                const float mag = std::exp (std::min (logEnv[(size_t) k], 30.0f)) * tilt * (k == 0 || hz > 0.45f * (float) sr ? 0.0f : 1.0f);
                const float ph = 6.2831853f * uni();
                fftBuf[(size_t) (2 * k)] = mag * std::cos (ph); fftBuf[(size_t) (2 * k + 1)] = mag * std::sin (ph);
                energy += mag * mag;
            }
            for (int k = half + 1; k < nFft; ++k) { fftBuf[(size_t) (2 * k)] = fftBuf[(size_t) (2 * (nFft - k))]; fftBuf[(size_t) (2 * k + 1)] = -fftBuf[(size_t) (2 * (nFft - k) + 1)]; }
            fft->performRealOnlyInverseTransform (fftBuf.data());
            float rms = 0.0f;
            for (int i = 0; i < nFft; ++i) rms += fftBuf[(size_t) i] * fftBuf[(size_t) i];
            rms = std::sqrt (rms / (float) nFft);
            const float norm = rms > 1.0e-12f ? 1.0f / (rms * std::sqrt (1.5f)) : 0.0f;
            for (int i = 0; i < nFft; ++i)
                noiseOla[(size_t) ((olaPos + i) % (2 * nFft))] += fftBuf[(size_t) i] * hann[(size_t) i] * norm;
            juce::ignoreUnused (energy);
        }

        static constexpr int howlN = 2048, howlHop = 256;
        static int nFftHowl (double) noexcept { return howlN; }
        std::vector<float> howlEnv; int howlPos = 0, sinceHowl = 0; float howlSmooth = -120.0f;
        juce::dsp::FFT howlFft { 11 };
        std::vector<float> howlBuf = std::vector<float> ((size_t) (2 * howlN), 0.0f), howlWin = std::vector<float> ((size_t) howlN, 0.0f);
        std::array<float, 256> howlFreqs {}; int howlFreqPos = 0, howlFreqCount = 0;   // (half a second of frames: 94 at 48 kHz, 188 at 96 kHz)
        float howlGoneS = 0.0f;
        /** The howl watch, on the input (the loop is in the room, not in the unit). A howl is ONE sine that stays
            put: 90 % and more of the band's energy within a bin of one frequency (23 Hz bins), that frequency still
            within 4 Hz half a second on (a voice's harmonics move with every period's jitter and its intonation),
            louder than -24 dBFS and growing (or already near full scale), for 0.6 s -> muted, until the tone has
            been gone 0.3 s (or the input quiet a second). */
        void howlFrame() noexcept
        {
            constexpr int N = howlN;
            for (int i = 0; i < N; ++i) howlBuf[(size_t) i] = howlEnv[(size_t) ((howlPos + i) % N)] * howlWin[(size_t) i];
            std::fill (howlBuf.begin() + N, howlBuf.end(), 0.0f);
            howlFft.performFrequencyOnlyForwardTransform (howlBuf.data(), true);
            const int k0 = std::max (2, (int) (150.0 * N / sr)), k1 = std::min (N / 2 - 3, (int) (6000.0 * N / sr));
            float total = 1.0e-12f, peak = 0.0f; int kPeak = k0;
            for (int k = k0; k <= k1; ++k)
            {
                const float p = howlBuf[(size_t) k] * howlBuf[(size_t) k];
                total += p;
                if (p > peak) { peak = p; kPeak = k; }
            }
            const float pm = howlBuf[(size_t) (kPeak - 1)], p0 = howlBuf[(size_t) kPeak], pp = howlBuf[(size_t) (kPeak + 1)];
            const float around = pm * pm + p0 * p0 + pp * pp;
            const float purity = around / total;
            const float den = pm - 2.0f * p0 + pp;
            const float freq = ((float) kPeak + (std::abs (den) > 1.0e-12f ? std::clamp (0.5f * (pm - pp) / den, -0.5f, 0.5f) : 0.0f)) * (float) sr / (float) N;
            const float peakDb = 20.0f * std::log10 (std::sqrt (around) / ((float) N * 0.25f) + 1.0e-9f) + 6.0f;   // (a full-scale sine: about 0 dB)
            const float dt = (float) howlHop / (float) sr;
            howlFreqs[(size_t) howlFreqPos] = purity > 0.9f ? freq : -1.0f; howlFreqPos = (howlFreqPos + 1) % (int) howlFreqs.size();
            howlFreqCount = std::min ((int) howlFreqs.size(), howlFreqCount + 1);
            // (stays put: every pure frame of the last half second within 4 Hz of this one)
            const int span = std::min (howlFreqCount, (int) (0.5 * sr / howlHop));
            bool stays = purity > 0.9f && span >= (int) (0.5 * sr / howlHop);
            for (int k = 1; k < span && stays; ++k)
            {
                const float f = howlFreqs[(size_t) ((howlFreqPos - 1 - k + 2 * (int) howlFreqs.size()) % (int) howlFreqs.size())];
                stays = f > 0.0f && std::abs (f - freq) < 4.0f;
            }
            howlSmooth += 0.25f * (peakDb - howlSmooth);
            const float before = howlPast[(size_t) howlCount]; howlPast[(size_t) howlCount] = howlSmooth; howlCount = (howlCount + 1) % 8;
            const bool sine = stays && peakDb > -24.0f;
            const bool growing = howlSmooth > before + 0.15f || peakDb > -8.0f;
            howlScoreS = sine && growing ? howlScoreS + dt : sine ? howlScoreS : std::max (0.0f, howlScoreS - 2.0f * dt);
            if (howlDebug && howlCount == 0) std::printf ("howl t %.2f peak %.1f purity %.2f freq %.1f stays %d score %.2f\n", (double) t / sr, peakDb, purity, freq, stays ? 1 : 0, howlScoreS);
            if (howlScoreS > 0.6f) howl = true;
            // (muted: back once the tone has been gone 0.3 s)
            if (howl) { howlGoneS = purity < 0.6f || peakDb < -40.0f ? howlGoneS + dt : 0.0f; if (howlGoneS > 0.3f) { howl = false; howlScoreS = 0.0f; howlGoneS = 0.0f; } }
        }

        // --- pitch correction ----------------------------------------------------------------------------
        float tuneAmt = 0.0f, tuneNote = 0.0f, tuneMidiSlow = -1.0f, tuneCorr = 0.0f, tuneDevCents = 0.0f;
        int tuneKey = 0, scaleUsed = 1; bool tuneAuto = false;
        static int scaleMask (int sc) noexcept
        {
            constexpr int masks[numScales] { 0xFFF,
                (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),     // major
                (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),     // natural minor
                (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11),     // harmonic minor
                (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                            // major pentatonic
                (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),                           // minor pentatonic
                (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10) };              // blues
            return masks[std::clamp (sc, 0, (int) numScales - 1)];
        }
        /** The scale's note nearest a pitch (MIDI, fractional). */
        float nearestNote (float midi) const noexcept
        {
            const int mask = scaleMask (scaleUsed);
            float best = std::round (midi), bestD = 1.0e9f;
            for (int d = -7; d <= 7; ++d)
            {
                const int m = (int) std::round (midi) + d;
                if ((mask >> (((m - tuneKey) % 12 + 12) % 12) & 1) == 0) continue;
                if (const float dd = std::abs ((float) m - midi); dd < bestD) { bestD = dd; best = (float) m; }
            }
            return best;
        }
        /** The pitch correction for one period (a ratio): where the voice is going (its pitch, after PITCH and the
            character) to the scale's nearest note. SPEED: how quickly it gets there (0: at once - the hard, stepped
            sound; slower: invisible). HUMANIZE: corrects the slow pitch and lets the fast wiggles - vibrato, a scoop -
            through. AUTO: gentle and slow when it's nearly in tune, firm and quick when it's well off. */
        float tuneRatio (float markStep, const Knobs& k) noexcept
        {
            const float dt = markStep / (float) sr;   // (one period)
            const float f = (float) sr / markStep * ratioNow;
            const float midi = 69.0f + 12.0f * std::log2 (std::max (20.0f, f) / 440.0f);
            if (tuneMidiSlow < 0.0f || std::abs (midi - tuneMidiSlow) > 3.0f) tuneMidiSlow = midi;   // (a new phrase, a leap: start there)
            const float h = std::clamp (k.humanize, 0.0f, 10.0f);
            if (h > 0.0f) tuneMidiSlow += (1.0f - std::exp (-dt / (0.02f + 0.03f * h))) * (midi - tuneMidiSlow); else tuneMidiSlow = midi;
            const float base = tuneMidiSlow;
            // (the note it is on stays until the voice is clearly nearer another: no flicker between two)
            const int mask = scaleMask (scaleUsed);
            const bool inScale = tuneNote > 0.0f && ((mask >> ((((int) tuneNote - tuneKey) % 12 + 12) % 12)) & 1) != 0;
            if (! inScale || std::abs (base - tuneNote) > 0.65f) tuneNote = nearestNote (base);
            const float dev = tuneNote - base;
            tuneDevCents = -100.0f * dev;
            float amount = tuneAmt, speedMs = std::max (k.speedMs, 12.0f * smoothNow);   // (SMOOTH: never quite a click)
            if (tuneAuto)
            {
                const float far = std::clamp ((std::abs (dev) - 0.1f) / 0.3f, 0.0f, 1.0f);   // (full strength from 0.4 st off)
                amount *= 0.6f + 0.4f * far;
                speedMs = 150.0f + (20.0f - 150.0f) * far;
            }
            const float want = dev * amount;
            tuneCorr += (speedMs < 0.5f ? 1.0f : 1.0f - std::exp (-dt * 1000.0f / speedMs)) * (want - tuneCorr);
            return std::exp2 (tuneCorr / 12.0f);
        }

        // --- AUTO key: a chroma of the whole input (every 0.1 s, a 15 s memory) against the major and minor key profiles
        int keyN = 4096;   // (sized to the rate in prepare: about 12 Hz a bin at any rate)
        std::unique_ptr<juce::dsp::FFT> keyFft;
        std::vector<float> keyRing, keyBuf;
        std::array<float, 12> chroma {};
        float chromaWeight = 0.0f;
        int keyFound = -1, keyCandidate = -1, keyVotes = 0, keyPos = 0, sinceKey = 0, sinceKeyEval = 0;
        void keyFrame() noexcept
        {
            for (int i = 0; i < keyN; ++i)
                keyBuf[(size_t) i] = keyRing[(size_t) ((keyPos + i) % keyN)] * (0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) keyN));
            std::fill (keyBuf.begin() + keyN, keyBuf.end(), 0.0f);
            keyFft->performFrequencyOnlyForwardTransform (keyBuf.data(), true);
            std::array<float, 12> frame {}; float energy = 0.0f;
            for (int b = std::max (1, (int) (110.0 * keyN / sr)); b < (int) (2500.0 * keyN / sr) && b < keyN / 2; ++b)
            {
                const float hz = (float) b * (float) sr / (float) keyN;
                const int pc = (int) std::lround (12.0f * std::log2 (hz / 440.0f) + 69.0f) % 12;
                const float m = std::sqrt (howlSafe (keyBuf[(size_t) b]));
                frame[(size_t) ((pc + 12) % 12)] += m; energy += m;
            }
            const float decay = std::exp (-0.1f / 15.0f);
            if (energy > 1.0e-3f)
            {
                for (int i = 0; i < 12; ++i) chroma[(size_t) i] = chroma[(size_t) i] * decay + frame[(size_t) i] / energy;
                chromaWeight = chromaWeight * decay + 1.0f;
            }
            if (++sinceKeyEval < 5 || chromaWeight < 25.0f) return;   // (every half second; after ~3 s of sound)
            sinceKeyEval = 0;
            static constexpr float majP[12] { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
            static constexpr float minP[12] { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
            auto corr = [&] (const float* prof, int tonic) {
                float mx = 0, my = 0; for (int i = 0; i < 12; ++i) { mx += chroma[(size_t) ((i + tonic) % 12)]; my += prof[i]; }
                mx /= 12; my /= 12; float sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < 12; ++i) { const float a = chroma[(size_t) ((i + tonic) % 12)] - mx, b = prof[i] - my; sxy += a * b; sxx += a * a; syy += b * b; }
                return sxy / std::sqrt (sxx * syy + 1.0e-12f); };
            int best = 0; float bestC = -2.0f, currentC = -2.0f;
            for (int kk = 0; kk < 24; ++kk)
            {
                const float c = corr (kk < 12 ? majP : minP, kk % 12);
                if (c > bestC) { bestC = c; best = kk; }
                if (kk == keyFound) currentC = c;
            }
            // (a new key must win three evaluations in a row, and clearly)
            if (keyFound < 0) { if (best == keyCandidate) { if (++keyVotes >= 3 && bestC > 0.5f) keyFound = best; } else { keyCandidate = best; keyVotes = 1; } }
            else if (best != keyFound && bestC > currentC + 0.03f) { if (best == keyCandidate) { if (++keyVotes >= 3) keyFound = best; } else { keyCandidate = best; keyVotes = 1; } }
            else keyVotes = 0;
        }
        static float howlSafe (float v) noexcept { return std::isfinite (v) ? v : 0.0f; }

        // --- learning the speaker: their usual pitch (a decaying histogram of voiced frames, 25-cent bins from 50 Hz) and
        //     their vocal tract (LPC on an ~11 kHz copy every 40 ms: each formant's tube length (2n-1) c / 4 Fn, the median
        //     of F1-F3, in a decaying 0.25 cm histogram) - medians, so one odd vowel or a slip moves nothing
        static constexpr int pitchBins = 176, tractBins = 49;   // (50 Hz .. ~630 Hz; 10 .. 22 cm)
        std::array<float, pitchBins> pitchHist {}; std::array<float, tractBins> tractHist {};
        float pitchHistW = 0.0f, tractHistW = 0.0f, spkPitchHz = 0.0f, spkTractCm = 0.0f, spkConf = 0.0f;
        int dec2 = 4, dec2Count = 0, dec2Pos = 0, sinceLpc = 0; float dec2Acc = 0.0f;
        static constexpr int dec2Size = 512;
        std::array<float, dec2Size> dec2Ring {};
        sim::Svf lpcAa;
        std::array<double, 400> lpcFrame {};
        void learnPitch (float hz) noexcept
        {
            const int b = (int) std::lround (4.0f * 12.0f * std::log2 (std::max (50.0f, hz) / 50.0f));
            if (b < 0 || b >= pitchBins) return;
            pitchHist[(size_t) b] += 1.0f; pitchHistW += 1.0f;
        }
        static float histMedian (const float* h, int n, float total) noexcept
        {
            float acc = 0.0f;
            for (int i = 0; i < n; ++i) { acc += h[i]; if (acc >= 0.5f * total) return (float) i; }
            return (float) (n - 1);
        }
        void lpcLearn() noexcept
        {
            const double fs = sr / dec2;
            const int w = std::min ((int) lpcFrame.size(), (int) (0.03 * fs));
            for (int i = 0; i < w; ++i)
            {
                const double x0 = dec2Ring[(size_t) ((dec2Pos - w + i + 2 * dec2Size) % dec2Size)];
                const double x1 = dec2Ring[(size_t) ((dec2Pos - w + i - 1 + 2 * dec2Size) % dec2Size)];
                lpcFrame[(size_t) i] = (x0 - 0.94 * x1) * (0.54 - 0.46 * std::cos (6.283185307179586 * i / (w - 1)));
            }
            constexpr int order = 12;
            std::array<double, order + 1> r {}, a {}, tmp {};
            for (int l = 0; l <= order; ++l) for (int i = l; i < w; ++i) r[(size_t) l] += lpcFrame[(size_t) i] * lpcFrame[(size_t) (i - l)];
            if (r[0] < 1.0e-9) return;
            r[0] *= 1.0001; a[0] = 1.0; double err = r[0];
            for (int i = 1; i <= order; ++i)
            {
                double acc = r[(size_t) i]; for (int j = 1; j < i; ++j) acc += a[(size_t) j] * r[(size_t) (i - j)];
                const double k = -acc / err; tmp = a;
                for (int j = 1; j < i; ++j) a[(size_t) j] = tmp[(size_t) j] + k * tmp[(size_t) (i - j)];
                a[(size_t) i] = k; err *= (1.0 - k * k);
                if (err <= 0.0) return;
            }
            // the envelope's first three peaks, 200 Hz .. 4 kHz (25 Hz steps)
            std::array<double, 3> fm {}; int nf = 0; double prev2 = 0, prev1 = 0;
            for (double f = 200.0; f < std::min (4000.0, 0.45 * fs) && nf < 3; f += 25.0)
            {
                double re = 0, im = 0;
                for (int j = 0; j <= order; ++j) { re += a[(size_t) j] * std::cos (6.283185307179586 * f * j / fs); im -= a[(size_t) j] * std::sin (6.283185307179586 * f * j / fs); }
                const double e = -std::log (re * re + im * im + 1e-18);
                if (f > 250.0 && prev1 > prev2 && prev1 >= e) fm[(size_t) nf++] = f - 25.0;
                prev2 = prev1; prev1 = e;
            }
            if (nf < 3) return;
            std::array<double, 3> L {};
            for (int n2 = 0; n2 < 3; ++n2) L[(size_t) n2] = (2.0 * n2 + 1.0) * 35000.0 / (4.0 * fm[(size_t) n2]);
            std::sort (L.begin(), L.end());
            const int b = (int) std::lround ((L[1] - 10.0) * 4.0);
            if (b < 0 || b >= tractBins) return;
            tractHist[(size_t) b] += 1.0f; tractHistW += 1.0f;
        }
        /** The speaker, as learnt so far (once a block). */
        void updateSpeaker() noexcept
        {
            if (pitchHistW > 1.0f) spkPitchHz = 50.0f * std::exp2 (histMedian (pitchHist.data(), pitchBins, pitchHistW) / 48.0f);
            if (tractHistW > 1.0f) spkTractCm = 10.0f + 0.25f * histMedian (tractHist.data(), tractBins, tractHistW);
            // (sure after ~1.5 s of voice for the pitch, ~30 good formant frames for the tract)
            spkConf = std::min (std::clamp (pitchHistW / 500.0f, 0.0f, 1.0f), std::clamp (tractHistW / 30.0f, 0.0f, 1.0f));
        }

        /** F1 and F2 of an envelope (32 log-spaced bands): the two biggest peaks between 250 Hz and 3 kHz. */
        static void formantsOf (const std::array<float, 32>& e, std::array<float, 2>& f) noexcept
        {
            auto hzOf = [] (int b) { return 100.0f * std::pow (80.0f, (float) b / 31.0f); };
            int a = -1, b = -1;
            for (int i = 1; i < 31; ++i)
                if (hzOf (i) > 250.0f && hzOf (i) < 3200.0f && e[(size_t) i] >= e[(size_t) (i - 1)] && e[(size_t) i] >= e[(size_t) (i + 1)])
                {
                    if (a < 0) a = i; else if (b < 0) { b = i; break; }
                }
            f[0] = a >= 0 ? hzOf (a) : 0.0f; f[1] = b >= 0 ? hzOf (b) : 0.0f;
        }
        /** A 32-band picture of a ring's newest frame (for the screen). */
        void bandsOf (const std::vector<float>& ring, int pos, std::array<float, 32>& out) noexcept
        {
            for (int i = 0; i < nFft; ++i) fftBuf[(size_t) i] = ring[(size_t) ((pos + i) % nFft)] * hann[(size_t) i];
            std::fill (fftBuf.begin() + nFft, fftBuf.end(), 0.0f);
            fft->performFrequencyOnlyForwardTransform (fftBuf.data(), true);
            for (int b = 0; b < 32; ++b)
            {
                const float lo = 100.0f * std::pow (80.0f, ((float) b - 0.5f) / 31.0f), hi = 100.0f * std::pow (80.0f, ((float) b + 0.5f) / 31.0f);
                const int k0 = std::max (1, (int) (lo * (float) nFft / (float) sr)), k1 = std::max (k0, std::min (nFft / 2, (int) (hi * (float) nFft / (float) sr)));
                float s = 0.0f; for (int k = k0; k <= k1; ++k) s += fftBuf[(size_t) k] * fftBuf[(size_t) k];
                const float db = 10.0f * std::log10 (s / (float) (k1 - k0 + 1) + 1.0e-12f) - 20.0f * std::log10 ((float) nFft * 0.25f);
                out[(size_t) b] += 0.3f * (std::clamp ((db + 72.0f) / 72.0f, 0.0f, 1.0f) - out[(size_t) b]);
            }
        }

        void render (float* const* io, int n, const float* q) noexcept override
        {
            juce::ScopedNoDenormals noDenormals;   // (its filters' tails over silence: never the slow denormal path)
            const Knobs k = knobsFrom (q);
            smoothNow = std::clamp (k.smooth, 0.0f, 10.0f) / 10.0f;
            const double pAll = profile ? nowS() : 0.0;
            if (profile) profSamples += n;
            const int wantMode = k.mode > 0.5f ? 1 : 0;
            if (wantMode != mode) configure (wantMode);   // (the host is told the new latency; a short restart)
            const int ch = (int) std::clamp (std::lround (k.character), 0L, (long) numCharacters - 1);
            const Shape& sh = shapes[(size_t) ch];
            const float strength = std::clamp (k.strength * 0.01f, 0.0f, 2.0f);
            // where it goes: pitch (st) and tract (octaves); the knobs add to the character
            const float age = k.age, gender = k.gender;
            // (STRENGTH: how far the whole move goes - the character and every knob; 0 is the voice as it came)
            // (a character with a target: the move that takes THIS speaker there, once it has learnt them - before then
            //  its usual move, blended over as it gets sure)
            updateSpeaker();
            float charSt = sh.pitchSt, charOct = sh.tractOct;
            if (sh.targetHz > 0.0f && spkPitchHz > 0.0f)
            {
                const float w = std::clamp (pitchHistW / 500.0f, 0.0f, 1.0f);
                charSt += w * (std::clamp (12.0f * std::log2 (sh.targetHz / spkPitchHz), -15.0f, 15.0f) - charSt);
            }
            if (sh.targetCm > 0.0f && spkTractCm > 0.0f)
            {
                const float w = std::clamp (tractHistW / 30.0f, 0.0f, 1.0f);
                charOct += w * (std::clamp (std::log2 (spkTractCm / sh.targetCm), -0.6f, 0.54f) - charOct);
            }
            intonation = sh.intonation;
            float pitchSt = charSt + k.pitch + gender * 0.9f + (age < 0.0f ? -age * 1.1f : -age * 0.1f);
            float tractOct = charOct + k.formant * 0.05f + gender * 0.0227f + (age < 0.0f ? -age * 0.04f : -age * 0.004f);
            pitchSt = std::clamp (pitchSt * strength, -24.0f, 24.0f);
            if (k.autoTune) tractOct += 0.15f * pitchSt / 12.0f;   // (AUTO voice match: formants follow a big move a little, as a voice does)
            tractOct = std::clamp (tractOct * strength, -0.6f, 0.54f);
            // the pitch correction: how much, to which notes
            tuneAmt = std::clamp (k.tune, 0.0f, 1.0f) * std::min (1.0f, strength);
            tuneAuto = k.autoTune;
            if (k.autoTune) { tuneKey = keyFound >= 0 ? keyFound % 12 : 0; scaleUsed = keyFound < 0 ? chromatic : keyFound < 12 ? major : minor; }
            else { tuneKey = (int) std::clamp (std::lround (k.key), 0L, 11L); scaleUsed = (int) std::clamp (std::lround (k.scale), 0L, (long) numScales - 1); }
            const float ratioWant = std::exp2 (pitchSt / 12.0f), alphaWant = std::exp2 (tractOct);
            // VOICE QUALITY - measured on real voices (AudioLab voicefit: real women vs M TO F on real men, and the
            // reverse): a voice moved toward a woman's or a child's has less presence (1-4 kHz), more air (6-8 kHz) and
            // nothing under its new pitch; toward a man's, more presence, a little air, a little warmth. As far as the
            // tract moved (a full man <-> woman move: 1).
            {
                const float toward = std::clamp (tractOct / 0.23f, -1.5f, 1.5f);
                const float f = std::max (0.0f, toward), m = std::max (0.0f, -toward);
                vqPresenceT = std::pow (10.0f, (-4.0f * f + 6.0f * m) / 20.0f);
                vqAirT = std::pow (10.0f, (7.0f * f + 6.0f * m) / 20.0f);
                vqWarmT = std::pow (10.0f, (2.0f * m) / 20.0f);
                vqLowCutT = f;   // (0..1.5: how much under the new pitch goes)
                const float newHz = std::max (60.0f, (spkPitchHz > 0.0f ? spkPitchHz : 120.0f) * ratioWant);
                vqHp.set (sr, std::clamp (0.6f * newHz, 50.0f, 220.0f), 0.7071);
            }
            const float breath = std::clamp ((sh.breath + k.breath + std::max (0.0f, age) * 0.4f) * strength * std::clamp (k.multiply, 0.25f, 3.0f), 0.0f, 10.0f) / 10.0f;
            const float whisperAmt = std::clamp (sh.whisper * strength, 0.0f, 1.0f);
            const float gateDb = k.gate;
            const float deess = std::min (1.0f, strength) * std::max (k.deess, alphaWant > 1.05f ? 3.0f : 0.0f) / 10.0f;
            const float mix = std::clamp (k.mix * 0.01f, 0.0f, 1.0f);
            outGain = std::pow (10.0f, std::clamp (k.output, -12.0f, 12.0f) / 20.0f);
            noiseNeeded = breath > 0.0f || whisperAmt > 0.0f;
            // (nothing asked of it - STRENGTH 0, or every knob at rest on CUSTOM: the voice passes exactly, only later)
            const float tx = strength * std::clamp (k.multiply, 0.25f, 3.0f);
            const bool moving = std::abs (pitchSt) > 1.0e-3f || std::abs (tractOct) > 1.0e-4f || noiseNeeded || deess > 0.0f || tuneAmt > 0.0f
                             || (sh.rough + k.rough + sh.vibrato + k.vibrato + sh.tremor + std::max (0.0f, k.age) + sh.fry + k.fry) * tx > 0.0f
                             || std::abs (outGain - 1.0f) > 1.0e-6f;
            const float whisperShare = noiseNeeded ? whisperAmt / std::max (1.0e-3f, whisperAmt + breath) : 0.0f;
            const float kFast = 1.0f - std::exp (-1.0f / (0.001f * (float) sr)), kSlow = 1.0f - std::exp (-1.0f / (0.020f * (float) sr));
            const float kLvl = 1.0f - std::exp (-1.0f / (0.015f * (float) sr));
            const float kPow = 1.0f - std::exp (-1.0f / (0.40f * (float) sr)), kMatch = 1.0f - std::exp (-1.0f / (0.15f * (float) sr));
            // (SMOOTH: the moves glide 50 .. 300 ms; the change in and out fades over 15 .. 50 ms)
            const float kVad = 1.0f / ((0.015f + 0.035f * smoothNow) * (float) sr), kGlide = 1.0f - std::exp (-1.0f / ((0.05f + 0.25f * smoothNow) * (float) sr));
            const long long hold = (long long) (0.15 * sr);

            for (int i = 0; i < n; ++i)
            {
                // ---- in ----
                const float l = io[0][i], r = io[1][i], x = 0.5f * (l + r);
                // (only the middle is processed - the side, a track's stereo, passes as it came; and of the middle only
                //  what is over 50 Hz (4 poles: a voice's lowest pitch is far above) - a track's kick and sub pass untouched;
                //  lows + rest add back to it exactly)
                float lowBand, lowHp, low1; lowSplit.process (x, low1, lowHp); lowSplit2.process (low1, lowBand, lowHp);
                at (inL, t) = l; at (inR, t) = r; at (inMid, t) = x - lowBand; at (inLow, t) = lowBand;
                if (tuneAuto && tuneAmt > 0.0f)   // (AUTO: the key, from the whole input - music and voice)
                {
                    keyRing[(size_t) keyPos] = x; keyPos = (keyPos + 1) % keyN;
                    if (++sinceKey >= (int) (0.1 * sr)) { sinceKey = 0; keyFrame(); }
                }
                float lp, hp; markLp.process (x, lp, hp); at (lpRing, t) = lp;
                levelFastPow += kLvl * (x * x - levelFastPow);
                levelDb = 10.0f * std::log10 (levelFastPow + 1.0e-12f) + 3.0f;
                at (levelRing, t) = levelDb;
                const float a = std::abs (x);
                envFast += (a > envFast ? kFast : kFast * 0.1f) * (a - envFast);
                envSlow += kSlow * (a - envSlow);
                if (envFast > 3.0f * envSlow + 1.0e-4f && levelDb > gateDb) lastTransient = t;
                // an ~11 kHz copy for learning the speaker's vocal tract (LPC every 40 ms of confident voice)
                {
                    float al, ah; lpcAa.process (x, al, ah);
                    if (++dec2Count >= dec2) { dec2Count = 0; dec2Ring[(size_t) dec2Pos] = al; dec2Pos = (dec2Pos + 1) % dec2Size; }
                    if (++sinceLpc >= (int) (0.04 * sr)) { sinceLpc = 0; if (voicedNow && voiceLike && levelDb > speechDb - 15.0f) lpcLearn(); }
                }
                // decimated copy for YIN
                float dl, dh; pitchLp.process (x, dl, dh);
                float d2l, d2h; decLp.process (dl, d2l, d2h);
                if (++decCount >= dec) { decCount = 0; decRing[(size_t) decPos] = d2l; decPos = (decPos + 1) % decSize; }
                juce::ignoreUnused (hp, dh, d2h);
                if (++sinceHop >= cfg.hop)
                {
                    sinceHop = 0;
                    float period = 0.0f, ap = 1.0f;
                    { const double p0 = profile ? nowS() : 0.0; yin (period, ap); if (profile) profYin += nowS() - p0; }
                    lastAp = ap;
                    // (real speech often correlates only 0.6 - 0.75 inside a vowel: voiced from 0.65, staying voiced down to 0.5)
                    const bool voiced = period > 0.0f && ap < (lastFrameVoiced ? 0.5f : 0.35f) && levelDb > gateDb;
                    // the wobble: the median of the last 16 frame-to-frame pitch changes (a drum hit or an octave
                    // slip moves a few frames; a voice moves nearly all of them)
                    if (voiced && lastFrameVoiced && prevPeriod > 0.0f)
                    {
                        const float cents = std::abs (1200.0f * std::log2 (period / prevPeriod));
                        if (cents < 60.0f) { wobble[(size_t) (wobblePos++ % 16)] = cents; wobbleCount = std::min (16, wobbleCount + 1); }
                        if (wobbleCount >= 5)
                        {
                            std::array<float, 16> w2 = wobble;
                            std::nth_element (w2.begin(), w2.begin() + wobbleCount / 2, w2.begin() + wobbleCount);
                            pitchDev = w2[(size_t) (wobbleCount / 2)];
                        }
                    }
                    else if (voiced && unvoicedRun > 10) { pitchDev = 0.0f; wobbleCount = 0; wobblePos = 0; }   // (a real pause - not a weak frame - starts it over)
                    voicedRun = voiced ? voicedRun + 1 : 0;
                    unvoicedRun = voiced ? 0 : unvoicedRun + 1;
                    prevPeriod = voiced ? period : 0.0f;
                    lastFrameVoiced = voiced;
                    // voice evidence: fills while voiced frames wobble like a voice (half full in 50 ms), drains
                    // while voiced frames hold dead still (an instrument, a hum), slowly over silence (30 s) - so a
                    // drum hit's 40 ms of wobble never counts, and a talker's pauses don't start it over
                    {
                        const float dt = (float) cfg.hop / (float) sr;
                        // (and it moves: over the last quarter second a voice's pitch spans 20 cents and more - intonation,
                        //  jitter - where a held note, even with a drum beating against it, stays within a few)
                        // (the 15th to 85th percentile of the last quarter second's periods: a drum's few stray
                        //  readings across a held note don't make it move)
                        int nSpan = 0;
                        for (int k = 0, n2 = std::min (frameCount, numFrames); k < n2 && nSpan < (int) spanBuf.size(); ++k)
                        {
                            const auto& f = frames[(size_t) ((frameCount - 1 - k) % numFrames)];
                            if (t - f.centre > (long long) (0.25 * sr) + (cfg.yinWin + (long long) (sr / 60.0)) / 2) break;
                            if (f.voiced && period > 0.0f && std::abs (std::log2 (f.period / period)) < 0.6f)   // (an octave slip is not intonation)
                                spanBuf[(size_t) nSpan++] = f.period;
                        }
                        float spanCents = 0.0f;
                        if (nSpan >= 5)
                        {
                            std::sort (spanBuf.begin(), spanBuf.begin() + nSpan);
                            const float lo = spanBuf[(size_t) (nSpan * 15 / 100)], hi = spanBuf[(size_t) std::min (nSpan - 1, nSpan * 85 / 100)];
                            spanCents = hi > lo ? 1200.0f * std::log2 (hi / lo) : 0.0f;
                        }
                        const bool wobbling = voiced && wobbleCount >= 5 && pitchDev > 0.8f && spanCents >= 20.0f;
                        // (truly still - an instrument's note, a hum: drains, emptying in 0.6 s; monotone speech, in between: held)
                        const bool still = voiced && wobbleCount >= 5 && pitchDev < 0.6f && spanCents < 15.0f;
                        voiceEvidence += wobbling ? dt / 0.10f : still ? -dt / 0.6f : voiced ? 0.0f : -dt / 30.0f;
                        voiceEvidence = std::clamp (voiceEvidence, 0.0f, 1.0f);
                        voiceLike = voiceEvidence >= 0.5f;
                    }
                    if (vadDebug) std::printf ("vad t %.3f period %.1f ap %.3f lvl %.1f voiced %d run %d dev %.2f evidence %.2f\n", (double) t / sr, period, ap, levelDb, voiced ? 1 : 0, voicedRun, pitchDev, voiceEvidence);
                    frames[(size_t) (frameCount % numFrames)] = { t - (cfg.yinWin + (long long) (sr / 60.0)) / 2, period, ap, voiced };
                    ++frameCount;
                    {   // (the speaker, slowly forgotten - a new speaker takes over in a few seconds)
                        const float d = std::exp (-(float) cfg.hop / (6.0f * (float) sr));
                        for (auto& v2 : pitchHist) v2 *= d;
                        for (auto& v2 : tractHist) v2 *= d;
                        pitchHistW *= d; tractHistW *= d;
                    }
                    if (voiced)
                    {
                        f0In = (float) sr / period; speechDb += 0.02f * (levelDb - speechDb);
                        if (voiceLike) learnPitch (f0In);
                        // the marks are read off the fundamental alone (a low-pass just over the pitch): one clean peak a
                        // period, never a formant's ringing - so they sit on the same point of every pulse
                        markLp.set (sr, std::clamp (1.6 * (double) f0In, 90.0, 900.0), 0.6);
                    }
                    voicedNow = voiced;
                }
                lastVoicedRing[(size_t) (t & ringMask)] = voiceLike ? t : (t > 0 ? lastVoicedRing[(size_t) ((t - 1) & ringMask)] : -1000000);
                ++t;
                { const double p0 = profile ? nowS() : 0.0; placeMarks(); if (profile) profMarks += nowS() - p0; }
                // the targets glide (a character change never jumps)
                ratioNow += kGlide * (ratioWant - ratioNow);
                alphaNow += kGlide * (alphaWant - alphaNow);
                tremPhase += tremRate / sr; if (tremPhase > 1.0) { tremPhase -= 1.0; tremRate = 4.5f + 2.0f * uni(); }
                // ---- grains ----
                if (nextS <= t - cfg.rMax)
                {
                    const double p0 = profile ? nowS() : 0.0;
                    for (int g = 0; g < 16 && nextS <= t - cfg.rMax; ++g)
                        renderGrain (k, sh, strength);
                    if (profile) profGrain += nowS() - p0;
                }

                // ---- out: the sample `latency` ago ----
                const long long o = t - 1 - latency;
                float y = 0.0f, yPre = 0.0f;
                if (o >= 0)
                {
                    const float ws = at (wsum, o);
                    y = at (acc, o) / std::max (ws, 0.5f);
                    const float pl = std::min (1.0f, at (pulse, o));
                    at (acc, o) = 0.0f; at (wsum, o) = 0.0f; at (pulse, o) = 0.0f;
                    // breath and whisper
                    envRing[(size_t) envPos] = y; envPos = (envPos + 1) % nFft;
                    if (++sinceNoise >= nHop) { sinceNoise = 0; olaPos = (olaPos + nHop) % (2 * nFft); const double p0 = profile ? nowS() : 0.0; noiseFrame (whisperShare); if (profile) profNoise += nowS() - p0; }
                    // (the first hop of the newest frame: no later frame adds to it)
                    float& slot = noiseOla[(size_t) ((olaPos + sinceNoise) % (2 * nFft))];
                    const float nz = noiseNeeded ? slot : 0.0f;
                    slot = 0.0f;
                    wetRmsFast += 0.004f * (y * y - wetRmsFast);
                    const float env = std::sqrt (wetRmsFast);
                    if (noiseNeeded)
                    {
                        float bl, bh; breathHp.process (y, bl, bh);
                        // (as much as the sound is a voice: a clean voice fully; a voice in a dense track much less - the
                        //  noise is made from everything in the middle, and would turn the music to hiss)
                        voicing += 0.002f * ((voicedNow ? std::clamp (1.0f - (lastAp - 0.1f) / 0.3f, 0.0f, 1.0f) : 0.0f) - voicing);
                        const float wA = whisperAmt * voicing, bA = breath * voicing;
                        const float voiceW = 1.0f - wA;
                        const float breathy = (y - 0.35f * bA * bh) * (1.0f - 0.2f * bA) + nz * env * 0.75f * bA * (0.45f + 0.55f * pl);
                        y = breathy * voiceW + nz * env * 1.15f * wA;
                    }
                    {   // the voice quality (only where it is changing the voice: vadGain); its gains glide (~30 ms: no zipper)
                        const float kq = 1.0f - std::exp (-1.0f / (0.03f * (float) sr));
                        vqPresence += kq * (vqPresenceT - vqPresence); vqAir += kq * (vqAirT - vqAir);
                        vqWarm += kq * (vqWarmT - vqWarm); vqLowCut += kq * (vqLowCutT - vqLowCut);
                        float pl2, ph2, al2, ah2, wl, wh, hl2, hh2, hl3, hh3;
                        const float band = vqPresenceSvf.process (y, pl2, ph2);
                        vqAirSvf.process (y, al2, ah2);
                        const float warmBand = vqWarmSvf.process (y, wl, wh);
                        vqHp.process (y, hl2, hh2);   // (under the new pitch: less - as a real woman's)
                        const float under = hl2; juce::ignoreUnused (hl3, hh3);
                        y += (vqPresence - 1.0f) * band + (vqAir - 1.0f) * ah2 + (vqWarm - 1.0f) * warmBand - std::min (1.0f, vqLowCut) * under;
                    }
                    yPre = y;   // (the level is matched on the voice as changed - what the de-esser and SMOOTH take off stays off)
                    // de-ess: the top over 4.5 kHz, down while it towers over the rest
                    {
                        float sl, sh2; deessSplit.process (y, sl, sh2);
                        const float top = y - sl;
                        deessEnvHp += (std::abs (top) > deessEnvHp ? 0.05f : 0.0008f) * (std::abs (top) - deessEnvHp);
                        deessEnvAll += (std::abs (y) > deessEnvAll ? 0.05f : 0.0008f) * (std::abs (y) - deessEnvAll);
                        const float ratio = deessEnvHp / (deessEnvAll + 1.0e-6f);
                        const float want = deess > 0.0f && ratio > 0.42f ? std::max (0.25f, std::pow (0.42f / ratio, 1.5f * deess)) : 1.0f;
                        deessGain += (want < deessGain ? 0.02f : 0.0015f) * (want - deessGain);
                        deessDb = 20.0f * std::log10 (std::max (1.0e-3f, deessGain));
                        y = sl + top * deessGain;
                    }
                }
                // the dry (as it came, `latency` ago) and whether a voice is there
                const float dL = o >= 0 ? at (inL, o) : 0.0f, dR = o >= 0 ? at (inR, o) : 0.0f, dM = 0.5f * (dL + dR);
                // (voiced anywhere from `hold` before it to now - the latency is its look-ahead)
                const long long lastVoiced = lastVoicedRing[(size_t) ((t - 1) & ringMask)];
                const bool activeNear = o >= 0 && lastVoiced >= 0 && lastVoiced >= o - hold && moving;
                vadGain = std::clamp (vadGain + (activeNear ? kVad : -kVad), 0.0f, 1.0f);
                // level: the voice as loud as it came
                // SMOOTH: highs the change adds over the original's own brightness (over 4 kHz, tracked over ~100 ms) taken
                // back - never more than 6 dB, and never below the original's
                if (smoothNow > 0.0f && o >= 0)
                {
                    float hl, hh, ol, oh; hfIn.process (dM, hl, hh); hfOut.process (y, ol, oh);
                    hfInEnv += 0.0004f * (hh * hh - hfInEnv); hfOutEnv += 0.0004f * (oh * oh - hfOutEnv);
                    const float want = std::clamp (std::sqrt ((hfInEnv + 1.0e-12f) / (hfOutEnv + 1.0e-12f)), 0.5f, 1.0f);
                    hfGain += 0.002f * (1.0f + smoothNow * (want - 1.0f) - hfGain);
                    y -= (1.0f - hfGain) * oh;
                }
                // (the middle's lows, as they came - except when the voice moved toward a woman's or a child's: then the old
                //  voice's lowest rumble, the ghost of its fundamental, goes with the low cut)
                const float yMid = y + (o >= 0 ? at (inLow, o) : 0.0f) * (1.0f - vadGain * std::min (1.0f, vqLowCut));
                if (activeNear) { const float wm = yPre + (o >= 0 ? at (inLow, o) : 0.0f) * (1.0f - std::min (1.0f, vqLowCut)); dryPow += kPow * (dM * dM - dryPow); wetPow += kPow * (wm * wm - wetPow); }
                const float matchWant = std::clamp (std::sqrt ((dryPow + 1.0e-9f) / (wetPow + 1.0e-9f)), 0.25f, 4.0f);
                matchGain += kMatch * (matchWant - matchGain);
                const float v = vadGain * mix;
                // left and right: the dry, with only its middle moved to the processed one - the side stays
                const float change = v * (yMid * matchGain * outGain - dM);
                float outL = dL + change, outR = dR + change;
                // its peaks: no more than the input's +3 dB (held 50 ms)
                // (the input's peak as it arrives - `latency` ahead of this sample - held for that and 50 ms more: the cap is
                //  already up when a burst comes, even one the tract change has moved a little earlier)
                const float dp = std::max (std::abs (dL), std::abs (dR));
                const float ahead = std::max (std::abs (l), std::abs (r));
                // (a true running maximum over the latency and 50 ms more - kept as 64-sample block maxima - so the cap is
                //  never under a sample the dry path is still to play)
                peakBlockNow = std::max (peakBlockNow, ahead);
                if (++peakBlockFill >= 64)
                {
                    peakBlocks[(size_t) peakBlockPos] = peakBlockNow; peakBlockPos = (peakBlockPos + 1) % (int) peakBlocks.size();
                    peakBlockNow = 0.0f; peakBlockFill = 0;
                    const int span = std::min ((int) peakBlocks.size() - 1, (latency + (int) (0.05 * sr)) / 64 + 2);
                    float mx = 0.0f;
                    for (int b2 = 1; b2 <= span; ++b2) mx = std::max (mx, peakBlocks[(size_t) ((peakBlockPos - b2 + (int) peakBlocks.size()) % (int) peakBlocks.size())]);
                    peakBlocksMax = mx;
                }
                peakHold = std::max (peakBlocksMax, peakBlockNow);
                // (and never over full scale where the input wasn't: a hot track peaking near 0 dBFS gets no headroom
                //  to clip into - the rack's limiter would squash it)
                const float cap = std::max (std::max (peakHold, 1.0e-4f), std::min (1.4125f * std::max (peakHold, 1.0e-4f), 0.955f)) * std::max (1.0f, outGain);
                // (the cap only ever takes back the CHANGE it makes - the dry track is under it already - so the side, and
                //  whatever it isn't changing, stay exactly as they came)
                auto room = [cap] (float d, float c) noexcept { return std::abs (d + c) <= cap || std::abs (c) < 1.0e-9f ? 1.0f
                                                                  : std::clamp (((d + c > 0.0f ? cap : -cap) - d) / c, 0.0f, 1.0f); };
                const float need = std::min (room (dL, change * limGain), room (dR, change * limGain)) * limGain;
                limGain = need < limGain ? need : limGain + 0.0005f * (1.0f - limGain);
                outL = std::clamp (dL + change * limGain, -cap, cap); outR = std::clamp (dR + change * limGain, -cap, cap);
                // a howl: muted until the input has been quiet for a second
                if (howl)
                {
                    howlQuietS = dp < 0.003f ? howlQuietS + 1.0f / (float) sr : 0.0f;
                    if (howlQuietS > 1.0f) { howl = false; howlScoreS = 0.0f; howlQuietS = 0.0f; }
                }
                howlGain += ((howl ? 0.0f : 1.0f) - howlGain) * (howl ? 0.003f : 0.0005f);
                io[0][i] = howlGain * outL;
                io[1][i] = howlGain * outR;
                // the screen's input envelope; the howl watch
                inEnvRing[(size_t) inEnvPos] = dM; inEnvPos = (inEnvPos + 1) % nFft;
                howlEnv[(size_t) howlPos] = x; howlPos = (howlPos + 1) % howlN;
                if (++sinceHowl >= howlHop) { sinceHowl = 0; howlFrame(); }
            }
            // the screen, every ~20 ms
            if ((histTick += n) >= (int) (0.02 * sr))
            {
                histTick = 0;
                bandsOf (inEnvRing, inEnvPos, envIn); bandsOf (envRing, envPos, envOut);
                formantsOf (envIn, formIn); formantsOf (envOut, formOut);
                histIn[(size_t) histPos] = voicedNow ? f0In : 0.0f; histOut[(size_t) histPos] = voicedNow ? f0Out : 0.0f;
                histPos = (histPos + 1) % 32;
            }
            if (profile) profAll += nowS() - pAll;
            setMeter (vadGain * (std::abs (std::log2 (ratioNow)) + std::abs (std::log2 (alphaNow)) + breath + whisperAmt));
        }
    };

    /** VOCAL IDENTITY PROCESSOR (VIP-12): another person's voice - its knobs, straight. */
    class VoiceIdentity final : public VoiceEngine
    {
        Knobs knobsFrom (const float* q) const noexcept override
        {
            Knobs k;
            k.character = q[1]; k.mode = q[2]; k.pitch = q[3]; k.formant = q[4]; k.age = q[5]; k.gender = q[6]; k.breath = q[7]; k.fry = q[8];
            k.rough = q[9]; k.vibrato = q[10]; k.gate = q[11]; k.deess = q[12]; k.mix = q[13]; k.output = q[14]; k.smooth = q[15]; k.multiply = q[16]; k.strength = q[17];
            return k;
        }
    };

    /** PITCH CORRECTOR (PC-7): autotune. Each period of the voice is moved to the nearest note of the key's scale
        (PSOLA: the formants stay, it is still the same voice) - SPEED from an instant, stepped snap to a slow,
        invisible correction; HUMANIZE lets vibrato through; AMOUNT how far to the note. AUTO finds the key and scale
        from the whole input, corrects gently when it is nearly in tune and firmly when it is well off, and keeps the
        level. Only a voice is corrected; music passes as it came. */
    class PitchCorrector final : public VoiceEngine
    {
        Knobs knobsFrom (const float* q) const noexcept override
        {
            Knobs k;
            k.autoTune = q[1] > 0.5f; k.key = q[2]; k.scale = q[3]; k.speedMs = q[4]; k.humanize = q[5]; k.tune = q[6] * 0.01f;
            k.formant = q[7]; k.mode = q[8]; k.gate = q[9]; k.mix = q[10]; k.output = q[11]; k.smooth = q[12]; k.multiply = 1.0f; k.strength = q[14];
            return k;
        }
    };

    /** VOCAL TUNING AND IDENTITY PROCESSOR (VT-24): everything in one - the twelve characters, pitch, formants, age
        and gender, the glottal source, and the pitch correction with AUTO. */
    class VocalStation final : public VoiceEngine
    {
        Knobs knobsFrom (const float* q) const noexcept override
        {
            Knobs k;
            k.character = q[1]; k.autoTune = q[2] > 0.5f; k.key = q[3]; k.scale = q[4]; k.speedMs = q[5]; k.humanize = q[6]; k.tune = q[7] * 0.01f;
            k.pitch = q[8]; k.formant = q[9]; k.age = q[10]; k.gender = q[11]; k.breath = q[12]; k.fry = q[13]; k.rough = q[14]; k.vibrato = q[15];
            k.gate = q[16]; k.deess = q[17]; k.mode = q[18]; k.mix = q[19]; k.output = q[20]; k.smooth = q[21]; k.multiply = q[22]; k.strength = q[23];
            return k;
        }
    };
}
