#include "FootstepDetector.h"

namespace enh::dsp
{
    namespace
    {
        constexpr double confirmTime  = 0.004;   // provisional confidence
        constexpr double decisionTime = 0.042;   // full decision (FFT windows now cover the event)
        constexpr double secondLookTime = 0.070; // decay-limited events are judged again here
        constexpr double sustainTime  = 0.110;   // must have decayed by here
        constexpr double watchEnd     = 0.160;   // end of the retraction watch -> learn the step
        constexpr double refractory   = 0.070;   // heel + toe merge into one event
        constexpr double minStepGap   = 0.200;   // onsets closer than this are clutter, not walking
    }

    void FootstepDetector::prepare (const BandAnalyzer& analyzer, double)
    {
        activeCount = analyzer.getActiveCount();

        for (int k = 0; k < numBands; ++k)
        {
            const auto i = (size_t) k;
            const double hz = BandAnalyzer::centreHz (k);
            const bool on = analyzer.isActive (k);
            voiceBand[i] = on && hz >= 450.0 && hz <= 1100.0;
            lowBand[i]   = on && hz >= 60.0 && hz <= 250.0;
            highBand[i]  = on && hz >= 2600.0 && hz <= 7000.0;

            // A preference, not a requirement: game footstep detail mostly sits around 1-4 kHz
            emphasis[i] = on ? 0.7f + 0.3f * octaveBell (hz, 2000.0, 1.3) : 0.0f;
        }

        reset();
    }

    void FootstepDetector::reset()
    {
        background.fill (-90.0f);
        onsetBand.fill (0.0f);
        onsetPattern.fill (0.0f);
        preBackground.fill (-90.0f);
        peakDb.fill (-120.0f);
        dynamicWeight.fill (0.0f);
        competitorWeight.fill (0.0f);
        fingerprint.fill (0.0f);

        phase = Phase::idle;
        clock = 0.0;
        onsetTime = lastOnsetTime = lastStepTime = -10.0;
        lastInterval = 0.0;
        strengthMax = hotMax = broadbandMax = midMax = risingTonalMax = 0.0f;
        contextTonal = clutterAtOnset = sequence = expected = eventScore = lastDecay = 0.0f;
        decided = false;
        armed = true;
        strengthLow = 0.0f;
        secondLook = false;
        othersAtDecision = 0.0f;
        learnedDrop = 10.0f;
        clutterCount = rhythm = hotHold = fingerprintStrength = suspicion = suspicionAtDecision = 0.0f;
        confidence = confidenceTarget = 0.0f;
        eventCount = rejectedCount = 0;
        trace = {};
    }

    //==============================================================================
    void FootstepDetector::computeShape (std::array<float, numBands>& shape) const noexcept
    {
        std::array<float, numBands> raw {};
        for (int k = 0; k < activeCount; ++k)
            raw[(size_t) k] = saturate01 ((peakDb[(size_t) k] - preBackground[(size_t) k] - 3.0f) / 12.0f);

        float peak = 1.0e-3f;
        for (int k = 0; k < numBands; ++k)
        {
            const float l = k > 0 ? raw[(size_t) (k - 1)] : raw[(size_t) k];
            const float r = k + 1 < activeCount ? raw[(size_t) (k + 1)] : raw[(size_t) k];
            shape[(size_t) k] = k < activeCount ? (0.25f * l + 0.5f * raw[(size_t) k] + 0.25f * r) * emphasis[(size_t) k] : 0.0f;
            peak = std::max (peak, shape[(size_t) k]);
        }

        for (auto& s : shape)
            s = std::min (1.0f, s / peak);
    }

    float FootstepDetector::fingerprintMatch() const noexcept
    {
        if (fingerprintStrength < 0.05f)
            return 0.0f;

        std::array<float, numBands> shape {};
        computeShape (shape);

        float dot = 0, a = 0, b = 0;
        for (int k = 0; k < activeCount; ++k)
        {
            dot += shape[(size_t) k] * fingerprint[(size_t) k];
            a += shape[(size_t) k] * shape[(size_t) k];
            b += fingerprint[(size_t) k] * fingerprint[(size_t) k];
        }

        const float cosine = a > 0 && b > 0 ? dot / std::sqrt (a * b) : 0.0f;
        return saturate01 ((cosine - 0.8f) / 0.15f) * fingerprintStrength;
    }

    //==============================================================================
    void FootstepDetector::startEvent (const BandAnalyzer& a, SpectralAnalyzer& spectrum) noexcept
    {
        const double sinceOnset = clock - lastOnsetTime;

        // Clutter: a leaky count of recent onsets, plus a penalty for onsets too close together to be steps
        // Which bands this onset rises in, compared with the previous onset
        float dot = 0, e1 = 0, e2 = 0;
        for (int k = 0; k < activeCount; ++k)
        {
            dot += onsetBand[(size_t) k] * onsetPattern[(size_t) k];
            e1 += onsetBand[(size_t) k] * onsetBand[(size_t) k];
            e2 += onsetPattern[(size_t) k] * onsetPattern[(size_t) k];
        }
        const float similarity = e1 > 0 && e2 > 0 ? dot / std::sqrt (e1 * e2) : 0.0f;
        onsetPattern = onsetBand;

        clutterAtOnset = saturate01 ((clutterCount - 1.6f) / 2.0f);
        if (sinceOnset < minStepGap && similarity > 0.45f && lastDecay > 0.5f)
        {
            // The same kind of sound again, too soon to be walking: a rattle / rummage,
            // and so was the previous event
            clutterAtOnset = 1.0f;
            suspicion = 1.0f;
            if (phase == Phase::provisional || phase == Phase::accepted)
                retract();
        }
        else if (phase == Phase::accepted)
        {
            // A different sound on top of a step that already passed its checks: keep the step
            commit();
        }

        lastDecay = 0.0f;
        lastOnsetTime = clock;
        onsetTime = clock;
        phase = Phase::provisional;
        decided = false;
        secondLook = false;
        decayScale = 1.0f;

        preBackground = background;
        for (int k = 0; k < activeCount; ++k)
            peakDb[(size_t) k] = a.transientDb[(size_t) k];

        spectrum.captureReference();
        contextTonal = spectrum.getTonalityHold();
        strengthMax = hotMax = broadbandMax = midMax = risingTonalMax = 0.0f;
        eventScore = 0.0f;
        eventPeakEnergy = 0.0f;
        peakAt = 0.0;
    }

    void FootstepDetector::retract() noexcept
    {
        phase = Phase::rejected;
        confidenceTarget = 0.0f;
        ++rejectedCount;
    }

    void FootstepDetector::commit() noexcept
    {
        const double interval = onsetTime - lastStepTime;

        if (interval > minStepGap && interval < 0.95)
        {
            const bool steady = lastInterval > 0.0 && std::abs (interval - lastInterval) < 0.25 * lastInterval;
            rhythm = steady ? std::min (1.0f, rhythm + 0.35f) : rhythm * 0.7f;
            lastInterval = interval;
        }
        else
        {
            lastInterval = 0.0;
        }

        lastStepTime = onsetTime;

        std::array<float, numBands> shape {};
        computeShape (shape);
        const float k = fingerprintStrength < 0.05f ? 1.0f : 0.35f;
        for (int b = 0; b < numBands; ++b)
            fingerprint[(size_t) b] += (shape[(size_t) b] - fingerprint[(size_t) b]) * k;
        fingerprintStrength = std::min (1.0f, fingerprintStrength + 0.4f);

        ++eventCount;
        phase = Phase::idle;
    }

    float FootstepDetector::eventDropDb (const BandAnalyzer& a, bool useShortTerm) const noexcept
    {
        // How far the event has fallen from its peak, per band, weighted towards the bands the event
        // dominates most. Sounds that overlap it (speech, ambience) rise in their own bands but barely
        // touch the bands where the event stood far above its background.
        double sum = 0.0, weight = 0.0;
        for (int k = 0; k < activeCount; ++k)
        {
            const auto i = (size_t) k;
            const float excess = peakDb[i] - preBackground[i];
            if (excess <= 6.0f)
                continue;

            const float now = std::max (useShortTerm ? a.shortDb[i] : a.transientDb[i], preBackground[i]);
            const double w = (double) excess * excess;
            sum += w * std::clamp (peakDb[i] - now, 0.0f, excess);
            weight += w;
        }

        return weight > 0.0 ? (float) (sum / weight) : 0.0f;
    }

    float FootstepDetector::eventExcessDb() const noexcept
    {
        double sum = 0.0, weight = 0.0;
        for (int k = 0; k < activeCount; ++k)
        {
            const float excess = peakDb[(size_t) k] - preBackground[(size_t) k];
            if (excess <= 6.0f)
                continue;
            sum += (double) excess * excess * excess;
            weight += (double) excess * excess;
        }
        return weight > 0.0 ? (float) (sum / weight) : 0.0f;
    }

    void FootstepDetector::evaluate (const BandAnalyzer& a) noexcept
    {
        const float tonal = saturate01 ((risingTonalMax - 0.12f) / 0.25f);
        const float context = saturate01 ((contextTonal - 0.15f) / 0.3f);

        // Decay, scaled to how this programme's steps decay (0.55 .. 1: never below about half the
        // default requirement). Only outside tonal activity: a syllable inside speech is a short burst
        // that fades too, so there the default requirement stands.
        const bool clearOfVoice = adaptive && context < 0.3f && midMax < 0.25f && eventExcessDb() >= 14.0f && peakAt <= 0.012;
        const float drop = eventDropDb (a, false);
        const float s = clearOfVoice ? std::clamp (learnedDrop / 10.0f, 0.55f, 1.0f) : 1.0f;
        decayScale = s;
        const float decay = saturate01 ((drop - 2.0f * s) / (5.0f * s));

        // Only impulsive events (ones that die away) count as clutter; swelling sounds such as
        // speech syllables or engine noise do not
        lastDecay = decay;
        clutterCount += decay;
        suspicionAtDecision = suspicion;

        const float others = saturate01 (1.5f * strengthMax)
                   * (1.0f - tonal)
                   * (1.0f - 0.8f * hotMax)
                   * (1.0f - 0.6f * broadbandMax)
                   * (1.0f - 0.6f * midMax)
                   * (1.0f - clutterAtOnset)
                   * (1.0f - 0.5f * context)
                   * (1.0f - 0.8f * suspicion);   // just after a rejected ringing / rattling event
        eventScore = others * decay;
        othersAtDecision = others;

        // A step that continues a walking sequence gets the full lift; a lone event a moderate one
        const float final = saturate01 (eventScore * (0.8f + 0.6f * sequence));

        trace.decay = decay;
        trace.tonal = tonal;
        trace.context = context;
        trace.score = final;
        decided = true;

        if (final >= 0.45f)
        {
            phase = Phase::accepted;
            confidenceTarget = final;
            learnedDrop += (std::clamp (drop, 4.0f, 14.0f) - learnedDrop) * 0.15f;
        }
        else if (! secondLook && clearOfVoice && others * (0.8f + 0.6f * sequence) >= 0.45f && tonal < 0.3f && hotMax < 0.5f
                 && clutterAtOnset < 0.3f && decay > 0.05f)
        {
            // Everything but decay says step: look again over a longer window before deciding - with no
            // lift meanwhile (an undecided event must not lift or duck for longer than a normal one)
            secondLook = true;
            decided = false;
            confidenceTarget = 0.0f;
        }
        else
        {
            // Ringing or rattling rejections make the next second suspicious (crates, lids, loot);
            // quiet or loud (gunfire) rejections do not
            if (tonal > 0.5f || clutterAtOnset > 0.5f)
                suspicion = 1.0f;
            retract();
        }
    }

    //==============================================================================
    float FootstepDetector::update (const BandAnalyzer& a, SpectralAnalyzer& spectrum, float dt) noexcept
    {
        clock += dt;
        const double since = clock - onsetTime;
        const bool inEvent = phase != Phase::idle && since < 0.25;

        // --- onsets against each band's own background ------------------------------------
        // Flux is measured on 3-band neighbourhoods: single narrow bands flicker on noise
        float strengthSum = 0.0f;
        double lowT = 0, lowS = 0, highT = 0, highS = 0, voiceT = 0, voiceS = 0, otherT = 0, otherS = 0;

        for (int k = 0; k < activeCount; ++k)
        {
            const auto i = (size_t) k;
            const int lo = std::max (0, k - 1), hi = std::min (activeCount - 1, k + 1);
            float tSum = 0.0f, sSum = 0.0f;
            for (int j = lo; j <= hi; ++j)
            {
                tSum += a.transient[(size_t) j].env;
                sSum += a.shortTerm[(size_t) j].env;
            }

            const float flux = std::max (0.0f, powerToDb (tSum) - powerToDb (sSum));
            const float prominence = a.shortDb[i] - background[i];
            onsetBand[i] = saturate01 ((flux - 2.5f) / 6.0f) * saturate01 ((prominence - 4.0f) / 9.0f);
            strengthSum += onsetBand[i];

            const double t = a.transient[i].env, sh = a.shortTerm[i].env;
            if (lowBand[i])   { lowT += t; lowS += sh; }
            if (highBand[i])  { highT += t; highS += sh; }
            if (voiceBand[i]) { voiceT += t; voiceS += sh; }
            else if (prominence > 3.0f) { otherT += t; otherS += sh; }

            // Background: falls fast, rises slowly, and holds still during an event
            const bool falling = a.shortDb[i] < background[i];
            if (falling || ! inEvent)
                background[i] += (a.shortDb[i] - background[i]) * (1.0f - std::exp (-dt / (falling ? 0.12f : 2.5f)));
        }

        auto groupFlux = [] (double t, double sh) { return std::max (0.0f, powerToDb ((float) t) - powerToDb ((float) sh)); };
        const float lowFlux = groupFlux (lowT, lowS), highFlux = groupFlux (highT, highS);
        const float voiceFlux = groupFlux (voiceT, voiceS), otherFlux = groupFlux (otherT, otherS);

        const float strength = saturate01 (strengthSum / 2.5f);

        hotHold = std::max (saturate01 ((std::max (a.fullShortDb, a.fullTransientDb - 3.0f) + 16.0f) / 8.0f),
                            hotHold * std::exp (-dt / 0.15f));
        const float broadband = saturate01 ((std::min ({ lowFlux, highFlux, voiceFlux }) - 6.0f) / 4.0f);
        const float midDominance = saturate01 ((voiceFlux - std::max ({ otherFlux, lowFlux, highFlux }) - 2.0f) / 4.0f);
        const bool silent = a.fullShortDb < -80.0f;

        clutterCount *= std::exp (-dt / 0.7f);
        suspicion *= std::exp (-dt / 1.0f);
        rhythm *= std::exp (-dt / 2.5f);
        fingerprintStrength *= std::exp (-dt / 8.0f);

        // --- event lifecycle -----------------------------------------------------------------
        // Rising edge only: the onset measure has to fall back before a new event can start
        if (strength < 0.2f || strengthSum - strengthLow > 2.0f)
            armed = true;
        strengthLow = std::min (strengthLow, strengthSum);

        // A clearly different sound starting inside an undecided event (a step under a rising
        // syllable) replaces it instead of being swallowed by the refractory time
        bool replaces = false;
        if (armed && phase == Phase::provisional && ! decided && clock - onsetTime >= 0.008 && clock - onsetTime < refractory)
        {
            float dot = 0, e1 = 0, e2 = 0;
            for (int k = 0; k < activeCount; ++k)
            {
                dot += onsetBand[(size_t) k] * onsetPattern[(size_t) k];
                e1 += onsetBand[(size_t) k] * onsetBand[(size_t) k];
                e2 += onsetPattern[(size_t) k] * onsetPattern[(size_t) k];
            }
            replaces = e1 > 0 && e2 > 0 && dot / std::sqrt (e1 * e2) < 0.45f;
        }

        if (armed && ! silent && strength >= 0.6f && (clock - onsetTime >= refractory || replaces))
        {
            armed = false;
            strengthLow = strengthSum;
            if (replaces)
                lastOnsetTime = -10.0;   // the replaced onset does not count towards clutter
            startEvent (a, spectrum);
        }

        const double t = clock - onsetTime;

        if (phase != Phase::idle)
        {
            if (t < refractory)
            {
                float energy = 0.0f;
                for (int k = 0; k < activeCount; ++k)
                {
                    peakDb[(size_t) k] = std::max (peakDb[(size_t) k], a.transientDb[(size_t) k]);
                    energy += a.transient[(size_t) k].env;
                }
                if (energy > eventPeakEnergy)
                {
                    eventPeakEnergy = energy;
                    peakAt = t;
                }

                strengthMax = std::max (strengthMax, strength);
                hotMax = std::max (hotMax, hotHold);
                broadbandMax = std::max (broadbandMax, broadband);
                midMax = std::max (midMax, midDominance);
            }

            if (t >= 0.025)
                risingTonalMax = std::max (risingTonalMax, spectrum.getRisingTonality());

            if (phase == Phase::provisional)
            {
                // Does this event fit the walking sequence that is going on?
                const double interval = onsetTime - lastStepTime;
                float rhythmFit = 0.0f;
                if (interval > minStepGap && interval < 0.95)
                    rhythmFit = lastInterval > 0.0
                                  ? 1.0f - saturate01 (((float) (std::abs (interval - lastInterval) / lastInterval) - 0.1f) / 0.2f)
                                  : 0.4f;

                const bool walkingGap = interval > minStepGap && interval < 0.95;
                const float soundsAlike = walkingGap ? fingerprintMatch() : 0.0f;
                sequence = std::max (rhythm * rhythmFit, soundsAlike);

                // Before the event can be classified, only an onset that lands on the walking
                // rhythm AND sounds like the recent steps gets more than a slight lift
                expected = rhythm * rhythmFit * (0.5f + 0.5f * soundsAlike);

                if (t >= confirmTime && ! decided && ! secondLook)
                {
                    const float provisional = saturate01 (1.4f * strengthMax)
                                            * (1.0f - 0.8f * hotMax) * (1.0f - 0.7f * broadbandMax) * (1.0f - 0.6f * midMax)
                                            * (1.0f - clutterAtOnset) * (1.0f - 0.8f * suspicion)
                                            * (1.0f - 0.5f * saturate01 ((contextTonal - 0.12f) / 0.25f));

                    // Unknown events only get a slight lift until they are classified
                    confidenceTarget = std::min (0.25f + 0.75f * expected, provisional);
                }

                if (t >= decisionTime && ! secondLook)
                    evaluate (a);
                else if (secondLook && ! decided && t >= secondLookTime)
                {
                    // The longer view: the short-term level against the event's peak, needing a clear fall
                    const float s = std::clamp (learnedDrop / 10.0f, 0.55f, 1.0f);
                    const float decay2 = saturate01 ((eventDropDb (a, true) - 3.0f * s) / (6.0f * s));
                    const float final2 = saturate01 (othersAtDecision * decay2 * (0.8f + 0.6f * sequence));
                    decided = true;
                    trace.decay = decay2;
                    trace.score = final2;
                    if (final2 >= 0.45f && risingTonalMax < 0.30f)
                    {
                        phase = Phase::accepted;
                        confidenceTarget = final2;
                    }
                    else
                        retract();
                }
            }
            else if (phase == Phase::accepted)
            {
                // Ringing, creaking or sustained sounds are not steps: take the decision back
                if (risingTonalMax > 0.37f)
                {
                    suspicion = 1.0f;
                    retract();
                }
                else if (t >= sustainTime && t < sustainTime + 0.01)
                {
                    // Must have fallen well away by now - by as much as this programme's steps do
                    // (a room tail holds a step up; a sustained sound does not fall at all)
                    if (eventDropDb (a, true) < 9.0f * decayScale)
                        retract();
                }

                if (phase == Phase::accepted)
                {
                    if (t > 0.09)
                        confidenceTarget = 0.0f;   // let the lift release over the tail
                    if (t >= watchEnd)
                        commit();
                }
            }
            else if (phase == Phase::rejected && t >= watchEnd)
            {
                phase = Phase::idle;
            }
        }

        // --- confidence: instant rise, slow release for steps, fast drop for rejected events --
        const float release = phase == Phase::rejected || (secondLook && ! decided) ? 0.02f : 0.15f;   // an undecided second look lets go at once too
        confidence = std::max (confidenceTarget, confidence * std::exp (-dt / release));

        // --- where to lift / duck -----------------------------------------------------------
        if (phase == Phase::provisional || phase == Phase::accepted)
        {
            std::array<float, numBands> shape {};
            computeShape (shape);

            // Early in an event the spectrum is still forming: lean on the learned fingerprint
            const float fp = phase == Phase::provisional ? 0.6f * fingerprintStrength : 0.25f * fingerprintStrength;
            float meanBg = 0.0f;
            for (int k = 0; k < activeCount; ++k)
            {
                dynamicWeight[(size_t) k] = shape[(size_t) k] + (fingerprint[(size_t) k] - shape[(size_t) k]) * fp;
                meanBg += preBackground[(size_t) k];
            }
            meanBg /= (float) std::max (1, activeCount);

            for (int j = 0; j < activeCount; ++j)
            {
                float near = 0.0f;
                for (int k = std::max (0, j - 5); k <= std::min (activeCount - 1, j + 5); ++k)
                    near = std::max (near, dynamicWeight[(size_t) k] * std::exp (-(float) ((j - k) * (j - k)) / 12.0f));

                const auto i = (size_t) j;
                competitorWeight[i] = (1.0f - saturate01 (dynamicWeight[i] / 0.25f))
                                    * saturate01 ((preBackground[i] - meanBg - 4.0f) / 8.0f) * near;
            }
        }

        trace.strength = strengthMax;
        trace.clutter = std::max (clutterAtOnset, suspicionAtDecision);
        trace.hot = hotMax;
        trace.broadband = broadbandMax;
        trace.midDominance = midMax;
        trace.sequence = sequence;
        trace.confidence = confidence;
        trace.phase = phase;

        return confidence;
    }
}
