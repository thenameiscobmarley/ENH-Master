#pragma once

#include "BandAnalyzer.h"

namespace enh::dsp
{
    /** Multi-region footstep detector for game audio (control rate).

        Game footsteps vary a lot by surface, gear and distance (e.g. Call of Duty: concrete
        clicks, wooden thumps, metal clanks, gravel/grass crunch, gear rustle, muffled steps
        through walls). The detector therefore watches five regions:

            thump  60-250 Hz   heel impact, heavy boots, floors
            body   250-500 Hz  wood, stairs, hollow floors
            click  1.3-2.6 kHz hard soles, metal, tile
            scuff  2.6-7 kHz   concrete scuffs, grit
            crunch 7-12 kHz    gravel, grass, gear rustle

        An onset in any region (or several at once) that stands out from that region's own
        background counts. Onsets are rejected or down-weighted when they are near full scale
        or broadband (gunfire, explosions), dominated by the voice region (450-1100 Hz), or keep
        ringing past ~110 ms. A click-only onset (the region voices and UI sounds share most)
        needs support from another region or a walking rhythm. Repeating onsets 0.22-0.9 s apart
        raise confidence.

        Besides the confidence, it reports WHICH regions made the step (dynamicWeight), so the
        adaptive EQ lifts the frequencies of the actual footstep instead of fixed ones.
    */
    class FootstepDetector
    {
    public:
        void prepare (const BandAnalyzer&, double controlRate);
        void reset();
        float update (const BandAnalyzer&, float dt) noexcept;

        float getConfidence() const noexcept { return confidence; }
        int getEventCount() const noexcept   { return eventCount; }

        /** Per band: where the current footstep lives (0..1), and regions that usually mask steps. */
        std::array<float, numBands> dynamicWeight {}, competitorWeight {};

        /** Last computed decision factors (for tests / diagnostics). */
        struct Trace { float onset, balance, share, hot, broadband, midDominance, sustain, raw, confirmed, rhythm; };
        Trace trace {};

    private:
        struct Group
        {
            std::array<bool, numBands> member {};
            float background = -90.0f;
            bool active = false;

            float power (const std::array<PowerFollower, numBands>& f) const noexcept
            {
                float sum = 0.0f;
                for (int k = 0; k < numBands; ++k)
                    if (member[(size_t) k])
                        sum += f[(size_t) k].env;
                return sum;
            }
        };

        static constexpr int numRegions = 5;
        enum Region { thump = 0, body, click, scuff, crunch };

        struct RegionState
        {
            Group group;
            float trust = 1.0f;
            float flux = 0, prominence = 0, level = -120.0f, score = 0;
            std::array<float, numBands> bell {};
        };

        std::array<RegionState, numRegions> regions {};
        std::array<float, numRegions> regionMix {};
        Group voice;

        float confidence = 0.0f, rhythm = 0.0f, sustainPenalty = 0.0f, hotHold = 0.0f, eventPeakDb = -120.0f, fullBackground = -90.0f;
        double clock = 0.0, lastEvent = -10.0, lastInterval = 0.0;
        int eventCount = 0;

        static constexpr int maxConfirmTicks = 32;
        std::array<float, maxConfirmTicks> rawHistory {};
        int historyPos = 0, confirmTicks = 6;
    };
}
