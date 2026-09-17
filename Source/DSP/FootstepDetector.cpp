#include "FootstepDetector.h"

namespace enh::dsp
{
    void FootstepDetector::prepare (const BandAnalyzer& analyzer, double controlRate)
    {
        confirmTicks = std::clamp ((int) std::lround (0.004 * controlRate), 1, maxConfirmTicks);

        struct Def { double lo, hi, centre, width; float trust; };
        constexpr std::array<Def, numRegions> defs {{
            { 60.0,   250.0,   130.0,  0.9, 1.00f },   // thump
            { 250.0,  500.0,   350.0,  0.6, 0.85f },   // body
            { 1300.0, 2600.0,  1900.0, 0.7, 0.75f },   // click
            { 2600.0, 7000.0,  4000.0, 0.9, 1.00f },   // scuff
            { 7000.0, 12500.0, 9500.0, 0.7, 0.85f },   // crunch
        }};

        for (int r = 0; r < numRegions; ++r)
        {
            auto& reg = regions[(size_t) r];
            const auto& d = defs[(size_t) r];
            reg.trust = d.trust;
            reg.group.active = false;

            for (int k = 0; k < numBands; ++k)
            {
                const double hz = BandAnalyzer::centreHz (k);
                const bool on = analyzer.isActive (k);
                reg.group.member[(size_t) k] = on && hz >= d.lo && hz <= d.hi;
                reg.group.active = reg.group.active || reg.group.member[(size_t) k];
                reg.bell[(size_t) k] = on ? octaveBell (hz, d.centre, d.width) : 0.0f;
            }
        }

        voice.active = false;
        for (int k = 0; k < numBands; ++k)
        {
            const double hz = BandAnalyzer::centreHz (k);
            const bool on = analyzer.isActive (k);
            voice.member[(size_t) k] = on && hz >= 450.0 && hz <= 1100.0;
            voice.active = voice.active || voice.member[(size_t) k];

            // Rumble and the voice/weapon-body region are what usually buries footsteps
            competitorWeight[(size_t) k] = on ? std::max (octaveBell (hz, 40.0, 0.7), octaveBell (hz, 700.0, 0.8)) : 0.0f;
        }

        reset();
    }

    void FootstepDetector::reset()
    {
        for (auto& r : regions)
        {
            r.group.background = -90.0f;
            r.flux = r.prominence = r.score = 0.0f;
            r.level = -120.0f;
        }

        voice.background = -90.0f;

        // Until a step has been heard, assume the classic heel thump + scuff
        regionMix.fill (0.0f);
        regionMix[thump] = 0.5f;
        regionMix[scuff] = 0.5f;
        dynamicWeight.fill (0.0f);

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
            if (! g.active)
            {
                flux = prominence = 0.0f;
                level = -120.0f;
                return;
            }

            const float transientDb = powerToDb (g.power (a.transient));
            const float shortDb     = powerToDb (g.power (a.shortTerm));

            flux = std::max (0.0f, transientDb - shortDb);
            prominence = shortDb - g.background;
            level = shortDb;

            // Background from the short-term level (~95 dB/s release): recovers right after
            // loud events. Falls fast, rises slowly.
            const float k = 1.0f - std::exp (-dt / (shortDb < g.background ? 0.12f : 2.5f));
            g.background += (shortDb - g.background) * k;
        };

        // --- per-region onset scores -----------------------------------------------------
        float best = 0.0f, second = 0.0f, scoreSum = 0.0f, stepLevel = -120.0f, bestFlux = 0.0f, bestProm = 0.0f;
        int bestRegion = -1;

        for (int r = 0; r < numRegions; ++r)
        {
            auto& reg = regions[(size_t) r];
            measure (reg.group, reg.flux, reg.prominence, reg.level);

            reg.score = reg.group.active
                          ? saturate01 ((reg.flux - 2.0f) / 6.0f) * saturate01 ((reg.prominence - 3.0f) / 9.0f) * reg.trust
                          : 0.0f;

            scoreSum += reg.score;
            stepLevel = std::max (stepLevel, reg.level);
            bestFlux = std::max (bestFlux, reg.flux);
            bestProm = std::max (bestProm, reg.prominence);

            if (reg.score > best)
            {
                second = best;
                best = reg.score;
                bestRegion = r;
            }
            else if (reg.score > second)
            {
                second = reg.score;
            }
        }

        float fluxVoice = 0, promVoice = 0, levelVoice = 0;
        measure (voice, fluxVoice, promVoice, levelVoice);

        // Strongest region, plus a bonus when several regions fire together (a real impact)
        float onset = saturate01 (best * 0.8f + std::sqrt (best * second) * 0.6f);

        // A click-only onset shares its range with UI sounds and consonants: it needs another
        // region or a walking rhythm behind it
        if (bestRegion == click && second < 0.3f)
            onset *= 0.35f + 0.65f * rhythm;

        // --- rejections ----------------------------------------------------------------------
        hotHold = std::max (saturate01 ((std::max (a.fullShortDb, a.fullTransientDb - 3.0f) + 16.0f) / 8.0f),
                            hotHold * std::exp (-dt / 0.15f));
        const float hot = hotHold;

        // Broadband: thump, scuff AND voice regions all rising together (gunfire, explosions)
        const float broadband = saturate01 ((std::min ({ regions[thump].flux, regions[scuff].flux, fluxVoice }) - 4.0f) / 4.0f);
        const float midDominance = saturate01 ((fluxVoice - bestFlux - 1.0f) / 4.0f);
        const float silence = saturate01 ((-75.0f - a.fullShortDb) / 10.0f);

        // Spectral shape at the onset: step regions should outweigh the voice region, or the rise
        // itself should be step-centric; and they should carry a real share of the level (or have
        // clearly jumped above their own background under a louder bed)
        const float levelBalance = saturate01 ((stepLevel - levelVoice + 2.0f) / 8.0f);
        const float riseBalance = saturate01 ((bestFlux - fluxVoice + 1.0f) / 5.0f);
        const float balance = std::max (levelBalance, riseBalance);
        const float share = std::max (saturate01 ((stepLevel - a.fullShortDb + 14.0f) / 8.0f),
                                      saturate01 ((bestProm - 6.0f) / 8.0f));
        const float shape = (0.35f + 0.65f * balance) * (0.3f + 0.7f * share);

        // Sustain check: footsteps decay within ~100 ms, shots/explosions keep ringing
        const double sinceEvent = clock - lastEvent;
        fullBackground += (a.fullShortDb - fullBackground) * (1.0f - std::exp (-dt / (a.fullShortDb < fullBackground ? 0.3f : 3.0f)));

        if (sinceEvent > 0.11 && sinceEvent < 0.20 && eventPeakDb - fullBackground > 10.0f
            && a.fullTransientDb > eventPeakDb - 4.0f)
            sustainPenalty = 1.0f;
        sustainPenalty *= std::exp (-dt / 0.35f);

        const float gate = shape * (1.0f - 0.8f * hot) * (1.0f - 0.7f * broadband);
        const float raw = onset * gate * (1.0f - 0.6f * midDominance) * (1.0f - silence) * (1.0f - 0.9f * sustainPenalty);

        if (sinceEvent < 0.11)
            eventPeakDb = std::max (eventPeakDb, a.fullTransientDb);

        // --- which regions made this step (drives the dynamic EQ lift) -----------------------------
        if (raw > 0.3f && scoreSum > 0.0f)
        {
            const float k = 1.0f - std::exp (-dt / 0.01f);
            for (int r = 0; r < numRegions; ++r)
                regionMix[(size_t) r] += (regions[(size_t) r].score / scoreSum - regionMix[(size_t) r]) * k;
        }

        // --- walking rhythm ------------------------------------------------------------------------
        if (onset * gate > 0.4f && sinceEvent > 0.12)
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

        // Per-band lift weights for the step that is (or was just) happening
        float peak = 1.0e-3f;
        for (int k = 0; k < numBands; ++k)
        {
            float w = 0.0f;
            for (int r = 0; r < numRegions; ++r)
                w += regionMix[(size_t) r] * regions[(size_t) r].bell[(size_t) k];
            dynamicWeight[(size_t) k] = w;
            peak = std::max (peak, w);
        }

        for (auto& w : dynamicWeight)
            w = std::min (1.0f, w / peak);

        return confidence;
    }
}
