#pragma once

#include <array>
#include <vector>
#include "BandAnalyzer.h"

namespace enh::dsp
{
    /** SPECTRAL LIMITER - frequency-selective control of abnormal energy, so a loud event in one
        region does not turn the whole mix down ("anti-pumping").

        It sits between the UPWARD LEVELER and the ADAPTIVE COMPRESSOR, and exists because the
        compressor's detector is broadband: a sudden bass hit used to pull the mids and highs
        (footsteps, detail) down with it. Here the hit is handled where it is.

        Detection shares ENH Master's 24-band analyser (BandAnalyzer): its band filters and
        short-term level followers are read, not duplicated. Per band this adds only:
          - a rolling baseline (the band's mean power over ~3 s), which all but stops while the band
            is flagged, so the spike it is judging cannot drag it up;
          - what is normal for this band: the 97th percentile of how far it went above that
            baseline over the last 10 s, not counting the most recent 1.5 s - so a kick drum
            teaches it that its hits are normal, while a sudden rumble is not learnt while it lasts;
          - leakage rejection: the analyser's band-passes have gentle skirts, so a huge 70 Hz hit
            also raises the 400 Hz band. A band only counts when its excess is not explained by
            leakage from bands an octave or more away (the filters' own response, precomputed).
        Only the part of a band that exceeds its own normal is "excess".

        Two reasons to cut, both handled the same way - by moving cuts confined to the offending
        region, never by turning the whole mix down first:
          1. TONE: a region jumps out of balance with the rest of the spectrum (its excess beyond its
             own normal, and not shared by the other regions - a louder mix overall is not a reason).
             Up to three moving cuts (bell, or a shelf when the excess runs off the bottom or top of
             the spectrum) follow it, up to RANGE; nothing else moves. This is tonal correction, not
             loudness control: how loud the programme is does not enter into it.
          2. CLIPPING: the event would take this stage over the CEILING (0 dBFS by default) -> that
             same region is cut deeper, by what its share of the energy says is needed to get back
             under; only when the abnormal energy covers most of the spectrum is broadband gain used,
             for what is left.
        Normal material, however loud, is left to the compressor and the output limiter (which also
        cuts the region responsible for an over before it touches the whole mix).

        The compressor is also keyed through this unit: key() is the output with the flagged regions
        taken down to their baseline (the same moving filters, deeper). While a localised event is
        being handled here, the compressor does not duck the whole mix for it as well - an adaptive
        version of the side-chain high-pass engineers put on bus compressors so the bass cannot pump
        them. With nothing flagged the key is the audio itself, so the compressor is unchanged; as an
        event becomes broadband the exclusion fades out and the compressor sees all of it.

        Timing is exact regardless of host block size: each analyser tick schedules its decision at
        the sample it was made. Zero latency (IIR, TPT state-variable filters, modulation-safe).
        Real-time safe after prepare(): no allocation, no locking.
    */
    class SpectralLimiter
    {
    public:
        static constexpr int numSlots = 3;

        struct Settings
        {
            float rangeDb = 9.0f;        // deepest spectral cut (0..18 dB)
            float releaseMs = 150.0f;    // how quickly a cut lets go (attack follows it)
            float ceilingDb = 0.0f;      // it acts only on peaks over this (dBFS); 0 dBFS: only real overs
            bool active = false;         // the plugin's parameter default is In; raw settings stay inert
        };

        enum class Shape { bell, lowShelf, highShelf };

        /** One moving cut as applied right now (depthDb >= 0 is the attenuation). */
        struct Slot
        {
            Shape shape = Shape::bell;
            float hz = 1000.0f, octaves = 1.0f, depthDb = 0.0f;
        };

        void prepare (double sampleRate, int maxBlockSize, int controlInterval);
        void reset();

        /** Call at the start of every engine chunk, before the analyser ticks of that chunk. */
        void beginChunk() noexcept { pending = 0; }

        /** Analyser tick: reads the shared band analysis and schedules its decision at `offset`
            samples into the current chunk. */
        void analyse (const BandAnalyzer&, float dt, int offset) noexcept;

        /** Applies the scheduled decisions to the chunk (the audio at this point in the chain) and
            writes the compressor's key signal for it (see key()). */
        void process (float* const* channels, int numChannels, int numSamples, const Settings&) noexcept;

        /** The key for the compressor after this unit, for the chunk just processed (two channels). */
        const float* const* key() const noexcept { return keyPointers.data(); }

        /** The cuts as applied right now, headroom protection included. */
        std::array<Slot, numSlots> getSlots() const noexcept;
        float getBroadbandDb() const noexcept   { return broadbandDb; }   // >= 0, gain reduction
        float getMakeupDb() const noexcept      { return makeupDb; }      // >= 0, the loudness keeper's lift
        float getDeepestCutDb() const noexcept;
        float getLocalisation() const noexcept  { return localised; }     // 1 = localised, 0 = broadband

        /** True while a localised abnormal event is being handled (and for 0.5 s after): level-matching
            loops elsewhere hold still rather than chase the spike (analyser rate, from analyse()). */
        bool isHandlingLocalisedEvent() const noexcept { return eventHoldS > 0.0f; }

        /** Per analyser band, at the last tick: excursion above the long-term spectrum, what is normal
            for that band, and the excess beyond it (dB). For the tests and the --analyze report. */
        struct BandView { std::array<float, numBands> excursionDb {}, normalDb {}, overDb {}; };
        const BandView& getBandView() const noexcept { return view; }

        /** Magnitude (dB, <= 0) of all the cuts together at a frequency - the analogue prototypes,
            for the display and the tests. */
        static float responseDb (const std::array<Slot, numSlots>&, float hz) noexcept;

    private:
        struct Target
        {
            Shape shape = Shape::bell;
            float octave = 0.0f, width = 1.0f, depthDb = 0.0f;   // octave = log2 (hz / 1 kHz)
            float excursionDb = 0.0f;                             // how far above its baseline it is
            float share = 0.0f;                                   // this region's share of the energy
        };

        struct Decision
        {
            int offset = 0;
            std::array<Target, numSlots> targets {};
            float localised = 1.0f;     // 1 = the excess is confined to a region
            bool abnormal = false;
            std::array<float, numBands> powerK {}, usualK {};   // per band now / usually, ear-weighted (loudness keeper)
        };

        struct Svf
        {
            float ic1 = 0.0f, ic2 = 0.0f;
            void reset() noexcept { ic1 = ic2 = 0.0f; }
        };

        struct Coeffs { float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, m0 = 1.0f, m1 = 0.0f, m2 = 0.0f; };

        void controlStep (const Settings&) noexcept;
        void design (int slot) noexcept;
        static void designCut (Coeffs&, Shape, float hz, float octaves, float depthDb, double sampleRate) noexcept;

        double sr = 48000.0;
        int interval = 32, stepCountdown = 32;
        float stepDt = 1.0f / 1500.0f;

        // Detection (analyser rate)
        static constexpr int blockHistory = 200, blockDelay = 30;   // 10 s of 50 ms blocks, 1.5 s behind
        static constexpr int ringLength = blockHistory + blockDelay;

        BandView view;
        std::array<float, numBands> basePower {};      // rolling baseline per band (power)
        std::array<float, numBands> normalExcess {};   // how far above baseline this band normally goes
        std::array<float, numBands> normalGain {};     // the same as a power ratio
        std::array<float, numBands> blockMax {};       // this block's highest excursion per band
        std::array<bool, numBands> blockFlagged {}, flaggedNow {};   // flagged: enters 3 dB over normal, leaves at 0
        std::array<std::array<float, ringLength>, numBands> history {};
        std::array<float, blockHistory> scratch {};
        std::array<std::array<float, numBands>, numBands> leak {};   // power response of band k at band j's centre
        std::array<float, 32> localHistory {};         // the last ~10 ms of localisation judgements
        int localHead = 0, localTicks = 15;
        int ringHead = 0, ringFilled = 0, blockTicks = 75, blockTick = 0;
        bool baselineSet = false;
        float heardSeconds = 0.0f, eventHoldS = 0.0f;
        std::array<Target, numSlots> assigned {};      // the slot each region was matched to
        std::vector<Decision> schedule;                // this chunk's decisions, in order
        int pending = 0;
        Decision current {};

        // Processing (stage rate)
        std::array<Slot, numSlots> slots {};
        std::array<Coeffs, numSlots> coeffs {}, keyCoeffs {};
        std::array<std::array<Svf, numSlots>, 2> svf {}, keySvf {};
        std::array<float, numSlots> keyDb {};           // extra depth in the compressor's key only
        std::array<bool, numSlots> audioLive {}, keyLive {};   // a filter at 0 dB is skipped (and restarts clean)
        std::vector<float> keyBuffer;
        std::array<float*, 2> keyPointers {};
        std::array<float, numSlots> extraDb {};        // headroom protection on top of the spectral cut
        float peakEnv = 0.0f, broadbandDb = 0.0f, broadbandGain = 1.0f, localised = 1.0f;
        float makeupDb = 0.0f, makeupGain = 1.0f;          // the loudness keeper's lift
        float keeperLostDb = 0.0f;                          // usual loudness the cuts take away (dB)
        int keeperStep = 0;

        /** Roughly how much a band counts toward loudness (the K-weighting's shape: little below 60 Hz,
            +4 dB above 2 kHz), per unit of its power. */
        static float loudnessWeight (int band) noexcept
        {
            const float f = (float) BandAnalyzer::centreHz (band);
            const float lowCut = (f * f) / (f * f + 60.0f * 60.0f);
            return lowCut * (1.0f + 1.5f * (f * f) / (f * f + 1500.0f * 1500.0f));
        }
        float peakOutEnv = 0.0f, headroomFeedback = 0.0f;   // what is still over CEILING after the cuts
    };
}
