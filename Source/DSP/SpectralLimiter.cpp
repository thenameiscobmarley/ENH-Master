#include "SpectralLimiter.h"

#include <complex>

namespace enh::dsp
{
    namespace
    {
        constexpr float bandSpacingOct = 0.3757f;   // log2 (400) / 23: the analyser's band spacing

        float bandOctave (int k) noexcept { return (float) std::log2 (BandAnalyzer::centreHz (k) / 1000.0); }

        /** Bandwidth in octaves -> Q of a bell. */
        float qForOctaves (float oct) noexcept
        {
            const float p = std::pow (2.0f, std::max (0.4f, oct));
            return std::sqrt (p) / (p - 1.0f);
        }
    }

    void SpectralLimiter::prepare (double sampleRate, int maxBlockSize, int controlInterval)
    {
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        interval = std::max (1, controlInterval);
        stepDt = (float) interval / (float) sr;
        // Every analyser tick in a chunk can schedule a decision (+2 for a partial tick at each end)
        schedule.assign ((size_t) (std::max (1, maxBlockSize) / interval + 2), Decision {});
        blockTicks = std::max (1, (int) std::lround (0.050 / stepDt));
        localTicks = std::clamp ((int) std::lround (0.003 / stepDt), 1, (int) localHistory.size());
        keyBuffer.assign ((size_t) (2 * std::max (1, maxBlockSize)), 0.0f);
        keyPointers = { keyBuffer.data(), keyBuffer.data() + std::max (1, maxBlockSize) };

        // How much of band j's energy each analyser band-pass (Q 2.5, as BandAnalyzer uses) picks up
        for (int k = 0; k < numBands; ++k)
            for (int j = 0; j < numBands; ++j)
            {
                const double r = BandAnalyzer::centreHz (j) / BandAnalyzer::centreHz (k), x = 2.5 * (r - 1.0 / r);
                leak[(size_t) k][(size_t) j] = (float) (1.0 / (1.0 + x * x));
            }
        reset();
    }

    void SpectralLimiter::reset()
    {
        // Prior: transients 8 dB over the baseline are normal until the programme says otherwise
        normalExcess.fill (8.0f);
        normalGain.fill (std::pow (10.0f, 0.8f));
        audioLive.fill (false);
        keyLive.fill (false);
        for (auto& h : history)
            h.fill (8.0f);
        blockMax.fill (-100.0f);
        blockFlagged.fill (false);
        flaggedNow.fill (false);
        localHistory.fill (1.0f);
        localHead = 0;
        basePower.fill (0.0f);
        baselineSet = false;
        ringHead = ringFilled = blockTick = 0;
        view = {};
        heardSeconds = 0.0f;
        eventHoldS = 0.0f;
        assigned = {};
        pending = 0;
        current = {};
        slots = {};
        extraDb = {};
        keyDb = {};
        for (int s = 0; s < numSlots; ++s)
            design (s);
        for (auto* bank : { &svf, &keySvf })
            for (auto& ch : *bank)
                for (auto& f : ch)
                    f.reset();
        stepCountdown = interval;
        peakEnv = 0.0f;
        peakOutEnv = 0.0f;
        headroomFeedback = 0.0f;
        broadbandDb = 0.0f;
        broadbandGain = 1.0f;
        localised = 1.0f;
    }

    std::array<SpectralLimiter::Slot, SpectralLimiter::numSlots> SpectralLimiter::getSlots() const noexcept
    {
        auto applied = slots;
        for (int i = 0; i < numSlots; ++i)
            applied[(size_t) i].depthDb += extraDb[(size_t) i];
        return applied;
    }

    float SpectralLimiter::getDeepestCutDb() const noexcept
    {
        float d = 0.0f;
        for (int i = 0; i < numSlots; ++i)
            d = std::max (d, slots[(size_t) i].depthDb + extraDb[(size_t) i]);
        return d;
    }

    //==============================================================================
    void SpectralLimiter::analyse (const BandAnalyzer& a, float dt, int offset) noexcept
    {
        const int n = a.getActiveCount();
        if (n <= 0)
            return;

        if (a.fullShortDb > -75.0f)
            heardSeconds += dt;

        // --- 1. per band: how far above its own normal is it right now? ------------------------
        if (! baselineSet && a.fullShortDb > -75.0f)
        {
            for (int k = 0; k < n; ++k)
                basePower[(size_t) k] = a.shortTerm.env[(size_t) k];
            baselineSet = true;
        }

        // The baseline settles quickly on the first seconds of programme, then integrates over ~3 s
        const float kBase = 1.0f - std::exp (-dt / std::clamp (heardSeconds, 0.2f, 3.0f));
        const float kBaseFlagged = 1.0f - std::exp (-dt / 30.0f);
        std::array<float, numBands> over {}, power {}, excess {}, excursions {};
        float total = 1.0e-12f;

        for (int k = 0; k < n; ++k)
        {
            const auto i = (size_t) k;
            power[i] = a.shortTerm.env[i];
            total += power[i];

            // The analyser's fast (transient) follower: a hit is seen within a few ms, before a
            // broadband compressor could react to it. What is normal is learnt from the same measure.
            const float level = std::max (-100.0f, a.transientDb[i]);
            const float excursion = level - powerToDb (basePower[i]);
            const float x = excursion - normalExcess[i];                  // beyond what is normal here
            const float content = saturate01 ((level + 66.0f) / 10.0f);  // near-silent bands never count
            over[i] = x * content;
            excess[i] = std::max (0.0f, power[i] - basePower[i]);
            excursions[i] = excursion;

            // The baseline follows the programme, and all but stops while the band is flagged
            if (a.fullShortDb > -75.0f)
                basePower[i] += (x > 3.0f ? kBaseFlagged : kBase) * (power[i] - basePower[i]);

            blockMax[i] = std::max (blockMax[i], excursion);
            view.excursionDb[i] = excursion;
            view.normalDb[i] = normalExcess[i];
        }

        // Leakage: a band whose excess energy is mostly explained by bands an octave or more away
        // (through its band-pass skirt) is not itself excessive.
        int withContent = 0, flagged = 0;
        for (int k = 0; k < n; ++k)
        {
            const auto i = (size_t) k;
            if (over[i] > (flaggedNow[i] ? 0.0f : 3.0f))
            {
                float leaked = 0.0f;
                for (int j = 0; j < n; ++j)
                    if (std::abs (j - k) >= 3)
                        leaked += excess[(size_t) j] * leak[i][(size_t) j];
                if (leaked > 0.5f * excess[i])
                    over[i] = std::min (over[i], 0.0f);
            }

            // Hysteresis: a band enters 3 dB beyond its normal and stays flagged until it is back to it
            flaggedNow[i] = over[i] > (flaggedNow[i] ? 0.0f : 3.0f);
            if (! flaggedNow[i])
                over[i] = std::min (over[i], 0.0f);

            blockFlagged[i] = blockFlagged[i] || flaggedNow[i];
            view.overDb[i] = over[i];
            withContent += a.shortDb[i] > -60.0f ? 1 : 0;
            flagged += flaggedNow[i] ? 1 : 0;
        }

        // Every 50 ms: file this block into the history and re-read what is normal. The newest 1.5 s is
        // left out, and a flagged block is filed only a little above what was allowed, so an event
        // cannot teach the detector that it is normal while it lasts (repeats creep in slowly).
        if (++blockTick >= blockTicks)
        {
            blockTick = 0;
            const bool settled = heardSeconds > 1.0f;   // excursions while the baseline settles are not evidence
            for (int k = 0; k < n && settled; ++k)
            {
                const auto i = (size_t) k;
                const bool learning = heardSeconds < 5.0f;   // the first seconds teach it the programme
                const float v = blockFlagged[i] && ! learning ? std::min (blockMax[i], normalExcess[i] + 4.0f) : blockMax[i];
                history[i][(size_t) ringHead] = std::clamp (v, -40.0f, 40.0f);
                blockMax[i] = -100.0f;
                blockFlagged[i] = false;
            }
            if (settled)
            {
                ringHead = (ringHead + 1) % ringLength;
                ringFilled = std::min (ringFilled + 1, ringLength);
            }
            blockMax.fill (-100.0f);
            blockFlagged.fill (false);

            // Only what the programme has actually filed counts (the prior stands in until there is any).
            // While learning, the newest blocks count too; after that, the newest 1.5 s is left out.
            const bool learning = heardSeconds < 5.0f;
            const int skip = learning ? 0 : blockDelay;
            const int count = std::min (blockHistory, ringFilled - skip);
            if (count >= 8)
            {
                const int rank = (count * 97) / 100;
                for (int k = 0; k < n; ++k)
                {
                    const auto& h = history[(size_t) k];
                    for (int b = 0; b < count; ++b)   // walking back from the newest block that counts
                        scratch[(size_t) b] = h[(size_t) ((ringHead - 1 - skip - b + 2 * ringLength) % ringLength)];
                    std::nth_element (scratch.begin(), scratch.begin() + rank, scratch.begin() + count);
                    normalExcess[(size_t) k] = std::clamp (scratch[(size_t) rank], 3.0f, 30.0f);
                    normalGain[(size_t) k] = std::pow (10.0f, 0.1f * normalExcess[(size_t) k]);
                }
            }
        }

        // Nothing is judged until the baseline has had a couple of seconds of programme
        const float warm = saturate01 ((heardSeconds - 1.0f) / 0.7f);

        // --- 2. localised or broadband? ---------------------------------------------------------
        // How many bands the energy beyond normal is spread over (participation ratio): a bass hit puts
        // it in a handful of bands - its leakage into the others carries almost no energy - while
        // a broadband event spreads it over ten or more at once.
        double sumE = 0.0, sumE2 = 0.0;
        for (int k = 0; k < n; ++k)
        {
            const auto i = (size_t) k;
            const double e = std::max (0.0, (double) power[i] - (double) basePower[i] * (double) normalGain[i]);
            sumE += e;
            sumE2 += e * e;
        }
        const float spread = sumE2 > 1.0e-30 ? (float) (sumE * sumE / sumE2) : 0.0f;
        const float t = saturate01 ((spread - 4.0f) / 5.0f);   // up to ~4 bands: localised; ~9 or more: broadband
        (void) withContent;
        Decision d;
        d.offset = offset;

        // Confirmation: a region counts as localised once it has looked that way for ~3 ms; "broadband"
        // is immediate. (The first ticks of a broadband onset reach the high bands before the low ones;
        // what that lets through is let go at once in controlStep, not at the RELEASE speed.)
        localHistory[(size_t) localHead] = 1.0f - t * t * (3.0f - 2.0f * t);
        localHead = (localHead + 1) % localTicks;
        d.localised = *std::min_element (localHistory.begin(), localHistory.begin() + localTicks);
        d.abnormal = flagged > 0 && warm > 0.0f;
        eventHoldS = d.abnormal && d.localised > 0.5f ? 0.5f : std::max (0.0f, eventHoldS - dt);   // (after d.abnormal is known)

        // Soft knee: gentle for the first 4 dB beyond normal, full treatment above that
        for (int k = 0; k < n; ++k)
        {
            auto& o = over[(size_t) k];
            const float kt = saturate01 (o / 4.0f);
            o = o > 0.0f ? o * kt * kt * (3.0f - 2.0f * kt) : 0.0f;
        }

        // --- 3. the offending regions: runs of excessive bands (a one-band gap does not split one)
        struct Region { int lo, hi; float peak, weight, centre, share, excursion; };
        std::array<Region, numBands> regions {};
        int numRegions = 0;

        for (int k = 0; k < n;)
        {
            if (over[(size_t) k] <= 0.0f) { ++k; continue; }

            Region r { k, k, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
            int j = k;
            while (j < n && (over[(size_t) j] > 0.0f || (j + 1 < n && over[(size_t) j + 1] > 0.0f && j > k)))
            {
                const float o = std::max (0.0f, over[(size_t) j]);
                r.hi = j;
                r.peak = std::max (r.peak, o);
                r.weight += o;
                r.centre += o * bandOctave (j);
                r.share += power[(size_t) j];
                r.excursion = std::max (r.excursion, over[(size_t) j] > 0.0f ? excursions[(size_t) j] : 0.0f);
                ++j;
            }
            r.centre /= std::max (1.0e-6f, r.weight);
            r.share /= total;
            regions[(size_t) numRegions++] = r;
            k = j;
        }

        // Strongest first
        std::sort (regions.begin(), regions.begin() + numRegions, [] (const Region& x, const Region& y) { return x.peak > y.peak; });

        // --- 4. give each region a slot, following it if it moves -----------------------------
        std::array<bool, numSlots> used {};
        for (int r = 0; r < std::min (numRegions, numSlots * 2); ++r)
        {
            const auto& reg = regions[(size_t) r];
            Target target;
            if (reg.lo == 0)
            {
                target.shape = Shape::lowShelf;                     // the excess runs off the bottom
                target.octave = bandOctave (reg.hi) + 0.5f * bandSpacingOct;
            }
            else if (reg.hi == n - 1)
            {
                target.shape = Shape::highShelf;                    // ... or off the top
                target.octave = bandOctave (reg.lo) - 0.5f * bandSpacingOct;
            }
            else
            {
                target.shape = Shape::bell;
                target.octave = reg.centre;
            }
            target.width = (float) (reg.hi - reg.lo + 1) * bandSpacingOct;
            target.depthDb = reg.peak * warm;
            target.share = reg.share;
            target.excursionDb = reg.excursion * warm;

            // The nearest slot already following something of this shape, else the quietest free one
            int best = -1;
            float bestDistance = 1.5f;
            for (int s = 0; s < numSlots; ++s)
            {
                const bool following = assigned[(size_t) s].depthDb > 0.0f || slots[(size_t) s].depthDb > 0.3f;
                const float dist = std::abs (assigned[(size_t) s].octave - target.octave);
                if (! used[(size_t) s] && following && dist < bestDistance)
                {
                    best = s;
                    bestDistance = dist;
                }
            }
            if (best < 0)
                for (int s = 0; s < numSlots; ++s)
                    if (! used[(size_t) s] && slots[(size_t) s].depthDb < 0.5f && assigned[(size_t) s].depthDb <= 0.0f
                        && (best < 0 || slots[(size_t) s].depthDb < slots[(size_t) best].depthDb))
                        best = s;
            if (best < 0)
                continue;   // more regions than slots: the strongest three are handled

            used[(size_t) best] = true;
            // A slot that is still cutting keeps its shape; the region's new extent only moves it
            if (slots[(size_t) best].depthDb + keyDb[(size_t) best] > 0.3f && slots[(size_t) best].shape != target.shape)
            {
                target.shape = slots[(size_t) best].shape;
                if (target.shape == Shape::bell)
                    target.octave = reg.centre;
            }
            assigned[(size_t) best] = target;
        }

        // Slots with nothing to follow any more let go where they are
        for (int s = 0; s < numSlots; ++s)
            if (! used[(size_t) s])
                assigned[(size_t) s].depthDb = 0.0f;

        d.targets = assigned;

        if (pending < (int) schedule.size())
            schedule[(size_t) pending++] = d;
        else
            schedule.back() = d;
    }

    //==============================================================================
    void SpectralLimiter::designCut (Coeffs& c, Shape shape, float hz, float octaves, float depthDb, double sampleRate) noexcept
    {
        const float A = std::pow (10.0f, -depthDb / 40.0f);
        const float fc = std::clamp (hz, 20.0f, 0.45f * (float) sampleRate);
        const float w = std::tan ((float) pi * fc / (float) sampleRate);

        float g = w, k = 1.0f;
        switch (shape)
        {
            case Shape::bell:
                k = 1.0f / (qForOctaves (octaves) * A);
                c.m0 = 1.0f; c.m1 = k * (A * A - 1.0f); c.m2 = 0.0f;
                break;
            case Shape::lowShelf:
                g = w / std::sqrt (A);
                k = 1.4142136f;
                c.m0 = 1.0f; c.m1 = k * (A - 1.0f); c.m2 = A * A - 1.0f;
                break;
            case Shape::highShelf:
                g = w * std::sqrt (A);
                k = 1.4142136f;
                c.m0 = A * A; c.m1 = k * (1.0f - A) * A; c.m2 = 1.0f - A * A;
                break;
        }

        c.a1 = 1.0f / (1.0f + g * (g + k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
    }

    void SpectralLimiter::design (int s) noexcept
    {
        const auto i = (size_t) s;
        const auto& sl = slots[i];
        const float audio = sl.depthDb + extraDb[i], key = audio + keyDb[i];
        designCut (coeffs[i], sl.shape, sl.hz, sl.octaves, audio, sr);
        designCut (keyCoeffs[i], sl.shape, sl.hz, sl.octaves, keyDb[i], sr);

        // An idle filter is the identity: skip it, and let it start from rest when it is next needed
        auto liven = [] (bool& live, bool needed, auto& bank, size_t slot)
        {
            if (needed && ! live)
                for (auto& ch : bank)
                    ch[slot].reset();
            live = needed;
        };
        liven (audioLive[i], audio > 1.0e-3f, svf, i);
        liven (keyLive[i], key > 1.0e-3f && keyDb[i] > 1.0e-3f, keySvf, i);
    }

    void SpectralLimiter::controlStep (const Settings& s) noexcept
    {
        const float dt = stepDt;
        const float range = std::clamp (s.rangeDb, 0.0f, 18.0f);
        const float releaseS = std::clamp (s.releaseMs, 20.0f, 2000.0f) * 0.001f;
        const float attackS = std::clamp (releaseS / 30.0f, 0.0015f, 0.008f);
        const float kAttack = 1.0f - std::exp (-dt / attackS), kRelease = 1.0f - std::exp (-dt / releaseS);
        const float kGlide = 1.0f - std::exp (-dt / 0.030f);
        const float kDrop = 1.0f - std::exp (-dt / 0.008f);   // a cut that turned out to be for a broadband event

        // Toward "broadband" at once, back toward "localised" over ~20 ms
        localised = current.localised < localised ? current.localised
                                                  : localised + (current.localised - localised) * (1.0f - std::exp (-dt / 0.020f));

        // 1. The spectral cut: the excess beyond normal, confined to its region, and faded out as the
        //    event becomes broadband (that is not this unit's job).
        std::array<float, numSlots> want {}, wantKey {};
        float deepest = 0.0f, share = 0.0f;
        for (int i = 0; i < numSlots; ++i)
        {
            const auto& t = current.targets[(size_t) i];
            want[(size_t) i] = std::min (range, 0.9f * t.depthDb) * localised;
            if (t.depthDb > 0.0f)
                share += t.share;
            deepest = std::max (deepest, want[(size_t) i]);
        }
        share = std::min (share, 0.98f);

        // 2. Headroom: an abnormal event that also pushes this stage past CEILING gets the region cut
        //    deeper. First estimate: what that region's share of the energy says is needed. Then
        //    feedback from this unit's own output, which also sees what the stages before it added
        //    (the leveler's lift): while the output is still over CEILING, the cut keeps deepening.
        //    Up to 12 dB beyond RANGE in all.
        const float peakDb = 20.0f * std::log10 (std::max (1.0e-6f, peakEnv));
        const float overDb = current.abnormal ? peakDb - s.ceilingDb : 0.0f;
        const float outOverDb = current.abnormal && localised > 0.3f ? 20.0f * std::log10 (std::max (1.0e-6f, peakOutEnv)) - s.ceilingDb : -1.0f;
        headroomFeedback = outOverDb > 0.0f ? std::min (12.0f, headroomFeedback + 0.5f * outOverDb)
                                            : headroomFeedback - headroomFeedback * kRelease;
        std::array<float, numSlots> protect {};
        float bbTarget = 0.0f;

        if (overDb > 0.0f)
        {
            float achievedDb = 0.0f;
            if (share > 0.02f)
            {
                const float g = (std::pow (10.0f, -overDb / 10.0f) - (1.0f - share)) / share;
                const float needed = g > 1.0e-3f ? -10.0f * std::log10 (g) : 30.0f;
                const float extra = std::clamp (needed - deepest + headroomFeedback, 0.0f, 12.0f);
                for (int i = 0; i < numSlots; ++i)
                    if (current.targets[(size_t) i].depthDb > 0.0f)
                        protect[(size_t) i] = extra * localised;
                const float cut = deepest + extra * localised;
                achievedDb = -10.0f * std::log10 ((1.0f - share) + share * std::pow (10.0f, -cut / 10.0f));
            }

            // 3. Broadband, only for what is left and only as far as the event really is broadband
            bbTarget = std::max (0.0f, overDb - achievedDb) * (1.0f - localised);
        }

        // The compressor's key: the flagged region taken down to its baseline, less what the audio cut
        // already took out - so the compressor reacts to the rest of the programme, not to this event.
        for (int i = 0; i < numSlots; ++i)
        {
            const auto& t = current.targets[(size_t) i];
            wantKey[(size_t) i] = t.depthDb > 0.0f ? std::max (0.0f, std::min (30.0f, t.excursionDb) * localised - want[(size_t) i] - protect[(size_t) i]) : 0.0f;
        }

        for (int i = 0; i < numSlots; ++i)
        {
            auto& sl = slots[(size_t) i];
            const auto& t = current.targets[(size_t) i];
            const float before = sl.depthDb, extraBefore = extraDb[(size_t) i], keyBefore = keyDb[(size_t) i];
            const float hz0 = sl.hz, oct0 = sl.octaves;

            // Released at the RELEASE speed when the event is over, but at once when it is still going
            // and has turned out to be broadband: that is not this unit's to cut.
            const float kDown = localised < 0.5f ? kDrop : kRelease;
            auto follow = [&] (float& v, float target) { v += (target > v ? kAttack : kDown) * (target - v); };
            follow (sl.depthDb, want[(size_t) i]);
            follow (extraDb[(size_t) i], protect[(size_t) i]);
            follow (keyDb[(size_t) i], wantKey[(size_t) i]);

            // Follow the region: glide while cutting, jump while idle (nothing audible moves)
            const float targetHz = 1000.0f * std::pow (2.0f, t.octave);
            if (t.depthDb > 0.0f)
            {
                if (sl.depthDb + extraDb[(size_t) i] + keyDb[(size_t) i] < 0.3f)
                {
                    sl.shape = t.shape;
                    sl.hz = targetHz;
                    sl.octaves = t.width;
                }
                else if (sl.shape == t.shape)
                {
                    sl.hz *= std::pow (targetHz / sl.hz, kGlide);
                    sl.octaves += (t.width - sl.octaves) * kGlide;
                }
            }

            if (std::abs (sl.depthDb - before) > 1.0e-4f || std::abs (extraDb[(size_t) i] - extraBefore) > 1.0e-4f
                || std::abs (keyDb[(size_t) i] - keyBefore) > 1.0e-4f || sl.hz != hz0 || sl.octaves != oct0)
                design (i);
        }

        broadbandDb += (bbTarget > broadbandDb ? 1.0f - std::exp (-dt / 0.001f) : kRelease) * (bbTarget - broadbandDb);
    }

    void SpectralLimiter::process (float* const* data, int numChannels, int numSamples, const Settings& s) noexcept
    {
        const int ch = std::min (2, numChannels);

        if (! s.active || ch <= 0)
        {
            // True bypass; the detector keeps learning, the cuts are gone when it comes back in
            if (slots[0].depthDb + slots[1].depthDb + slots[2].depthDb + keyDb[0] + keyDb[1] + keyDb[2] + broadbandDb > 0.0f)
            {
                for (auto& sl : slots) sl.depthDb = 0.0f;
                extraDb = {};
                keyDb = {};
                for (int i = 0; i < numSlots; ++i) design (i);
                for (auto* bank : { &svf, &keySvf })
                    for (auto& c : *bank)
                        for (auto& f : c)
                            f.reset();
                broadbandDb = 0.0f;
                broadbandGain = 1.0f;
            }
            if (pending > 0)
                current = schedule[(size_t) pending - 1];
            pending = 0;
            return;
        }

        const float peakRelease = std::exp (-1.0f / (0.060f * (float) sr));
        const float gainGlide = 1.0f - std::exp (-1.0f / (0.0006f * (float) sr));
        const int keyCapacity = (int) (keyBuffer.size() / 2);
        int next = 0;
        float outPeak = 0.0f;   // this unit's output, one sample behind (feeds the headroom feedback)

        auto tick = [] (const Coeffs& q, Svf& f, float x) noexcept
        {
            const float v3 = x - f.ic2;
            const float v1 = q.a1 * f.ic1 + q.a2 * v3;
            const float v2 = f.ic2 + q.a2 * f.ic1 + q.a3 * v3;
            f.ic1 = 2.0f * v1 - f.ic1;
            f.ic2 = 2.0f * v2 - f.ic2;
            return q.m0 * x + q.m1 * v1 + q.m2 * v2;
        };

        for (int i = 0; i < numSamples; ++i)
        {
            while (next < pending && schedule[(size_t) next].offset <= i)
                current = schedule[(size_t) next++];

            float peak = 0.0f;
            for (int c = 0; c < ch; ++c)
                peak = std::max (peak, std::abs (data[c][i]));
            peakEnv = peak > peakEnv ? peak : peakEnv * peakRelease;
            peakOutEnv = outPeak > peakOutEnv ? outPeak : peakOutEnv * peakRelease;
            outPeak = 0.0f;

            if (--stepCountdown <= 0)
            {
                stepCountdown = interval;
                controlStep (s);
            }

            broadbandGain += (dbToGain (-broadbandDb) - broadbandGain) * gainGlide;

            for (int c = 0; c < 2; ++c)
            {
                const int from = std::min (c, ch - 1);
                float y = data[from][i];
                for (int k = 0; k < numSlots; ++k)
                    if (audioLive[(size_t) k])
                        y = tick (coeffs[(size_t) k], svf[(size_t) c][(size_t) k], y);
                y *= broadbandGain;

                if (c < ch)
                    data[c][i] = y;
                outPeak = std::max (outPeak, std::abs (y));

                if (i < keyCapacity)
                {
                    float key = y;
                    for (int k = 0; k < numSlots; ++k)
                        if (keyLive[(size_t) k])
                            key = tick (keyCoeffs[(size_t) k], keySvf[(size_t) c][(size_t) k], key);
                    keyPointers[(size_t) c][i] = key;
                }

                if (ch == 1)
                {
                    if (i < keyCapacity) keyPointers[1][i] = keyPointers[0][i];
                    break;
                }
            }
        }

        while (next < pending)
            current = schedule[(size_t) next++];
        pending = 0;
    }

    //==============================================================================
    float SpectralLimiter::responseDb (const std::array<Slot, numSlots>& cuts, float hz) noexcept
    {
        float db = 0.0f;
        for (auto& sl : cuts)
        {
            if (sl.depthDb < 0.01f)
                continue;

            const double A = std::pow (10.0, -sl.depthDb / 40.0);
            const std::complex<double> s (0.0, hz / std::max (1.0f, sl.hz));
            std::complex<double> h;
            if (sl.shape == Shape::bell)
            {
                const double q = qForOctaves (sl.octaves);
                h = (s * s + s * (A / q) + 1.0) / (s * s + s / (A * q) + 1.0);
            }
            else
            {
                const double q = 0.70710678, sA = std::sqrt (A);
                h = sl.shape == Shape::lowShelf ? A * (s * s + (sA / q) * s + A) / (A * s * s + (sA / q) * s + 1.0)
                                                : A * (A * s * s + (sA / q) * s + 1.0) / (s * s + (sA / q) * s + A);
            }
            db += (float) (20.0 * std::log10 (std::max (1.0e-9, std::abs (h))));
        }
        return db;
    }
}
