#include "FootstepRadar.h"

#include <cmath>
#include <algorithm>

namespace enh::dsp
{
    namespace
    {
        // How much each band's onset counts: the thump, the click and the scuff carry a step; the
        // low-mids are shared with voices and music; the air band alone is often hiss or cymbals
        constexpr std::array<float, FootstepRadar::numBands> onsetWeight { 1.0f, 0.9f, 0.7f, 1.0f, 1.0f, 0.55f };

        inline float powerDb (float p) noexcept { return 10.0f * std::log10 (p + 1.0e-14f); }

        /** 0 at e0, 1 at e1 (either way round), smooth in between. */
        inline float ramp (float e0, float e1, float x) noexcept
        {
            const float t = saturate01 ((x - e0) / (e1 - e0));
            return t * t * (3.0f - 2.0f * t);
        }

        inline float cosine (const std::array<float, FootstepRadar::numBands>& a, const std::array<float, FootstepRadar::numBands>& b) noexcept
        {
            float ab = 0.0f, aa = 0.0f, bb = 0.0f;
            for (int i = 0; i < FootstepRadar::numBands; ++i)
            {
                ab += a[(size_t) i] * b[(size_t) i];
                aa += a[(size_t) i] * a[(size_t) i];
                bb += b[(size_t) i] * b[(size_t) i];
            }
            return aa > 1.0e-9f && bb > 1.0e-9f ? ab / std::sqrt (aa * bb) : 0.0f;
        }
    }

    //==============================================================================
    void FootstepRadar::prepare (double sampleRate, int maxBlock)
    {
        (void) maxBlock;
        sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        lookahead = std::max (1, (int) std::lround (0.0025 * sr));
        tickInterval = std::max (8, (int) std::lround (sr / 1000.0));
        tickDt = (float) tickInterval / (float) sr;
        pitchDecim = std::max (1, (int) std::lround (sr / 4000.0));

        for (int b = 0; b < numBands; ++b)
            bandCoeffs[(size_t) b] = SvfCoeffs::make (sr, bandHz[(size_t) b], 1.1);

        // Power envelope: up in half a millisecond (the step's attack), down over 12 ms
        fastAtk = onePole (0.0005, sr);
        fastRel = onePole (0.012, sr);
        medK = onePole (0.006, sr);
        shortAtk = onePole (0.0003, sr);
        shortRel = onePole (0.0015, sr);
        glide = 1.0f - onePole (0.0015, sr);   // lift attack; its release is slower (see process)

        for (auto& d : delay)
            d.assign ((size_t) lookahead, 0.0f);

        // The room: lines long enough for the hall and open air; the pre-delay up to 45 ms
        for (auto& l : room.line)
            l.assign ((size_t) std::lround (0.130 * sr) + 1, 0.0f);
        room.pre.assign ((size_t) std::lround (0.045 * sr) + 1, 0.0f);
        room.designedRoom = -1;
        designRoom (0);

        reset();
    }

    void FootstepRadar::reset()
    {
        for (auto& ch : analysis) for (auto& f : ch) f = {};
        for (auto& ch : liftFilters) for (auto& f : ch) f = {};
        fast.fill (0.0f); med.fill (0.0f); shortEnv.fill (0.0f); tickPeak.fill (0.0f); tickShort.fill (0.0f);
        accL.fill (0.0f); accR.fill (0.0f); accX.fill (0.0f);
        shortDb.fill (-140.0f);
        tickInPeak = 0.0f;
        tickSamples = 0;
        tickCountdown = tickInterval;
        clock = 0.0;
        sampleCounter = 0;
        zcPrev.fill (0.0f); zcLast.fill (0); zcPrevInterval.fill (0);
        tickZcSum.fill (0.0); tickZcSq.fill (0.0); tickZcJitter.fill (0.0); tickZcCount.fill (0);
        for (auto& h : levelHistory) h.fill (-140.0f);
        historyPos = 0;
        levelDb.fill (-140.0f);
        background.fill (-140.0f);
        excessNow.fill (0.0f);
        onsetNow = thresholdNow = 0.0f;
        musicality = 0.0f;
        deviation.fill (3.0f);
        for (auto& w : windowMean) w.fill (-140.0f);
        for (auto& w : windowFlicker) w.fill (0.0f);
        windowPower.fill (0.0); windowJitter.fill (0.0f); lastTickDb.fill (-140.0f);
        windowTick = windowPos = windowsFilled = 0;
        winSum.fill (0.0); winSq.fill (0.0); winCount.fill (0);
        winTicks = 0;
        lastOnset = -10.0;
        onsetTimes.fill (-100.0);
        onsetHead = 0;
        for (auto& e : ev) e = Event {};
        newest = -1;
        anyWatching = anyActive = false;
        for (auto& p : preShort) p.fill (-140.0f);
        prePos = 0;
        lastEventTime = -10.0;
        lastEventPrint.fill (0.0f);
        lastEventPan = 0.0f;
        lastEventBody = false;
        lastEventTrack = -1;
        lastEventGrid = 0.0f;
        for (auto& h : heard) h = Heard {};
        heardHead = 0;
        for (auto& f : faint) f = Faint {};
        faintHead = 0;
        for (auto& t : tracks) t = Track {};
        nextTrackId = 1;
        liftTarget.fill (0.0f); lift.fill (0.0f);
        sendTarget = send = 0.0f;
        activity = 0.0f;
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        delayPos = 0;
        for (auto& l : room.line) std::fill (l.begin(), l.end(), 0.0f);
        std::fill (room.pre.begin(), room.pre.end(), 0.0f);
        room.pos.fill (0); room.damp.fill (0.0f); room.prePos = 0;
        accepted = events = stepsTotal = 0;
        recent = {};
    }

    //==============================================================================
    void FootstepRadar::designRoom (int kind) noexcept
    {
        if (kind == room.designedRoom)
            return;
        room.designedRoom = kind;
        // Room: a small space close around the step. Hall: bigger, longer. Open air: a few distant
        // echoes and little tail, as far steps outdoors sound.
        static constexpr std::array<std::array<float, Room::lines>, 3> ms {{ { 17.0f, 23.0f, 29.0f, 37.0f },
                                                                             { 31.0f, 41.0f, 53.0f, 67.0f },
                                                                             { 47.0f, 71.0f, 97.0f, 127.0f } }};
        static constexpr std::array<float, 3> feedback { 0.55f, 0.74f, 0.32f }, damp { 0.35f, 0.25f, 0.55f };
        const int k = std::clamp (kind, 0, 2);
        for (int j = 0; j < Room::lines; ++j)
        {
            room.length[(size_t) j] = std::clamp ((int) std::lround (ms[(size_t) k][(size_t) j] * 0.001 * sr), 1, (int) room.line[(size_t) j].size() - 1);
            room.pos[(size_t) j] %= room.length[(size_t) j];
        }
        room.feedback = feedback[(size_t) k];
        room.dampK = damp[(size_t) k];
    }

    inline void FootstepRadar::roomProcess (float in, float& outL, float& outR) noexcept
    {
        // Pre-delay: the further the step, the later its room arrives
        const int preLen = (int) room.pre.size();
        room.pre[(size_t) room.prePos] = in;
        int readPos = room.prePos - room.preSamples;
        if (readPos < 0) readPos += preLen;
        const float x = room.pre[(size_t) readPos];
        if (++room.prePos >= preLen) room.prePos = 0;

        std::array<float, Room::lines> d {};
        for (int j = 0; j < Room::lines; ++j)
        {
            const float o = room.line[(size_t) j][(size_t) room.pos[(size_t) j]];
            room.damp[(size_t) j] += (1.0f - room.dampK) * (o - room.damp[(size_t) j]);   // air and walls take the top
            d[(size_t) j] = room.damp[(size_t) j];
        }
        // Hadamard: every line feeds every other, energy kept
        const float h0 = 0.5f * (d[0] + d[1] + d[2] + d[3]), h1 = 0.5f * (d[0] - d[1] + d[2] - d[3]);
        const float h2 = 0.5f * (d[0] + d[1] - d[2] - d[3]), h3 = 0.5f * (d[0] - d[1] - d[2] + d[3]);
        const std::array<float, Room::lines> h { h0, h1, h2, h3 };
        for (int j = 0; j < Room::lines; ++j)
        {
            room.line[(size_t) j][(size_t) room.pos[(size_t) j]] = x + room.feedback * h[(size_t) j];
            if (++room.pos[(size_t) j] >= room.length[(size_t) j]) room.pos[(size_t) j] = 0;
        }
        outL = 0.5f * (d[0] + d[2]);
        outR = 0.5f * (d[1] + d[3]);
    }

    //==============================================================================
    void FootstepRadar::process (float* const* ch, int numChannels, int n, const Settings& s, const float* const* key) noexcept
    {
        const int nc = std::min (2, numChannels);
        if (nc <= 0 || n <= 0)
            return;

        designRoom (s.room);
        const float glideDown = 1.0f - onePole (0.050, sr);   // a lift lets go over ~50 ms

        for (int i = 0; i < n; ++i)
        {
            const float xl = ch[0][i], xr = nc > 1 ? ch[1][i] : xl;

            if (s.active)
            {
                // --- analysis of what is arriving (2.5 ms ahead of what is heard) --------------------
                const float al = key != nullptr ? key[0][i] : xl, ar = key != nullptr ? key[nc > 1 ? 1 : 0][i] : xr;
                for (int b = 0; b < numBands; ++b)
                {
                    const auto& c = bandCoeffs[(size_t) b];
                    const float yl = analysis[0][(size_t) b].process (c, al).band;
                    const float yr = nc > 1 ? analysis[1][(size_t) b].process (c, ar).band : yl;
                    const float el = yl * yl, er = yr * yr, e = 0.5f * (el + er);
                    auto& f = fast[(size_t) b];
                    const float k = e > f ? fastAtk : fastRel;
                    f = k * f + (1.0f - k) * e;
                    tickPeak[(size_t) b] = std::max (tickPeak[(size_t) b], f);
                    med[(size_t) b] = medK * med[(size_t) b] + (1.0f - medK) * e;
                    auto& sh = shortEnv[(size_t) b];
                    const float ks = e > sh ? shortAtk : shortRel;
                    sh = ks * sh + (1.0f - ks) * e;
                    tickShort[(size_t) b] = std::max (tickShort[(size_t) b], sh);
                    accL[(size_t) b] += el;
                    accR[(size_t) b] += er;
                    accX[(size_t) b] += yl * yr;

                    if (b < zcBands)
                    {
                        // Zero crossings of the band: regular for a tone, irregular for noise
                        const float m = 0.5f * (yl + yr);
                        if ((m >= 0.0f) != (zcPrev[(size_t) b] >= 0.0f))
                        {
                            const long long interval = sampleCounter - zcLast[(size_t) b];
                            if (zcLast[(size_t) b] > 0 && interval > 0 && interval < (long long) (sr / 20.0))
                            {
                                const double iv = (double) interval;
                                if (anyWatching)
                                {
                                    tickZcSum[(size_t) b] += iv;
                                    tickZcSq[(size_t) b] += iv * iv;
                                    if (zcPrevInterval[(size_t) b] > 0)
                                        tickZcJitter[(size_t) b] += std::abs (iv - (double) zcPrevInterval[(size_t) b]) / iv;
                                    ++tickZcCount[(size_t) b];
                                }
                                else if (! anyActive)
                                {
                                    winSum[(size_t) b] += iv;
                                    winSq[(size_t) b] += iv * iv;
                                    ++winCount[(size_t) b];
                                }
                                zcPrevInterval[(size_t) b] = interval;
                            }
                            zcLast[(size_t) b] = sampleCounter;
                        }
                        zcPrev[(size_t) b] = m;
                    }
                }
                tickInPeak = std::max (tickInPeak, std::max (std::abs (al), std::abs (ar)));
                ++sampleCounter;
                {
                    float low = 0.0f;
                    for (int b = 0; b < 3; ++b)
                        low += 0.5f * (analysis[0][(size_t) b].ic2 + (nc > 1 ? analysis[1][(size_t) b].ic2 : analysis[0][(size_t) b].ic2));
                    pitchAcc += low;
                    if (++pitchCount >= pitchDecim)
                    {
                        pitchBuf[(size_t) pitchPos] = pitchAcc / (float) pitchCount;
                        pitchPos = (pitchPos + 1) % pitchLength;
                        pitchAcc = 0.0f;
                        pitchCount = 0;
                    }
                }

                if (--tickCountdown <= 0)
                {
                    tickCountdown = tickInterval;
                    controlTick (s);
                }
            }
            else if (anyActive || activity > 0.0f)
            {
                for (auto& e : ev)
                    e = Event {};
                newest = -1;
                anyWatching = anyActive = false;
                liftTarget.fill (0.0f);
                sendTarget = 0.0f;
                activity = 0.0f;
            }

            // --- what is heard: the audio 2.5 ms later, plus the step's own bands lifted ------------
            float& dL = delay[0][(size_t) delayPos];
            float& dR = delay[1][(size_t) delayPos];
            const float heardL = dL, heardR = nc > 1 ? dR : dL;
            dL = xl;
            dR = xr;
            if (++delayPos >= lookahead) delayPos = 0;

            float stepL = 0.0f, stepR = 0.0f;
            for (int b = 0; b < numBands; ++b)
            {
                auto& g = lift[(size_t) b];
                const float target = liftTarget[(size_t) b];
                g += (target - g) * (target > g ? glide : glideDown);
                // The filters always run, so a lift starts from the music's own state (no click)
                const float fl = liftFilters[0][(size_t) b].process (bandCoeffs[(size_t) b], heardL).band;
                const float fr = nc > 1 ? liftFilters[1][(size_t) b].process (bandCoeffs[(size_t) b], heardR).band : fl;
                stepL += g * fl;
                stepR += g * fr;
            }
            send += (sendTarget - send) * (sendTarget > send ? glide : glideDown);

            float roomL = 0.0f, roomR = 0.0f;
            roomProcess (0.5f * (stepL + stepR) * send, roomL, roomR);

            const bool soloing = s.active && s.solo;
            ch[0][i] = (soloing ? 0.0f : heardL) + stepL + roomL;
            if (nc > 1)
                ch[1][i] = (soloing ? 0.0f : heardR) + stepR + roomR;
        }
    }

    //==============================================================================
    float FootstepRadar::expectedNow (double t) const noexcept
    {
        float best = 0.0f;
        for (const auto& tr : tracks)
        {
            if (! tr.active || tr.period <= 0.0f || tr.confidence < 0.2f)
                continue;
            const double beats = (t - tr.last) / (double) tr.period;
            if (beats < 0.6 || beats > 2.4)
                continue;
            const float off = (float) std::abs (beats - std::round (beats));
            best = std::max (best, tr.confidence * (1.0f - ramp (0.05f, 0.14f, off)));
        }
        return best;
    }

    float FootstepRadar::trackMatch (const Track& tr, const Event& e, double t, int& expectedK) const noexcept
    {
        expectedK = 1;
        if (! tr.active)
            return 0.0f;

        // Same sound: the print of this event against the walker's
        std::array<float, numBands> print {};
        for (int b = 0; b < numBands; ++b)
            print[(size_t) b] = std::max (0.0f, e.eventPeak[(size_t) b] - e.eventBase[(size_t) b]);
        const float simPrint = ramp (0.55f, 0.93f, cosine (print, tr.print));

        // Same place
        const float simPan = 1.0f - ramp (0.15f, 0.6f, std::abs (e.eventPan - tr.pan));

        // In step: a whole number of the walker's periods after its last step
        const double ioi = t - tr.last;
        float simTime;
        if (ioi < 0.15)
            simTime = 0.1f;
        else if (tr.period > 0.0f)
        {
            const int k = std::clamp ((int) std::lround (ioi / (double) tr.period), 1, 3);
            expectedK = k;
            const float err = (float) std::abs (ioi - k * (double) tr.period) / tr.period;
            simTime = (1.0f - ramp (0.10f, 0.30f, err)) * (k == 1 ? 1.0f : k == 2 ? 0.8f : 0.6f);
        }
        else
            simTime = ioi >= 0.18 && ioi <= 1.2 ? 0.7f : 0.15f;
        if (ioi > 3.0)
            simTime *= 0.3f;   // the walker stopped long ago: a new walk, perhaps someone else

        return simPrint * simPan * simTime;
    }

    //==============================================================================
    void FootstepRadar::controlTick (const Settings& s) noexcept
    {
        const float dt = tickDt;
        clock += (double) dt;

        // --- levels, background, excess ---------------------------------------------------------------
        const float inPeakDb = 20.0f * std::log10 (tickInPeak + 1.0e-9f);
        tickInPeak = 0.0f;
        const bool listening = ! anyActive;
        float onset = 0.0f, loudest = -140.0f;
        for (int b = 0; b < numBands; ++b)
        {
            const float fastL = powerDb (tickPeak[(size_t) b]);
            tickPeak[(size_t) b] = fast[(size_t) b];
            const float l = powerDb (med[(size_t) b]);
            levelDb[(size_t) b] = l;
            shortDb[(size_t) b] = powerDb (tickShort[(size_t) b]);
            tickShort[(size_t) b] = shortEnv[(size_t) b];
            preShort[(size_t) b][(size_t) prePos] = fastL;
            loudest = std::max (loudest, l);

            // The background: what the band carries between events, and how much it flickers. A band
            // of noise measured over 12 ms flickers by several dB (the narrow low bands most), so a band
            // stands out only when it clears its own flicker. Frozen while an event is watched (a step
            // never lifts it), and quick to catch up when far below (the start, after a silence).
            windowPower[(size_t) b] += (double) med[(size_t) b];
            windowJitter[(size_t) b] += std::min (6.0f, std::abs (l - lastTickDb[(size_t) b]));
            lastTickDb[(size_t) b] = l;
            // Until the first windows are in, follow the level down quickly (a start, after silence)
            if (windowsFilled < 4)
                background[(size_t) b] = std::min (background[(size_t) b] < -139.0f ? l : background[(size_t) b] + 0.02f, l);
            const float margin = std::max (1.0f, 1.2f * deviation[(size_t) b]);
            excessNow[(size_t) b] = std::max (0.0f, l - (background[(size_t) b] + margin));

            // Onset: how far the band rose over the last ~6 ms, but no further than it stands over its
            // background (a loud passage getting louder is not a step)
            auto& hist = levelHistory[(size_t) b];
            float before = 1.0e9f;
            for (int k = 1; k < 7; ++k)
                before = std::min (before, hist[(size_t) ((historyPos - k + historyTicks * 4) % historyTicks)]);
            const float rise = fastL - before;
            riseNow[(size_t) b] = std::clamp (std::min (rise, excessNow[(size_t) b]), 0.0f, 40.0f);
            onset += onsetWeight[(size_t) b] * std::clamp (riseNow[(size_t) b] - 1.5f, 0.0f, 24.0f);
            hist[(size_t) historyPos] = fastL;
        }
        historyPos = (historyPos + 1) % historyTicks;
        prePos = (prePos + 1) % preTicks;
        onsetNow = onset;
        if (++windowTick >= bgWindowTicks)
            updateBackground();

        // Music and voices: pitch in the low end. A bass line, chords and voiced speech are periodic
        // there; ambience, wind, rain and footsteps are noise. Judged every 50 ms, followed over ~3 s.
        if (++winTicks >= 50)
        {
            const double rate = sr / (double) pitchDecim;
            const int minLag = std::max (2, (int) (rate / 500.0)), maxLag = std::min (pitchLength / 2, (int) (rate / 50.0));
            const int span = pitchLength - maxLag;
            double energy = 0.0;
            for (int i = 0; i < pitchLength; ++i)
                energy += (double) pitchBuf[(size_t) i] * pitchBuf[(size_t) i];
            float best = 0.0f;
            if (energy > 1.0e-10)
                for (int lag = minLag; lag <= maxLag; ++lag)
                {
                    double xy = 0.0, xx = 0.0, yy = 0.0;
                    for (int i = 0; i < span; ++i)
                    {
                        const float x = pitchBuf[(size_t) ((pitchPos + maxLag + i) % pitchLength)];
                        const float y = pitchBuf[(size_t) ((pitchPos + maxLag + i - lag) % pitchLength)];
                        xy += (double) x * y; xx += (double) x * x; yy += (double) y * y;
                    }
                    if (xx > 0.0 && yy > 0.0)
                        best = std::max (best, (float) (xy / std::sqrt (xx * yy)));
                }
            periodicity = best;
            const float windowMusic = listening && energy > 1.0e-10 ? ramp (0.65f, 0.90f, best) : musicality;
            musicality += (windowMusic - musicality) * (1.0f - std::exp (-0.05f / 3.0f));
            winSum.fill (0.0); winSq.fill (0.0); winCount.fill (0);
            winTicks = 0;
        }

        // --- the threshold: SENSITIVITY, the DETECTION method, and a walker due to step now -----------
        const float sens = std::clamp (s.sensitivity, 0.0f, 10.0f);
        float threshold = 12.0f - 0.9f * sens;                     // 12 (0) .. 3 (10)
        threshold *= s.detection == 1 ? 0.8f : s.detection == 2 ? 1.3f : 1.0f;
        threshold *= 1.0f + 0.3f * musicality;   // (the drum hits themselves are told apart by their grid)
        threshold *= 1.0f - 0.30f * expectedNow (clock) * (1.0f - 0.8f * musicality);
        thresholdNow = threshold;

        // --- walkers fade when they stop ----------------------------------------------------------------
        for (auto& tr : tracks)
        {
            if (! tr.active)
                continue;
            const double since = clock - tr.last;
            if (since > std::max (1.2, 2.5 * (double) tr.period))
                tr.confidence *= std::exp (-dt / 2.0f);
            if (tr.confidence < 0.03f || since > 6.0)
                tr.active = false;
        }

        // --- a new event, the toe of the step being judged, or another sound on top of it ----------------
        const auto isActive = [] (const Event& e) { return e.phase == Phase::watching || e.phase == Phase::lifted; };
        if (windowsFilled >= 4 && loudest > -95.0f && onset > threshold && clock - lastOnset > 0.012)   // (after 200 ms of listening)
        {
            Event* cur = newest >= 0 && isActive (ev[(size_t) newest]) ? &ev[(size_t) newest] : nullptr;
            const auto begin = [&]
            {
                // A free slot, or the oldest event (settled now if it was lifted: it passed its decision)
                int slot = -1;
                for (int k = 0; k < maxEvents && slot < 0; ++k)
                    if (! isActive (ev[(size_t) k]))
                        slot = k;
                if (slot < 0)
                {
                    slot = 0;
                    for (int k = 1; k < maxEvents; ++k)
                        if (ev[(size_t) k].eventStart < ev[(size_t) slot].eventStart)
                            slot = k;
                    auto& old = ev[(size_t) slot];
                    if (old.phase == Phase::lifted && ! old.eventSustained)
                        confirm (old, true);
                }
                newest = slot;
                startEvent (ev[(size_t) slot], s);
            };
            if (cur == nullptr)
            {
                if (clock - lastOnset > 0.035)
                    begin();
            }
            else
            {
                auto& e = *cur;
                const double age = clock - e.eventStart;
                // A much stronger onset just after a weak event: that was not the step, this is - start again
                // on it (before the weak one was confirmed, so it is never counted)
                if (age > 0.012 && age < 0.12 && onset > 2.2f * e.eventOnset && weakSoFar (e))
                    startEvent (e, s);
                else if (clock - lastOnset > 0.035)
                {
                    // The toe after the heel: the same sound again, from the same place, no louder. Anything
                    // else - another walker's step, a step under the syllable that began just before it - is
                    // judged on its own.
                    std::array<float, numBands> print {};
                    float was = 0.0f, now = 0.0f;
                    double tl = 0.0, tr = 0.0;
                    for (int b = 0; b < numBands; ++b)
                    {
                        print[(size_t) b] = std::max (0.0f, e.eventPeak[(size_t) b] - e.eventBase[(size_t) b]);
                        was = std::max (was, print[(size_t) b]);
                        now = std::max (now, riseNow[(size_t) b]);
                        tl += (double) onsetWeight[(size_t) b] * accL[(size_t) b];
                        tr += (double) onsetWeight[(size_t) b] * accR[(size_t) b];
                    }
                    const float panNow = tl + tr > 1.0e-12 ? (float) ((tr - tl) / (tr + tl)) : 0.0f;
                    const float panWas = e.eventL + e.eventR > 1.0e-12 ? (float) ((e.eventR - e.eventL) / (e.eventR + e.eventL)) : panNow;
                    const bool toe = cosine (riseNow, print) > 0.85f && now < was + 6.0f && std::abs (panNow - panWas) < 0.35f;
                    if (toe && age < 0.35)
                    {
                        if (e.phase == Phase::lifted)
                            e.holdS = std::max (e.holdS, 0.09f);
                        lastOnset = clock;
                    }
                    else
                        begin();
                }
            }
        }

        // --- the events being watched ---------------------------------------------------------------------
        for (auto& e : ev)
        {
            if (! isActive (e))
                continue;
            const double age = clock - e.eventStart;
            for (int b = 0; b < numBands; ++b)
            {
                const float fastNow = levelHistory[(size_t) b][(size_t) ((historyPos + historyTicks - 1) % historyTicks)];
                e.eventPeak[(size_t) b] = std::max (e.eventPeak[(size_t) b], levelDb[(size_t) b]);
                if (shortDb[(size_t) b] > e.eventShortPeak[(size_t) b])
                {
                    e.eventShortPeak[(size_t) b] = shortDb[(size_t) b];
                    e.eventPeakTime[(size_t) b] = age;
                }
                if (age <= 0.045 + 0.5 * dt)
                    e.eventShortAt45[(size_t) b] = shortDb[(size_t) b];
                if (age <= 0.05 && shortDb[(size_t) b] >= e.eventShortPeak[(size_t) b] - 10.0f)
                    ++e.eventAboveTicks[(size_t) b];
                if (e.eventTickCount < eventTicks)
                {
                    e.eventShortHistory[(size_t) b][(size_t) e.eventTickCount] = shortDb[(size_t) b];
                    e.eventFastHistory[(size_t) b][(size_t) e.eventTickCount] = fastNow;
                }
                e.eventFastPeak[(size_t) b] = std::max (e.eventFastPeak[(size_t) b], fastNow);
            }
            if (e.phase == Phase::watching)
                for (int b = 0; b < zcBands; ++b)
                {
                    e.zcSum[(size_t) b] += tickZcSum[(size_t) b];
                    e.zcSq[(size_t) b] += tickZcSq[(size_t) b];
                    e.zcJitter[(size_t) b] += tickZcJitter[(size_t) b];
                    e.zcCount[(size_t) b] += tickZcCount[(size_t) b];
                }
            if (age <= 0.025)
            {
                for (int b = 0; b < numBands; ++b)
                {
                    e.eventL += (double) onsetWeight[(size_t) b] * accL[(size_t) b];
                    e.eventR += (double) onsetWeight[(size_t) b] * accR[(size_t) b];
                    e.eventX += (double) onsetWeight[(size_t) b] * accX[(size_t) b];
                }
            }
            e.eventTickCount = std::min (eventTicks, e.eventTickCount + 1);
            e.eventInPeak = std::max (e.eventInPeak, inPeakDb);
            if (age >= 0.06 && age <= 0.18)
            {
                float p = 0.0f;
                for (int b = 0; b < numBands; ++b)
                    p += std::pow (10.0f, 0.1f * levelDb[(size_t) b]);
                e.eventTailSum += p;
                ++e.eventTailTicks;
            }

            if (e.phase == Phase::watching && age >= 0.050)
                decide (e, s);
            if (e.phase == Phase::lifted && age >= 0.13 && ! e.eventSustained)
                confirm (e);
            if (age >= 0.20 && e.phase != Phase::lifted)
                e.phase = Phase::done;
        }
        accL.fill (0.0f); accR.fill (0.0f); accX.fill (0.0f);
        tickZcSum.fill (0.0); tickZcSq.fill (0.0); tickZcJitter.fill (0.0); tickZcCount.fill (0);

        // --- the lift: each step's held while it lasts, then let go; the most any of them asks ----------
        anyWatching = anyActive = false;
        for (auto& e : ev)
        {
            if (e.holdS > 0.0f)
            {
                e.holdS -= dt;
                if (e.holdS <= 0.0f)
                    release (e);
            }
            anyWatching = anyWatching || e.phase == Phase::watching;
            anyActive = anyActive || isActive (e);
        }
        combineLifts();
        float most = 0.0f;
        for (float g : lift)
            most = std::max (most, g);
        activity = saturate01 (most / 1.5f);
    }

    float FootstepRadar::machineGrid (const Event& e, const std::array<float, numBands>& print) const noexcept
    {
        // Events like this one in the last 8 s: is this one a whole number of beats after them, exact to 2 ms
        // (the event clock ticks every millisecond), for a beat between 0.1 and 1 s?
        std::array<double, heardMemory> same {};
        int n = 0;
        for (const auto& h : heard)
            if (h.time > e.eventStart - 8.0 && h.time < e.eventStart - 0.05 && cosine (h.print, print) > 0.9f)
                same[(size_t) n++] = h.time;
        if (n < 4)
            return 0.0f;

        float best = 0.0f;
        for (int a = 0; a < n; ++a)
        {
            const double beat = e.eventStart - same[(size_t) a];   // the beat: back to one of them ...
            if (beat < 0.1 || beat > 1.0)
                continue;
            int onGrid = 0;                                        // ... and the others on its multiples
            for (int k = 0; k < n; ++k)
            {
                const double beats = (e.eventStart - same[(size_t) k]) / beat;
                const double err = std::abs (beats - std::round (beats)) * beat;
                onGrid += err < 0.0021 ? 1 : 0;
            }
            if (onGrid < 4)
                continue;

            // A walker can happen to be regular; a drum machine plays other drums on the same grid (half and
            // quarter beats too), or over pitched music
            int others = 0;
            for (const auto& h : heard)
            {
                if (h.time < e.eventStart - 8.0 || h.time > e.eventStart - 0.05 || cosine (h.print, print) > 0.9f)
                    continue;
                const double quarters = (e.eventStart - h.time) / (0.25 * beat);
                others += std::abs (quarters - std::round (quarters)) * 0.25 * beat < 0.0021 ? 1 : 0;
            }
            const float sure = std::min (1.0f, (float) (onGrid - 3) / 3.0f) * std::max (std::min (1.0f, (float) others / 3.0f), ramp (0.1f, 0.35f, musicality));
            best = std::max (best, sure);
        }
        return best;
    }

    float FootstepRadar::rhythmOf (const Event& e, const std::array<float, numBands>& print) const noexcept
    {
        // Two faint events like this one (the same sound, from the same place) at a steady pace behind it:
        // b at one or two periods before it, a one period before b
        const auto alike = [&] (const Faint& f) { return cosine (f.print, print) > 0.8f && std::abs (f.pan - e.eventPan) < 0.3f; };
        for (const auto& b : faint)
        {
            const double gapB = e.eventStart - b.time;
            if (gapB < 0.25 || gapB > 2.2 || ! alike (b))
                continue;
            for (const auto& a : faint)
            {
                const double period = b.time - a.time;
                if (period < 0.25 || period > 1.1 || ! alike (a))
                    continue;
                const double k = std::round (gapB / period);
                if (k >= 1.0 && k <= 2.0 && std::abs (gapB - k * period) < 0.06 * period)
                    return (float) period;
            }
        }
        return 0.0f;
    }

    void FootstepRadar::combineLifts() noexcept
    {
        liftTarget.fill (0.0f);
        sendTarget = 0.0f;
        for (const auto& e : ev)
        {
            for (int b = 0; b < numBands; ++b)
                liftTarget[(size_t) b] = std::max (liftTarget[(size_t) b], e.liftTarget[(size_t) b]);
            sendTarget = std::max (sendTarget, e.sendTarget);
        }
    }

    float FootstepRadar::pitchStrength (double& energy) const noexcept
    {
        // The strongest normalised autocorrelation of the low end at 50 - 500 Hz over the last ~48 ms
        const double rate = sr / (double) pitchDecim;
        const int minLag = std::max (2, (int) (rate / 500.0)), maxLag = std::min (pitchLength / 2, (int) (rate / 50.0));
        const int span = pitchLength - maxLag;
        energy = 0.0;
        for (int i = 0; i < pitchLength; ++i)
            energy += (double) pitchBuf[(size_t) i] * pitchBuf[(size_t) i];
        float best = 0.0f;
        if (energy <= 1.0e-10)
            return 0.0f;
        // Only past the autocorrelation's first dip below zero: low-passed noise (a thump, rumble) is
        // smooth, so it matches itself closely at short lags without any pitch; a pitch comes back as a
        // peak after that dip
        bool pastDip = false;
        for (int lag = 1; lag <= maxLag; ++lag)
        {
            double xy = 0.0, xx = 0.0, yy = 0.0;
            for (int i = 0; i < span; ++i)
            {
                const float x = pitchBuf[(size_t) ((pitchPos + maxLag + i) % pitchLength)];
                const float y = pitchBuf[(size_t) ((pitchPos + maxLag + i - lag) % pitchLength)];
                xy += (double) x * y; xx += (double) x * x; yy += (double) y * y;
            }
            const float r = xx > 0.0 && yy > 0.0 ? (float) (xy / std::sqrt (xx * yy)) : 0.0f;
            if (! pastDip)
                pastDip = r < 0.0f;
            else if (lag >= minLag)
                best = std::max (best, r);
        }
        return best;
    }

    bool FootstepRadar::weakSoFar (const Event& e) const noexcept
    {
        // The event so far is well under what is arriving now (10 dB in its strongest band)
        float was = 0.0f, now = 0.0f;
        for (int b = 0; b < numBands; ++b)
        {
            was = std::max (was, e.eventPeak[(size_t) b] - e.eventBase[(size_t) b]);
            now = std::max (now, excessNow[(size_t) b]);
        }
        return now > was + 10.0f;
    }

    void FootstepRadar::updateBackground() noexcept
    {
        // Close this 50 ms window: its mean level and how much the level jittered tick to tick
        for (int b = 0; b < numBands; ++b)
        {
            windowMean[(size_t) b][(size_t) windowPos] = powerDb ((float) (windowPower[(size_t) b] / (double) windowTick));
            windowFlicker[(size_t) b][(size_t) windowPos] = windowJitter[(size_t) b] / (float) windowTick;
            windowPower[(size_t) b] = 0.0;
            windowJitter[(size_t) b] = 0.0f;
        }
        windowTick = 0;
        windowPos = (windowPos + 1) % bgWindows;
        windowsFilled = std::min (bgWindows, windowsFilled + 1);
        if (windowsFilled < 2)
            return;

        std::array<float, bgWindows> sorted {}, flicker {};
        for (int b = 0; b < numBands; ++b)
        {
            const int n = windowsFilled;
            std::copy_n (windowMean[(size_t) b].begin(), n, sorted.begin());
            std::sort (sorted.begin(), sorted.begin() + n);
            const float p20 = sorted[(size_t) (n / 5)], p40 = sorted[(size_t) (2 * n / 5)];
            background[(size_t) b] = p20;
            // The flicker of the quiet windows only (median): what the band does when nothing happens
            int m = 0;
            for (int w = 0; w < n; ++w)
                if (windowMean[(size_t) b][(size_t) w] <= p40)
                    flicker[(size_t) m++] = windowFlicker[(size_t) b][(size_t) w];
            if (m > 0)
            {
                std::nth_element (flicker.begin(), flicker.begin() + m / 2, flicker.begin() + m);
                deviation[(size_t) b] = flicker[(size_t) (m / 2)];
            }
        }
    }

    float FootstepRadar::onsetRate() const noexcept
    {
        int n = 0;
        for (double t : onsetTimes)
            n += clock - t < 3.0 ? 1 : 0;
        return (float) n / 3.0f;
    }

    void FootstepRadar::startEvent (Event& e, const Settings& s) noexcept
    {
        onsetTimes[(size_t) onsetHead] = clock;
        onsetHead = (onsetHead + 1) % onsetMemory;
        e.phase = Phase::watching;
        e.eventStart = lastOnset = clock;
        ++events;
        for (int b = 0; b < numBands; ++b)
            e.eventBase[(size_t) b] = background[(size_t) b] + std::max (1.0f, 1.2f * deviation[(size_t) b]);
        e.eventOnset = onsetNow;
        e.eventAboveTicks.fill (0);
        e.eventShortPeak = shortDb;
        e.eventShortAt45 = shortDb;
        e.eventTickCount = 0;
        e.eventFastPeak.fill (-140.0f);
        for (int b = 0; b < numBands; ++b)
            for (int k = 0; k < preTicks; ++k)
                e.eventPre[(size_t) b][(size_t) k] = preShort[(size_t) b][(size_t) ((prePos + k) % preTicks)];
        e.eventPeak = levelDb;
        e.eventPeakTime.fill (0.0);
        e.eventAt45 = levelDb;
        e.eventL = e.eventR = e.eventX = 0.0;
        e.eventInPeak = -140.0f;
        e.eventTailSum = 0.0f;
        e.eventTailTicks = 0;
        e.eventSustained = false;
        e.eventMatch = e.eventBaseScore = e.eventProbability = 0.0f;
        e.eventBestTrack = -1;
        e.eventPan = 0.0f;
        e.eventRear = 0.0f;
        e.zcSum.fill (0.0); e.zcSq.fill (0.0); e.zcCount.fill (0); e.zcJitter.fill (0.0);

        // A walker due now, with a print like this onset's first millisecond: lift from the start
        // (the audio is 2.5 ms behind, so the step's own attack is lifted). Otherwise a small
        // e.provisional lift that the decision at 50 ms completes or withdraws.
        float best = 0.0f, bestConfidence = 0.0f;
        for (int t = 0; t < maxTracks; ++t)
        {
            int k = 1;
            const float m = trackMatch (tracks[(size_t) t], e, clock, k);
            if (m * tracks[(size_t) t].confidence > best * bestConfidence)
            {
                best = m;
                bestConfidence = tracks[(size_t) t].confidence;
                e.eventBestTrack = t;
            }
        }
        e.provisional = std::clamp (0.15f + 0.75f * best * bestConfidence, 0.0f, 0.85f);
        const float guessDistance = e.eventBestTrack >= 0 && best > 0.3f ? tracks[(size_t) e.eventBestTrack].distance
                                                                       : ramp (-22.0f, -60.0f, *std::max_element (levelDb.begin(), levelDb.end()) + 6.0f);
        room.preSamples = std::clamp ((int) std::lround ((0.006 + 0.030 * guessDistance) * sr), 0, (int) room.pre.size() - 1);
        const float quiet = std::clamp ((-(*std::max_element (levelDb.begin(), levelDb.end())) - 4.0f) / 25.0f, 0.25f, 1.0f);
        setLift (e, std::clamp (s.boostDb, 0.0f, 12.0f) * e.provisional * (0.35f + 0.65f * guessDistance) * quiet, 0.0f);
        e.holdS = 0.065f;   // until the decision
    }

    void FootstepRadar::setLift (Event& e, float boostDb, float sendAmount) noexcept
    {
        // The lift goes where this step's energy is: its bands in proportion to how far each rose
        // over its background, with the click and scuff (what makes a step audible and placeable)
        // never left out when they are part of it
        float most = 0.0f;
        std::array<float, numBands> ex {};
        for (int b = 0; b < numBands; ++b)
        {
            ex[(size_t) b] = std::max (0.0f, std::max (e.eventPeak[(size_t) b], levelDb[(size_t) b]) - e.eventBase[(size_t) b]);
            most = std::max (most, ex[(size_t) b]);
        }
        const float total = std::clamp (boostDb, 0.0f, 15.0f);
        for (int b = 0; b < numBands; ++b)
        {
            float shape = most > 3.0f && ex[(size_t) b] > 3.0f ? std::pow (ex[(size_t) b] / most, 0.7f) : 0.0f;
            if ((b == 3 || b == 4) && ex[(size_t) b] > 2.0f)
                shape = std::max (shape, 0.35f);
            e.liftTarget[(size_t) b] = dbToGain (total * shape) - 1.0f;
        }
        e.sendTarget = sendAmount;
    }

    void FootstepRadar::release (Event& e) noexcept
    {
        e.liftTarget.fill (0.0f);
        e.sendTarget = 0.0f;
        e.holdS = 0.0f;
        if (e.phase == Phase::lifted)
            e.phase = Phase::done;
    }

    void FootstepRadar::decide (Event& e, const Settings& s) noexcept
    {
        // --- what kind of event this is -------------------------------------------------------------------
        float most = 0.0f;
        std::array<float, numBands> print {};
        for (int b = 0; b < numBands; ++b)
        {
            print[(size_t) b] = std::max (0.0f, e.eventPeak[(size_t) b] - e.eventBase[(size_t) b]);
            most = std::max (most, print[(size_t) b]);
        }

        // How many bands it rose in, and which carry it (within 8 dB of its strongest)
        int spread = 0, carrying = 0;
        for (int b = 0; b < numBands; ++b)
        {
            spread += print[(size_t) b] >= 10.0f ? 1 : 0;
            carrying += print[(size_t) b] >= most - 8.0f && print[(size_t) b] >= 2.0f ? 1 : 0;
        }

        // Is it an impact? Judged band by band, on the best one: a step under a voice or music shows its
        // impact in its own bands (the thump, the scuff) though the others are the voice's
        double pitchEnergy = 0.0;
        const float pitched = pitchStrength (pitchEnergy);
        const float lowPitch = pitchEnergy > 1.0e-10 ? ramp (0.6f, 0.9f, pitched) : 0.0f;
        Decision d {};
        std::array<bool, numBands> leakage {};
        float bestImpact = 0.0f, attack = 50.0f, decay = 0.0f, durationMs = 0.0f, tonal = 0.0f, ring = 0.0f;
        int bestBand = 0;
        for (int b = 0; b < numBands; ++b)
        {
            const float p = print[(size_t) b];
            e.eventBefore[(size_t) b] = e.eventPeak[(size_t) b] - 30.0f;
            if (p < 3.0f)
                continue;
            // Attack: when it first came within 6 dB of its peak (a gravel step's grains, a heel and then
            // a louder toe, still arrive at once; a syllable swells up to it)
            const auto& fh = e.eventFastHistory[(size_t) b];
            int first = 0;
            while (first < e.eventTickCount - 1 && fh[(size_t) first] < e.eventFastPeak[(size_t) b] - 3.0f)
                ++first;
            // ... and from where: back through the moments before the event began, to where the band was last
            // 12 dB under that peak. An impact climbs that in 1 - 3 ms; a syllable swells over 15 or more,
            // whenever the detector happened to notice it.
            // Over a voice or music the band was never 12 dB under the step's peak: measure the rise of what
            // the event added to what was there before it (its power over the 8 - 32 ms before the event)
            float before = 0.0f;
            {
                std::array<float, preTicks - 8> pre {};
                for (int j = 0; j < preTicks - 8; ++j)
                    pre[(size_t) j] = e.eventPre[(size_t) b][(size_t) j];
                std::nth_element (pre.begin(), pre.begin() + pre.size() / 2, pre.end());
                before = std::pow (10.0f, 0.1f * pre[pre.size() / 2]);
            }
            e.eventBefore[(size_t) b] = powerDb (before);
            const float peakPower = std::pow (10.0f, 0.1f * e.eventFastPeak[(size_t) b]);
            int back = 0;
            const float floorDb = peakPower > before ? powerDb (before + 0.063f * (peakPower - before)) : e.eventFastPeak[(size_t) b] - 12.0f;
            {
                int k = first;
                while (k > 0 && fh[(size_t) (k - 1)] > floorDb) { --k; ++back; }
                if (k == 0)
                    for (int j = preTicks - 1; j >= 0 && e.eventPre[(size_t) b][(size_t) j] > floorDb; --j)
                        ++back;
            }
            // ... and when it first came within 6 dB of its (short-envelope) peak, counted from the event's start:
            // voiced sounds swell by that measure, whispered ones by the rise. An impact is quick by both.
            int arrive = 0;
            while (arrive < e.eventTickCount - 1 && e.eventShortHistory[(size_t) b][(size_t) arrive] < e.eventShortPeak[(size_t) b] - 6.0f)
                ++arrive;
            float att = 1000.0f * tickDt * (float) std::max (back, arrive);
            // A step landing inside a louder sound that began just before it (a syllable, a note): in the band
            // where the step stands out, the level climbs steeply out of that sound's plateau, late in the
            // event. Measure that climb on its own: where it began, how fast it reached the peak.
            {
                int k = first;
                while (k > 0 && fh[(size_t) (k - 1)] < fh[(size_t) k] - 0.75f)
                    --k;
                const float climb = fh[(size_t) first] - fh[(size_t) k];
                if (k >= 4 && climb >= 8.0f)
                {
                    int late = k;
                    while (late < e.eventTickCount - 1 && e.eventShortHistory[(size_t) b][(size_t) late] < e.eventShortPeak[(size_t) b] - 6.0f)
                        ++late;
                    att = std::min (att, 1000.0f * tickDt * (float) std::max (first - k, late - k));
                }
            }
            const float dec = e.eventShortPeak[(size_t) b] - e.eventShortAt45[(size_t) b];
            const float dur = 1000.0f * tickDt * (float) e.eventAboveTicks[(size_t) b];   // a step's 8 - 40 ms, a click's 1 - 3
            float ton = 0.0f;
            leakage[(size_t) b] = false;
            if (b < zcBands && e.zcCount[(size_t) b] >= 4)
            {
                const double mean = e.zcSum[(size_t) b] / e.zcCount[(size_t) b];
                // Crossing far faster than the band's own frequency: what it carries is a higher sound's skirt
                // (a ring at 1.3 kHz read through the 250 Hz band), not energy of its own
                leakage[(size_t) b] = mean > 0.0 && sr / (2.0 * mean) > 2.5 * bandHz[(size_t) b];
                const double var = std::max (0.0, e.zcSq[(size_t) b] / e.zcCount[(size_t) b] - mean * mean);
                ton = 1.0f - ramp (0.10f, 0.26f, mean > 0.0 ? (float) (std::sqrt (var) / mean) : 1.0f);
            }
            if (b >= 1 && p >= most - 15.0f)
                ring = std::max (ring, ton);      // a ring anywhere in the event (a reload's, a latch's, a chime's)
            if (b <= 2)
                ton = std::max (ton, lowPitch);   // a voiced syllable, a kick drum, a bass note
            float imp = (1.0f - ramp (6.0f, 16.0f, att)) * ramp (0.5f, 5.0f, dec) * ramp (5.0f, 12.0f, dur) * ramp (2.5f, 9.0f, p)
                      * (1.0f - 0.85f * ramp (0.35f, 0.75f, ton) * (1.0f - ramp (5.0f, 11.0f, dec)));
            imp *= ramp (most - 24.0f, most - 10.0f, p);   // a band far under the event's strongest is weak evidence
            // ... and a low band that was already busy just before (a rumble, the tail of a blast: narrow bands
            // of noise swell and sag by many dB on their own) must rise well clear of what it was doing
            if (b <= 2)
                imp *= ramp (4.0f, 10.0f, e.eventFastPeak[(size_t) b] - e.eventBefore[(size_t) b]);
            d.bandAtt[(size_t) b] = att; d.bandDec[(size_t) b] = dec; d.bandP[(size_t) b] = p; d.bandTon[(size_t) b] = ton; d.bandImp[(size_t) b] = imp;
            if (imp > bestImpact || (bestImpact == 0.0f && b == 0))
            {
                bestImpact = imp;
                attack = att; decay = dec; durationMs = dur; tonal = ton;
                bestBand = b;
            }
        }

        // Stereo: where it is (level), and whether it comes from behind (out of phase)
        e.eventPan = e.eventL + e.eventR > 1.0e-12 ? (float) ((e.eventR - e.eventL) / (e.eventR + e.eventL)) : 0.0f;
        const double corr = e.eventL > 1.0e-12 && e.eventR > 1.0e-12 ? e.eventX / std::sqrt (e.eventL * e.eventR) : 1.0;
        e.eventRear = ramp (0.0f, -0.6f, (float) corr);

        // --- the judgement, a factor per property ---------------------------------------------------------
        const float fDecay = ramp (0.5f, 5.0f, decay);
        const float fTonal = 1.0f - 0.85f * ramp (0.35f, 0.75f, tonal) * (1.0f - ramp (5.0f, 11.0f, decay));
        const float fBang = 1.0f - 0.9f * ramp (-16.0f, -7.0f, e.eventInPeak) * ramp (3.5f, 5.5f, (float) spread);
        // (a repeat of an impact: a rattle, a crate's clatter - not a syllable just before a step)

        const bool airOnly = carrying == 1 && print[5] >= most - 0.01f;
        const float fShape = airOnly ? 0.5f : 1.0f;
        const float fDuration = ramp (5.0f, 12.0f, durationMs);
        // A foot lands with weight: the thump or body band up and staying up (a heel's 20 - 40 ms; a click's
        // splatter into the low bands lasts 2 - 4)
        const auto lasts = [&] (int b) { return 1000.0f * tickDt * (float) e.eventAboveTicks[(size_t) b] >= 8.0f && ! leakage[(size_t) b]; };
        // (and strong: a click's splatter, rung out by the low band filters, sits 20 dB under its peak)
        const bool hasBody = (print[0] >= std::max (6.0f, most - 15.0f) && lasts (0)) || (print[1] >= std::max (6.0f, most - 15.0f) && lasts (1));
        // No weight behind it: fine for a light step now and then, but in a busy scene (rain, rattles:
        // many onsets a second) the high-only ones are what the scene is made of
        // --- against the walkers being followed ------------------------------------------------------------
        float bestScore = 0.0f;
        e.eventMatch = 0.0f;
        e.eventBestTrack = -1;
        for (int t = 0; t < maxTracks; ++t)
        {
            int k = 1;
            const float m = trackMatch (tracks[(size_t) t], e, e.eventStart, k);
            if (m * tracks[(size_t) t].confidence > bestScore)
            {
                bestScore = m * tracks[(size_t) t].confidence;
                e.eventMatch = m;
                e.eventBestTrack = t;
                e.eventExpectedK = k;
            }
        }
        // (from somewhere else, with weight behind one of them: two walkers stepping close together)
        // (or each followed as a walker of its own)
        const bool ownWalker = e.eventBestTrack >= 0 && e.eventMatch >= 0.5f && e.eventBestTrack != lastEventTrack;
        const bool elsewhere = std::abs (e.eventPan - lastEventPan) >= 0.3f && (hasBody || lastEventBody || ownWalker);
        // On a drum machine's grid (a sample-exact beat that other drums or pitched music share): music
        const float grid = machineGrid (e, print);
        const float fGrid = 1.0f - 0.85f * grid;
        // (not when that impact was the drum machine's: then this is what came right after a beat)
        const bool rapid = e.eventStart - lastEventTime < 0.18 && cosine (print, lastEventPrint) > 0.8f && ! elsewhere && lastEventGrid < 0.5f;
        const float fRapid = rapid ? 0.3f : 1.0f;
        const float fBody = hasBody ? 1.0f : ramp (5.0f, 10.0f, onsetRate()) * -0.35f + 0.70f;
        // A pure ring, however short: with no weight behind it, a click and a ring (a reload, a latch, a
        // UI chime); with a heel behind it, a resonant floor (wood, a metal grate) - little held against it
        const float fRing = 1.0f - 0.6f * ramp (0.8f, 1.0f, std::max (tonal, ring)) * (hasBody ? 0.25f : 1.0f);
        // A blip in a single band, with no weight behind it: a raindrop cluster, a flicker of hiss
        const float fNarrow = ! hasBody && spread <= 1 && most < 12.0f ? 0.45f : 1.0f;
        const float base = bestImpact * fRing * fBang * fRapid * fShape * fBody * fNarrow * fGrid * (1.0f - 0.15f * musicality);
        e.eventBestBand = bestBand;

        // A walker's step is trusted on its match, but never a bang or a tone: those stay what they are
        // A walker vouches only for what already sounds somewhat like a step (a hi-hat on the beat does not)
        const float vouched = bestScore * fBang * fTonal * fRing * fNarrow * fRapid * fGrid * fDuration * std::sqrt (fDecay) * ramp (0.08f, 0.25f, base)
                            * (airOnly ? 0.0f : 1.0f) * (1.0f - 0.8f * musicality);
        float probability = 1.0f - (1.0f - base) * (1.0f - 0.9f * vouched);
        e.eventBaseScore = base;
        e.eventProbability = probability;

        float needed = (s.detection == 1 ? 0.36f : s.detection == 2 ? 0.60f : 0.45f) + 0.05f * musicality;
        needed -= 0.02f * (std::clamp (s.sensitivity, 0.0f, 10.0f) - 6.0f);
        bool accept = probability >= needed;

        // Too faint to take on its own, but the third of its kind in a steady walking rhythm, in a quiet,
        // unmusical scene: a walker far off or buried in the ambience
        e.rhythmPeriod = 0.0f;
        if (! accept && base >= (s.detection == 2 ? 0.2f : 0.1f) && ! airOnly && musicality < 0.3f && onsetRate() <= 3.0f)
        {
            e.rhythmPeriod = rhythmOf (e, print);
            if (e.rhythmPeriod > 0.0f)
            {
                accept = true;
                probability = std::max (probability, needed);
                e.eventProbability = probability;
            }
            else
            {
                auto& f = faint[(size_t) faintHead];
                faintHead = (faintHead + 1) % faintMemory;
                f.time = e.eventStart;
                f.print = print;
                f.pan = e.eventPan;
            }
        }

        // Distance (for now): its level and how much of its top end is left; the room tail refines it
        const float hf = print[4] + print[5] - print[0] - print[1];
        const float dLevel = ramp (-20.0f, -58.0f, e.eventInPeak);
        const float dHf = 1.0f - ramp (-14.0f, 6.0f, hf);
        e.eventDistance = saturate01 (0.7f * dLevel + 0.3f * dHf);
        if (e.eventBestTrack >= 0 && e.eventMatch > 0.35f)
            e.eventDistance = 0.5f * e.eventDistance + 0.5f * tracks[(size_t) e.eventBestTrack].distance;

        if (log != nullptr && log->size() < log->capacity())
        {
            const auto bands = d;
            d = { e.eventStart, accept, probability, base, e.eventMatch, attack, decay, most, tonal, e.eventInPeak, (float) spread, e.eventDistance,
                  durationMs, fBody, onsetRate(), musicality, fRing, fBang, fRapid, fNarrow, bestImpact };
            d.grid = grid;
            d.bandAtt = bands.bandAtt; d.bandDec = bands.bandDec; d.bandP = bands.bandP; d.bandTon = bands.bandTon; d.bandImp = bands.bandImp;
            log->push_back (d);
        }

        {
            auto& h = heard[(size_t) heardHead];
            heardHead = (heardHead + 1) % heardMemory;
            h.time = e.eventStart;
            h.print = print;
        }
        if (bestImpact >= 0.2f || accept)
        {
            lastEventTime = e.eventStart;
            lastEventPrint = print;
            lastEventPan = e.eventPan;
            lastEventBody = hasBody;
            lastEventTrack = e.eventMatch >= 0.35f ? e.eventBestTrack : -1;
            lastEventGrid = grid;
        }

        if (accept)
        {
            e.phase = Phase::lifted;
            const float quiet = std::clamp ((-e.eventInPeak - 10.0f) / 25.0f, 0.25f, 1.0f);   // loud, near steps need little
            const float boost = std::clamp (s.boostDb, 0.0f, 12.0f) * std::min (1.0f, 0.4f + 0.7f * probability)
                              * (0.35f + 0.65f * e.eventDistance) * quiet;
            const float sendAmount = std::clamp (s.space, 0.0f, 10.0f) / 10.0f * ramp (0.2f, 0.9f, e.eventDistance) * 0.9f;
            setLift (e, boost, sendAmount);
            e.holdS = std::max (e.holdS, 0.09f + 0.08f * e.eventDistance);
        }
        else
        {
            e.phase = Phase::done;
            release (e);
        }
    }

    void FootstepRadar::confirm (Event& e, bool early) noexcept
    {
        // (early: another step arrived before the 130 ms check - this one passed its decision; it is kept)
        // Still as loud as at its peak 130 ms on: a sustained sound, not a step - withdraw it. Judged in the
        // band the event rose most in over what was there just before it (under a voice, the voice's bands
        // stay up after the step; the step's own band falls back)
        float most = -140.0f;
        int top = 0;
        for (int b = 0; b < numBands; ++b)
        {
            const float rose = e.eventPeak[(size_t) b] - std::max (e.eventBase[(size_t) b], e.eventBefore[(size_t) b]);
            if (rose > most)
            {
                most = rose;
                top = b;
            }
        }
        // A low end that rings on while everything above it has gone: a kick drum (with a hi-hat on it, a
        // bass pluck). A heel's thump is gone by now; a far step in a room rings in every band alike.
        float upperFall = 0.0f;
        for (int b = 2; b < numBands; ++b)
            if (e.eventPeak[(size_t) b] - e.eventBase[(size_t) b] > 6.0f)
                upperFall = std::max (upperFall, e.eventPeak[(size_t) b] - levelDb[(size_t) b]);
        const bool kick = e.eventPeak[0] - e.eventBase[0] > 6.0f && levelDb[0] > e.eventPeak[0] - 8.0f && upperFall >= 22.0f;
        if (! early && (levelDb[(size_t) top] > e.eventPeak[(size_t) top] - 4.0f || kick))
        {
            if (log != nullptr && ! log->empty() && log->back().time == e.eventStart)
                log->back().withdrawn = kick ? 2 : 1;
            e.eventSustained = true;
            release (e);
            e.phase = Phase::done;
            return;
        }

        // The room tail after it (60 - 130 ms so far) against its peak: far steps come with more room
        float peakPower = 0.0f;
        for (int b = 0; b < numBands; ++b)
            peakPower += std::pow (10.0f, 0.1f * e.eventPeak[(size_t) b]);
        const float tailRel = e.eventTailTicks > 0 ? powerDb (e.eventTailSum / (float) e.eventTailTicks) - powerDb (peakPower) : -40.0f;
        e.eventDistance = saturate01 (0.8f * e.eventDistance + 0.2f * ramp (-28.0f, -10.0f, tailRel));

        std::array<float, numBands> print {};
        for (int b = 0; b < numBands; ++b)
            print[(size_t) b] = std::max (0.0f, e.eventPeak[(size_t) b] - e.eventBase[(size_t) b]);

        // Learn it into its walker, or start a new one
        int t = e.eventBestTrack >= 0 && e.eventMatch >= 0.35f ? e.eventBestTrack : -1;
        if (t < 0 && (e.eventBaseScore >= 0.45f || e.rhythmPeriod > 0.0f))
        {
            // A free slot, or the weakest walker
            t = 0;
            for (int k = 0; k < maxTracks; ++k)
            {
                if (! tracks[(size_t) k].active) { t = k; break; }
                if (tracks[(size_t) k].confidence < tracks[(size_t) t].confidence) t = k;
            }
            tracks[(size_t) t] = Track {};
            tracks[(size_t) t].active = true;
            tracks[(size_t) t].id = nextTrackId++;
            tracks[(size_t) t].print = print;
            tracks[(size_t) t].pan = e.eventPan;
            tracks[(size_t) t].rear = e.eventRear;
            tracks[(size_t) t].distance = e.eventDistance;
            tracks[(size_t) t].confidence = 0.15f + 0.2f * e.eventBaseScore;
            if (e.rhythmPeriod > 0.0f)
            {
                tracks[(size_t) t].period = e.rhythmPeriod;   // three steps in already: it knows the pace
                tracks[(size_t) t].confidence = 0.45f;
                tracks[(size_t) t].steps = 3;
            }
            tracks[(size_t) t].last = e.eventStart;
            tracks[(size_t) t].loudestDb = e.eventInPeak;
            tracks[(size_t) t].steps = 1;
        }
        else if (t >= 0)
        {
            auto& tr = tracks[(size_t) t];
            for (int b = 0; b < numBands; ++b)
                tr.print[(size_t) b] += 0.3f * (print[(size_t) b] - tr.print[(size_t) b]);
            tr.pan += 0.35f * (e.eventPan - tr.pan);
            tr.rear += 0.35f * (e.eventRear - tr.rear);
            tr.distance += 0.4f * (e.eventDistance - tr.distance);
            const double ioi = (e.eventStart - tr.last) / (double) std::max (1, e.eventExpectedK);
            if (ioi >= 0.18 && ioi <= 1.1)
                tr.period = tr.period <= 0.0f ? (float) ioi : tr.period + 0.3f * ((float) ioi - tr.period);
            tr.confidence = std::min (1.0f, tr.confidence + 0.12f + 0.2f * std::max (e.eventBaseScore, e.eventMatch));
            tr.last = e.eventStart;
            tr.loudestDb = std::max (tr.loudestDb - 1.0f, e.eventInPeak);
            ++tr.steps;
        }

        ++accepted;
        Step st;
        st.time = e.eventStart;
        st.pan = e.eventPan;
        st.rear = e.eventRear;
        st.distance = e.eventDistance;
        st.confidence = e.eventProbability;
        st.levelDb = e.eventInPeak;
        float most2 = 0.0f;
        for (float g : liftTarget)
            most2 = std::max (most2, g);
        st.boostDb = 20.0f * std::log10 (1.0f + most2);
        st.track = t >= 0 ? tracks[(size_t) t].id : -1;
        recent[(size_t) (stepsTotal % recentCapacity)] = st;
        ++stepsTotal;

        e.eventSustained = true;   // confirmed once
    }
}
