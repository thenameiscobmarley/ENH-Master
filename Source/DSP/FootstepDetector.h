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

        Adapting to the game, not a fixed idea of a footstep:
          - how fast steps die away is learnt from the steps it accepts: in a reverberant game, or on
            wood and carpet, steps decay more slowly, and the decay test scales with them (bounded,
            so a slow swell can never pass as a step);
          - an event that failed only on decay at 42 ms (short, noise-like, not hot, not clutter) gets a
            second look at 70 ms over the longer window - steps with a room tail, on wood, far away.
        Both relaxations apply only to an impact: an event that reached its peak within 12 ms of its
        onset, stands well clear of its background (>= 14 dB) and is outside tonal activity. A
        syllable is a short burst that fades too, but it swells up over tens of ms, so for speech
        the default requirement stands.

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

        /** Tests: switch the adaptive decay and the second look off, to compare with fixed rules. */
        void setAdaptive (bool on) noexcept  { adaptive = on; }
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
        float eventExcessDb() const noexcept;   // how far the event stands above its own background

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
        bool adaptive = true;
        float decayScale = 1.0f;                 // this event's decay requirement (1 = the default)
        bool secondLook = false;                 // decay-limited: re-judged at secondLookTime
        float eventPeakEnergy = 0.0f;            // the event's loudest moment so far (transient power)
        double peakAt = 0.0;                     // ... and when, after the onset (impacts peak within ms)
        float othersAtDecision = 0.0f;           // the decision's score without its decay factor
        float learnedDrop = 10.0f;               // typical drop (dB) of accepted steps at the decision

        // History / sequence
        float clutterCount = 0.0f, rhythm = 0.0f, hotHold = 0.0f, suspicion = 0.0f, suspicionAtDecision = 0.0f;
        double lastStepTime = -10.0, lastInterval = 0.0;
        std::array<float, numBands> fingerprint {};
        float fingerprintStrength = 0.0f;

        float confidence = 0.0f, confidenceTarget = 0.0f;
        int eventCount = 0, rejectedCount = 0, activeCount = 0;
    };
}
