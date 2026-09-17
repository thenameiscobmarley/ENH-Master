#pragma once

#include "BandAnalyzer.h"
#include "SpectralAnalyzer.h"

namespace enh::dsp
{
    /** Event-based footstep classifier for game audio (control rate).

        Many game sounds are short transients that share a footstep's frequency range
        (crate latches and lids, rummaging, reloads, UI clicks), so frequency alone cannot
        identify a step. Every onset is treated as an event and judged on what it does,
        not only where its energy sits:

          onset     a sharp rise above each band's own background, in any band
          decay     a step dies away quickly (>= ~7 dB within 42 ms, >= 9 dB by 110 ms);
                    latches, lids and hinges ring or keep going
          noise     a step is a noise burst: its new energy has no persistent spectral peaks
                    (FFT, see SpectralAnalyzer); metal rings, creaks, chimes and voices do
          clutter   steps come one at a time, >= 0.2 s apart; rummaging / rattling
                    produces bursts of similar onsets 40-150 ms apart (a different sound
                    overlapping a step, e.g. a voice, does not count)
          context   onsets inside ongoing tonal activity (a creaking lid, speech) are suspect,
                    and so is the second after a rejected ringing / rattling event (a crate's
                    lid thud after its latch and rummage)
          sequence  steps repeat with a steady interval and the same spectral fingerprint
                    (same shoes, same surface); a matching event is trusted more
          level     near-full-scale or broadband onsets (gunfire, explosions) are rejected

        Timeline of an event: +4 ms a small provisional confidence (larger when the event fits
        the current walking sequence), +42 ms the full decision, until +160 ms a watch for
        ringing / sustain / re-attacks that retracts the decision. Only events that survive
        are learned into the rhythm and fingerprint.

        dynamicWeight describes where THIS step's energy actually rose above the background
        (with a gentle preference for the 1-4 kHz detail region that carries footsteps in
        games such as Call of Duty), so the EQ lifts the real step instead of a fixed region.
    */
    class FootstepDetector
    {
    public:
        void prepare (const BandAnalyzer&, double controlRate);
        void reset();
        float update (const BandAnalyzer&, SpectralAnalyzer&, float dt) noexcept;

        float getConfidence() const noexcept { return confidence; }
        int getEventCount() const noexcept   { return eventCount; }
        int getRejectedCount() const noexcept { return rejectedCount; }

        /** Per band: where the current footstep lives (0..1), and nearby bands that mask it (0..1). */
        std::array<float, numBands> dynamicWeight {}, competitorWeight {};

        enum class Phase { idle, provisional, accepted, rejected };

        /** Decision factors of the current / last event (tests, diagnostics). */
        struct Trace
        {
            float strength, decay, tonal, context, clutter, hot, broadband, midDominance, sequence, score, confidence;
            Phase phase;
        };
        Trace trace {};

    private:
        void startEvent (const BandAnalyzer&, SpectralAnalyzer&) noexcept;
        void evaluate (const BandAnalyzer&) noexcept;
        void retract() noexcept;
        void commit() noexcept;
        void computeShape (std::array<float, numBands>& shape) const noexcept;
        float fingerprintMatch() const noexcept;
        float eventDropDb (const BandAnalyzer&, bool useShortTerm) const noexcept;

        std::array<float, numBands> background {}, onsetBand {}, onsetPattern {}, emphasis {};
        std::array<bool, numBands> voiceBand {}, lowBand {}, highBand {};

        // Current event
        Phase phase = Phase::idle;
        double clock = 0.0, onsetTime = -10.0, lastOnsetTime = -10.0;
        std::array<float, numBands> preBackground {}, peakDb {};
        float strengthMax = 0, hotMax = 0, broadbandMax = 0, midMax = 0, risingTonalMax = 0;
        float contextTonal = 0, clutterAtOnset = 0, sequence = 0, expected = 0, eventScore = 0, lastDecay = 0;
        bool decided = false, armed = true;
        float strengthLow = 0.0f;

        // History / sequence
        float clutterCount = 0.0f, rhythm = 0.0f, hotHold = 0.0f, suspicion = 0.0f, suspicionAtDecision = 0.0f;
        double lastStepTime = -10.0, lastInterval = 0.0;
        std::array<float, numBands> fingerprint {};
        float fingerprintStrength = 0.0f;

        float confidence = 0.0f, confidenceTarget = 0.0f;
        int eventCount = 0, rejectedCount = 0, activeCount = 0;
    };
}
