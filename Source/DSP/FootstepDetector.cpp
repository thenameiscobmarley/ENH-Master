#include "FootstepDetector.h"

namespace enh::dsp
{
    void FootstepDetector::prepare (const BandAnalyzer& analyzer, double controlRate)
    {
        confirmTicks = std::clamp ((int) std::lround (0.004 * controlRate), 1, maxConfirmTicks);

        for (int k = 0; k < numBands; ++k)
        {
            const double hz = BandAnalyzer::centreHz (k);
            const bool on = analyzer.isActive (k);
            const auto i = (size_t) k;

            low.member[i]  = on && hz >= 70.0 && hz <= 260.0;
            high.member[i] = on && hz >= 1800.0 && hz <= 7000.0;
            mid.member[i]  = on && hz >= 400.0 && hz <= 1300.0;

            stepWeight[i] = on ? std::max (octaveBell (hz, 140.0, 0.9), octaveBell (hz, 3600.0, 1.0)) : 0.0f;
            competitorWeight[i] = on ? std::max (octaveBell (hz, 40.0, 0.7), octaveBell (hz, 700.0, 0.8)) * (1.0f - stepWeight[i]) : 0.0f;
        }

        reset();
    }

    void FootstepDetector::reset()
    {
        low.background = high.background = mid.background = -90.0f;
        confidence = rhythm = sustainPenalty = hotHold = 0.0f;
        eventPeakDb = -120.0f;
        fullBackground = -90.0f;
        clock = 0.0;
        lastEvent = -10.0;
        lastInterval = 0.0;
        eventCount = 0;
        rawHistory.fill (0.0f);
        historyPos = 0;
    }

    float FootstepDetector::update (const BandAnalyzer& a, float dt) noexcept
    {
        clock += dt;

        auto measure = [&] (Group& g, float& flux, float& prominence, float& level)
        {
            const float transientDb = powerToDb (g.power (a.transient));
            const float shortDb     = powerToDb (g.power (a.shortTerm));

            flux = std::max (0.0f, transientDb - shortDb);
            prominence = shortDb - g.background;
            level = shortDb;

            // Background tracks the region's floor from the short-term level (~48 dB/s release),
            // so it recovers right after loud events: falls fast, rises slowly.
            const float k = 1.0f - std::exp (-dt / (shortDb < g.background ? 0.12f : 2.5f));
            g.background += (shortDb - g.background) * k;
        };

        float fluxLow = 0, promLow = 0, levelLow = 0, fluxHigh = 0, promHigh = 0, levelHigh = 0, fluxMid = 0, promMid = 0, levelMid = 0;
        measure (low, fluxLow, promLow, levelLow);
        measure (high, fluxHigh, promHigh, levelHigh);
        measure (mid, fluxMid, promMid, levelMid);

        const float sLow  = saturate01 ((fluxLow - 2.0f) / 6.0f)  * saturate01 ((promLow - 3.0f) / 9.0f);
        const float sHigh = saturate01 ((fluxHigh - 2.0f) / 6.0f) * saturate01 ((promHigh - 3.0f) / 9.0f);
        const float onset = saturate01 (std::max (sLow, sHigh) * 0.8f + std::sqrt (sLow * sHigh) * 0.6f);

        // Rejections: very hot program (gunfire / explosions), broadband onsets, mid-dominated onsets, silence
        // Hold the loud-event rejection briefly so shot / explosion tails don't read as steps
        hotHold = std::max (saturate01 ((std::max (a.fullShortDb, a.fullTransientDb - 3.0f) + 16.0f) / 8.0f), hotHold * std::exp (-dt / 0.15f));
        const float hot = hotHold;
        const float broadband = saturate01 ((std::min ({ fluxLow, fluxHigh, fluxMid }) - 4.0f) / 4.0f);
        const float midDominance = saturate01 ((fluxMid - std::max (fluxLow, fluxHigh) - 1.0f) / 4.0f);
        const float silence = saturate01 ((-75.0f - a.fullShortDb) / 10.0f);

        // Spectral shape at the onset: step regions should outweigh the mids (voices, weapon
        // bodies are mid-heavy) and carry a real share of the total level right now.
        const float stepLevel = std::max (levelLow, levelHigh);
        const float levelBalance = saturate01 ((stepLevel - levelMid + 2.0f) / 8.0f);
        const float riseBalance = saturate01 ((std::max (fluxLow, fluxHigh) - fluxMid + 1.0f) / 5.0f);  // the rise itself is step-centric
        const float balance = std::max (levelBalance, riseBalance);
        // ...or, under a louder bed (e.g. voice), has clearly jumped above its own background
        const float share = std::max (saturate01 ((stepLevel - a.fullShortDb + 14.0f) / 8.0f),
                                      saturate01 ((std::max (promLow, promHigh) - 6.0f) / 8.0f));
        const float shape = (0.35f + 0.65f * balance) * (0.3f + 0.7f * share);

        // Sustain check: footsteps decay within ~100 ms, shots/explosions keep ringing
        const double sinceEvent = clock - lastEvent;
        fullBackground += (a.fullShortDb - fullBackground) * (1.0f - std::exp (-dt / (a.fullShortDb < fullBackground ? 0.3f : 3.0f)));

        if (sinceEvent > 0.11 && sinceEvent < 0.20 && eventPeakDb - fullBackground > 10.0f
            && a.fullTransientDb > eventPeakDb - 4.0f)
            sustainPenalty = 1.0f;
        sustainPenalty *= std::exp (-dt / 0.35f);

        const float raw = onset * shape * (1.0f - 0.8f * hot) * (1.0f - 0.7f * broadband) * (1.0f - 0.6f * midDominance)
                        * (1.0f - silence) * (1.0f - 0.9f * sustainPenalty);

        if (sinceEvent < 0.11)
            eventPeakDb = std::max (eventPeakDb, a.fullTransientDb);

        // Walking rhythm
        if (onset * shape * (1.0f - 0.8f * hot) * (1.0f - 0.7f * broadband) > 0.4f && sinceEvent > 0.12)
        {
            const double interval = sinceEvent;

            if (interval > 0.22 && interval < 0.9)
            {
                const bool steady = lastInterval > 0.0 && std::abs (interval - lastInterval) < 0.25 * lastInterval;
                rhythm = steady ? std::min (1.0f, rhythm + 0.35f) : rhythm * 0.7f;
                lastInterval = interval;
            }
            else
            {
                lastInterval = 0.0;
            }

            lastEvent = clock;
            eventPeakDb = a.fullTransientDb;
            ++eventCount;
        }

        rhythm *= std::exp (-dt / 2.5f);

        // Confirmation: the onset must hold for ~4 ms. Rejection cues (mid ring-up, level)
        // lag a gunshot's first milliseconds; a footstep easily outlasts the window.
        rawHistory[(size_t) historyPos] = raw;
        historyPos = (historyPos + 1) % confirmTicks;
        float confirmed = raw;
        for (int i = 0; i < confirmTicks; ++i)
            confirmed = std::min (confirmed, rawHistory[(size_t) i]);

        trace = { onset, balance, share, hot, broadband, midDominance, sustainPenalty, raw, confirmed, rhythm };
        const float target = saturate01 (confirmed * (1.35f + 0.4f * rhythm));
        confidence = std::max (target, confidence * std::exp (-dt / 0.2f));
        return confidence;
    }
}
