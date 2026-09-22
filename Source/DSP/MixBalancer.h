#pragma once

#include <array>
#include "DspMath.h"

namespace enh::dsp
{
    /** MIX BALANCER - rides six band faders the way a mix engineer would, to keep the mix balanced
        moment to moment.

        It listens to six regions (low < 100 Hz, 200 Hz, 500 Hz, 1.3 kHz, 3.5 kHz, air > 7 kHz) and
        compares how far each one has moved from its usual level (the last few seconds) with how far
        the mix as a whole has moved (the median of the six). A region that suddenly crowds the mix - the low-mids of an explosion burying the
        footsteps, a harsh upper-mid burst - is taken down by how far it jumped; a region that drops
        out is lifted, more gently. Only the balance counts: when everything gets louder or quieter
        together nothing moves, so this is tonal correction, never loudness control.

          BALANCE  how much of each jump it corrects (0 = none, 10 = most of it)
          SPEED    how quickly it rides (slow and smooth .. fast)
          TILT     the balance it steers toward: darker (-) or brighter (+) than the programme's own
          RANGE    the most any band moves (dB; lifts are held to half of it)
          RESOLUTION  from six broad bands (0) to spectral (1): 28 third-octave bands (31.5 Hz - 16 kHz),
                   each ridden the same way against the median of all 28 - as precise as it gets
                   without an FFT's latency. In between, the two sets of faders are blended: the six
                   are scaled by 1 - RESOLUTION and the 28 by RESOLUTION, in series, so the result is
                   exactly the blend of the two curves (no phasing between parallel paths).

        Attacks are respected: a band's cut is held back while that band is in a fresh transient, so
        footsteps and gunshots keep their front edge.

        Loudness keeper: where a fader holds a band below what it usually carries (letting go after a
        jump has passed, or a cut deeper than the jump), the mix is quieter than usual and the rest of
        it sounds quieter too, though its level never moved. That loss is measured ear-weighted (the
        shape of the K-weighting) and 60 % of it is given back to the whole mix, at most 3 dB,
        following the faders, and never past the headroom under 0 dBFS. Taking a jump away loses
        nothing: it was never part of the mix. Zero latency; minimum-phase bells and shelves.
        Real-time safe after prepare().
    */
    class MixBalancer
    {
    public:
        static constexpr int numBands = 6;

        struct Settings
        {
            float amount = 0.5f;    // 0..1
            float speed = 0.5f;     // 0..1
            float tilt = 0.0f;      // -1..1 (darker .. brighter)
            float rangeDb = 6.0f;   // 0..12
            float resolution = 0.0f; // 0 = six bands .. 1 = spectral (third-octave)
            bool active = false;
        };

        static constexpr std::array<float, numBands> centreHz { 70.0f, 200.0f, 500.0f, 1300.0f, 3500.0f, 9000.0f };

        static constexpr int numFine = 28;                         // spectral mode: third-octave bands
        static float fineHz (int k) noexcept { return 31.5f * std::pow (2.0f, (float) k / 3.0f); }

        void prepare (double sampleRate, int numChannels)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            chans = std::min (2, std::max (1, numChannels));
            tickEvery = std::max (8, (int) std::lround (sr / 750.0));
            dt = (float) tickEvery / (float) sr;

            analysisCoeffs[0] = SvfCoeffs::make (sr, 100.0, 0.7071);
            for (int b = 1; b < numBands - 1; ++b)
                analysisCoeffs[(size_t) b] = SvfCoeffs::make (sr, centreHz[(size_t) b], 1.0);
            analysisCoeffs[numBands - 1] = SvfCoeffs::make (sr, 7000.0, 0.7071);
            for (int b = 1; b < numBands - 1; ++b)
                bells[(size_t) b].setup (sr, centreHz[(size_t) b], 1.0);

            // Each band listens over a few of its own cycles: the bass needs longer than the air
            constexpr std::array<double, numBands> attS { 0.020, 0.010, 0.006, 0.005, 0.005, 0.005 };
            constexpr std::array<double, numBands> relS { 0.160, 0.100, 0.070, 0.060, 0.060, 0.060 };
            for (int b = 0; b < numBands; ++b)
            {
                fastAtt[(size_t) b] = onePole (attS[(size_t) b], sr);
                fastRel[(size_t) b] = onePole (relS[(size_t) b], sr);
            }
            transientK = onePole (0.0015, sr);
            slowK = onePole (4.0, sr);

            fineCount = 0;
            for (int k = 0; k < numFine; ++k)
            {
                const double hz = fineHz (k);
                if (hz < 0.43 * sr)
                    fineCount = k + 1;
                fineDetect.set (k, BiquadCoeffs::bandPass (sr, hz, 4.3));
                fineBells[(size_t) k].setup (sr, hz, 4.3);
                // Each band listens over a few of its own cycles
                fineAtt[(size_t) k] = onePole (std::max (0.005, 2.5 / hz), sr);
                fineRel[(size_t) k] = onePole (std::max (0.060, 10.0 / hz), sr);
            }
            reset();
        }

        void reset()
        {
            for (auto& s : analysis) s.reset();
            for (auto& c : filters) for (auto& s : c) s.reset();
            fast = {}; slow = {}; transient = {};
            gainDb = {}; designedDb.fill (1000.0f);
            for (int b = 0; b < numBands; ++b)
                design (b, 0.0f);
            fineDetect.reset();
            for (auto& c : fineFilters) for (auto& s : c) s.reset();
            fineFast = {}; fineSlow = {}; fineTransient = {};
            fineGainDb = {}; fineDesignedDb = {};
            for (int k = 0; k < numFine; ++k)
                fineCoeffs[(size_t) k] = fineBells[(size_t) k].make (0.0f);
            fineOn = fineEngaged = false;
            coarseScale = 1.0f; fineScale = 0.0f;
            toTick = tickEvery;
            deepestCutDb = 0.0f;
            makeupDb = 0.0f; makeupGain = 1.0f; peakEnv = 0.0f;
        }

        void process (float* const* data, int numChannels, int n, const Settings& s) noexcept
        {
            const int ch = std::min (chans, numChannels);
            if (ch <= 0)
                return;

            if (! s.active)
            {
                // True bypass; the analysis keeps listening so it is ready when it comes back in
                if (anyGain() || fineEngaged)
                {
                    gainDb = {};
                    for (int b = 0; b < numBands; ++b) design (b, 0.0f);
                    for (auto& c : filters) for (auto& f : c) f.reset();
                    fineGainDb = {};
                    fineDesignedDb = {};
                    for (int k = 0; k < numFine; ++k) fineCoeffs[(size_t) k] = fineBells[(size_t) k].make (0.0f);
                    for (auto& c : fineFilters) for (auto& f : c) f.reset();
                    fineEngaged = false;
                }
                makeupDb = 0.0f; makeupGain = 1.0f;
                for (int i = 0; i < n; ++i)
                    listen (ch == 2 ? 0.5f * (data[0][i] + data[1][i]) : data[0][i]);
                return;
            }

            const float peakRelease = std::exp (-1.0f / (0.060f * (float) sr));
            const float gainGlide = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
            for (int i = 0; i < n; ++i)
            {
                listen (ch == 2 ? 0.5f * (data[0][i] + data[1][i]) : data[0][i]);
                const float peak = ch == 2 ? std::max (std::abs (data[0][i]), std::abs (data[1][i])) : std::abs (data[0][i]);
                peakEnv = peak > peakEnv ? peak : peakEnv * peakRelease;
                if (--toTick <= 0)
                {
                    controlTick (s);
                    toTick = tickEvery;
                }

                if (engaged)
                    for (int c = 0; c < ch; ++c)
                    {
                        float w = data[c][i];
                        for (int b = 0; b < numBands; ++b)
                            w = filters[(size_t) c][(size_t) b].process (coeffs[(size_t) b], w);
                        data[c][i] = w;
                    }
                if (fineEngaged)
                    for (int c = 0; c < ch; ++c)
                    {
                        float w = data[c][i];
                        for (int k = 0; k < fineCount; ++k)
                            w = fineFilters[(size_t) c][(size_t) k].process (fineCoeffs[(size_t) k], w);
                        data[c][i] = w;
                    }
                if (makeupDb > 0.0f || makeupGain != 1.0f)
                {
                    makeupGain += (dbToGain (makeupDb) - makeupGain) * gainGlide;
                    if (makeupDb <= 0.0f && std::abs (makeupGain - 1.0f) < 1.0e-5f)
                        makeupGain = 1.0f;
                    for (int c = 0; c < ch; ++c)
                        data[c][i] *= makeupGain;
                }
            }
        }

        /** Per band: what it is doing (dB, + lift / - cut, as applied) and its level now (dB), for the display. */
        float getGainDb (int band) const noexcept  { return gainDb[(size_t) band] * coarseScale; }
        float getFineGainDb (int k) const noexcept { return fineGainDb[(size_t) k] * fineScale; }
        float getLevelDb (int band) const noexcept { return powerToDb (fast[(size_t) band]); }
        float getDeepestCutDb() const noexcept     { return deepestCutDb; }
        float getMakeupDb() const noexcept         { return makeupDb; }   // the loudness keeper's lift

        /** Roughly how much a band counts toward loudness, per unit of its power (the K-weighting's
            shape: little below 60 Hz, up to +4 dB above 2 kHz). */
        static float loudnessWeight (float hz) noexcept
        {
            const float lowCut = (hz * hz) / (hz * hz + 60.0f * 60.0f);
            return lowCut * (1.0f + 1.5f * (hz * hz) / (hz * hz + 1500.0f * 1500.0f));
        }

    private:
        inline void listen (float mono) noexcept
        {
            for (int b = 0; b < numBands; ++b)
            {
                const auto o = analysis[(size_t) b].process (analysisCoeffs[(size_t) b], mono);
                const float v = b == 0 ? o.low : b == numBands - 1 ? o.high : o.band;
                const float p = v * v;
                auto& f = fast[(size_t) b];
                f = (p > f ? fastAtt[(size_t) b] : fastRel[(size_t) b]) * (f - p) + p;
                auto& t = transient[(size_t) b];
                t = transientK * (t - p) + p;
            }

            if (fineOn)
            {
                // All third-octave bands at once (vectorised bank)
                alignas (16) float y[numFine];
                fineDetect.process (mono, y, fineCount);
                for (int k = 0; k < fineCount; ++k)
                {
                    const float p = y[k] * y[k];
                    float& f = fineFast[(size_t) k];
                    f = (p > f ? fineAtt[(size_t) k] : fineRel[(size_t) k]) * (f - p) + p;
                    float& tr = fineTransient[(size_t) k];
                    tr = transientK * (tr - p) + p;
                }
            }
        }

        /** Spectral mode's 28 faders, the same rules as the six, against the median of all 28. */
        void fineTick (const Settings& s, bool listening, float amount, float range, float kAtt, float kRel) noexcept
        {
            const float r = std::clamp (s.resolution, 0.0f, 1.0f);
            fineOn = r > 0.001f || fineEngaged;
            fineScale = r;
            if (! fineOn)
                return;

            double totalSlow = 0.0;
            for (int k = 0; k < fineCount; ++k)
                totalSlow += fineSlow[(size_t) k];
            const float learn = listening ? 1.0f - std::pow (slowK, (float) tickEvery) : 0.0f;
            std::array<float, numFine> moved {};
            for (int k = 0; k < fineCount; ++k)
            {
                auto& sl = fineSlow[(size_t) k];
                sl += (fineFast[(size_t) k] - sl) * learn * (std::abs (fineGainDb[(size_t) k]) > 1.0f ? 0.25f : 1.0f);
                moved[(size_t) k] = 10.0f * std::log10 ((fineFast[(size_t) k] + 1.0e-12f) / (sl + 1.0e-12f));
            }
            auto sorted = moved;
            std::sort (sorted.begin(), sorted.begin() + fineCount);
            const float mixMoved = fineCount > 1 ? 0.5f * (sorted[(size_t) (fineCount / 2 - 1)] + sorted[(size_t) (fineCount / 2)]) : 0.0f;
            fineMixMoved = mixMoved;

            bool any = false;
            for (int k = 0; k < fineCount; ++k)
            {
                float target = 0.0f;
                if (listening && totalSlow > 1.0e-9 && fineSlow[(size_t) k] > 0.002 * totalSlow)
                {
                    const float position = ((float) k - 0.5f * (float) (fineCount - 1)) / (0.5f * (float) (fineCount - 1));
                    const float error = moved[(size_t) k] - mixMoved - std::clamp (s.tilt, -1.0f, 1.0f) * 3.0f * position;
                    const float beyond = std::copysign (std::max (0.0f, std::abs (error) - 1.5f), error);
                    target = std::clamp (-amount * beyond, -range, 0.5f * range);
                    if (target < 0.0f && fineTransient[(size_t) k] > 2.5f * fineFast[(size_t) k])
                        target = std::max (target, fineGainDb[(size_t) k]);
                }
                auto& g = fineGainDb[(size_t) k];
                const bool movingAway = std::abs (target) > std::abs (g);
                g += (target - g) * (! movingAway ? kRel : target < 0.0f ? kAtt : 0.5f * kRel);

                const float applied = g * r;
                if (std::abs (applied - fineDesignedDb[(size_t) k]) > 0.05f)
                {
                    fineDesignedDb[(size_t) k] = std::abs (applied) < 0.05f ? 0.0f : applied;
                    fineCoeffs[(size_t) k] = fineBells[(size_t) k].make (fineDesignedDb[(size_t) k]);
                }
                any = any || fineDesignedDb[(size_t) k] != 0.0f;
            }
            bool state = false;
            for (auto& c : fineFilters)
                for (int k = 0; k < fineCount; ++k)
                    state = state || c[(size_t) k].z1 != 0.0f || c[(size_t) k].z2 != 0.0f;
            fineEngaged = any || state;
        }

        void controlTick (const Settings& s) noexcept
        {
            double total = 0.0, totalSlow = 0.0;
            for (int b = 0; b < numBands; ++b)
            {
                total += fast[(size_t) b];
                totalSlow += slow[(size_t) b];
            }

            // Silence and near-silence teach it nothing and move nothing
            const bool listening = total > 1.0e-7;   // about -70 dBFS
            const float learn = listening ? 1.0f - std::pow (slowK, (float) tickEvery) : 0.0f;
            // A band it is correcting is learnt four times slower, so a sudden excess is not simply
            // accepted as the new normal; a lasting change still is, over tens of seconds
            for (int b = 0; b < numBands; ++b)
                slow[(size_t) b] += (fast[(size_t) b] - slow[(size_t) b]) * learn * (std::abs (gainDb[(size_t) b]) > 1.0f ? 0.25f : 1.0f);

            const float amount = std::clamp (s.amount, 0.0f, 1.0f) * 0.85f;
            coarseScale = 1.0f - std::clamp (s.resolution, 0.0f, 1.0f);
            const float range = std::clamp (s.rangeDb, 0.0f, 12.0f);
            const float speed = std::clamp (s.speed, 0.0f, 1.0f);
            const float attackS = 0.200f * std::pow (0.1f, speed), releaseS = 1.2f * std::pow (0.15f, speed);
            const float kAtt = 1.0f - std::exp (-dt / attackS), kRel = 1.0f - std::exp (-dt / releaseS);

            // How far each band has moved from its usual level, and how far the mix as a whole has moved
            // (the median of the six): a band's jump is its move beyond the mix's. One band jumping
            // stands out; everything getting louder together cancels out.
            std::array<float, numBands> moved {};
            for (int b = 0; b < numBands; ++b)
                moved[(size_t) b] = (float) (10.0 * std::log10 ((fast[(size_t) b] + 1.0e-12) / (slow[(size_t) b] + 1.0e-12)));
            auto sorted = moved;
            std::sort (sorted.begin(), sorted.end());
            const float mixMoved = 0.5f * (sorted[numBands / 2 - 1] + sorted[numBands / 2]);

            float deepest = 0.0f;
            for (int b = 0; b < numBands; ++b)
            {
                float target = 0.0f;
                // A band carrying next to nothing of the mix (< 1 % of it, usually) is not ridden: its
                // level estimate is mostly noise, and there is nothing there to balance
                if (listening && totalSlow > 1.0e-9 && slow[(size_t) b] > 0.01 * totalSlow)
                {
                    const float jumpDb = moved[(size_t) b] - mixMoved;
                    // TILT: the balance it steers toward, +-3 dB at the ends of the spectrum
                    const float tiltDb = std::clamp (s.tilt, -1.0f, 1.0f) * 3.0f * ((float) b - 2.5f) / 2.5f;
                    const float error = jumpDb - tiltDb;

                    // Small wobbles are the programme breathing: a soft 1.5 dB dead zone
                    const float beyond = std::copysign (std::max (0.0f, std::abs (error) - 1.5f), error);
                    target = std::clamp (-amount * beyond, -range, 0.5f * range);

                    // A band in a fresh attack is not cut yet: the front edge goes through
                    if (target < 0.0f && transient[(size_t) b] > 2.5f * fast[(size_t) b])
                        target = std::max (target, gainDb[(size_t) b]);
                }

                // Cuts come in at the SPEED attack; lifts come in gently (half the release speed),
                // and both let go at the release speed
                auto& g = gainDb[(size_t) b];
                const bool movingAway = std::abs (target) > std::abs (g);
                g += (target - g) * (! movingAway ? kRel : target < 0.0f ? kAtt : 0.5f * kRel);
                const float applied = g * coarseScale;
                deepest = std::min (deepest, applied);
                if (std::abs (applied - designedDb[(size_t) b]) > 0.05f)
                    design (b, applied);
            }
            engaged = anyGain() || anyState();

            // RESOLUTION: the six faders give way to the 28 of spectral mode
            fineTick (s, listening, amount, range, kAtt, kRel);
            for (int k = 0; k < fineCount; ++k)
                deepest = std::min (deepest, fineGainDb[(size_t) k] * fineScale);
            deepestCutDb = -deepest;

            // Loudness keeper: a jump the faders take away was never part of the mix, but where a fader
            // holds a band below what it usually carries (still letting go after the jump has passed, or
            // cutting harder than the jump), the mix is quieter than usual and the rest of it sounds
            // quieter too. Each band's usual level is its slow level moved with the mix; the loss is
            // ear-weighted (lifts count back), and 60 % of it is given back to the whole mix, within
            // 3 dB and the headroom.
            auto keptOf = [] (auto bands, auto fastOf, auto slowOf, auto gainOf, auto hzOf, double usualScale)
            {
                double usualTotal = 1.0e-20, kept = 0.0;
                for (int b = 0; b < bands; ++b)
                {
                    const double w = loudnessWeight (hzOf (b));
                    const double now = fastOf (b), usual = std::min (now, slowOf (b) * usualScale);
                    usualTotal += usual * w;
                    kept += std::min (usual, now * std::pow (10.0, gainOf (b) / 10.0)) * w;
                }
                return kept / usualTotal;
            };
            double kept = keptOf (numBands, [this] (int b) { return fast[(size_t) b]; }, [this] (int b) { return slow[(size_t) b]; },
                                  [this] (int b) { return gainDb[(size_t) b] * coarseScale; }, [] (int b) { return centreHz[(size_t) b]; },
                                  std::pow (10.0, mixMoved / 10.0));
            if (fineOn)
                kept *= keptOf (fineCount, [this] (int k) { return (double) fineFast[(size_t) k]; }, [this] (int k) { return (double) fineSlow[(size_t) k]; },
                                [this] (int k) { return fineGainDb[(size_t) k] * fineScale; }, [] (int k) { return fineHz (k); },
                                std::pow (10.0, fineMixMoved / 10.0));
            const float lostDb = listening ? (float) (-10.0 * std::log10 (std::max (0.05, kept))) : 0.0f;
            const float headroomDb = -20.0f * std::log10 (std::max (1.0e-6f, peakEnv)) - 1.0f;
            const float target = std::clamp (std::min (0.6f * lostDb, headroomDb), 0.0f, 3.0f);
            makeupDb += (target - makeupDb) * (target > makeupDb ? kAtt : kRel);
            if (makeupDb < 0.01f && target <= 0.0f)
                makeupDb = 0.0f;
        }

        void design (int b, float db) noexcept
        {
            designedDb[(size_t) b] = std::abs (db) < 0.05f ? 0.0f : db;
            const float d = designedDb[(size_t) b];
            if (b == 0)
                coeffs[0] = BiquadCoeffs::lowShelf (sr, 100.0, 0.7071, d);
            else if (b == numBands - 1)
                coeffs[(size_t) b] = BiquadCoeffs::highShelf (sr, 7000.0, 0.7071, d);
            else
                coeffs[(size_t) b] = bells[(size_t) b].make (d);
        }

        bool anyGain() const noexcept
        {
            for (float d : designedDb)
                if (d != 0.0f && d < 999.0f)
                    return true;
            return false;
        }

        bool anyState() const noexcept
        {
            for (auto& c : filters)
                for (auto& f : c)
                    if (f.z1 != 0.0f || f.z2 != 0.0f)
                        return true;
            return false;
        }

        double sr = 48000.0;
        int chans = 2, tickEvery = 64, toTick = 64;
        float dt = 1.0f / 750.0f;
        std::array<SvfCoeffs, numBands> analysisCoeffs {};
        std::array<SvfState, numBands> analysis {};
        std::array<PeakingDesigner, numBands> bells {};
        std::array<BiquadCoeffs, numBands> coeffs {};
        std::array<std::array<BiquadState, numBands>, 2> filters {};
        std::array<double, numBands> fast {}, slow {}, transient {};
        std::array<float, numBands> gainDb {}, designedDb {};
        std::array<float, numBands> fastAtt {}, fastRel {};
        float transientK = 0.0f, slowK = 0.0f, deepestCutDb = 0.0f;
        bool engaged = false;

        // Spectral mode
        int fineCount = numFine;
        BiquadBank<numFine> fineDetect {};
        alignas (16) std::array<float, numFine> fineFast {}, fineSlow {}, fineTransient {}, fineAtt {}, fineRel {};
        std::array<float, numFine> fineGainDb {}, fineDesignedDb {};
        std::array<PeakingDesigner, numFine> fineBells {};
        std::array<BiquadCoeffs, numFine> fineCoeffs {};
        std::array<std::array<BiquadState, numFine>, 2> fineFilters {};
        float coarseScale = 1.0f, fineScale = 0.0f;
        float makeupDb = 0.0f, makeupGain = 1.0f, peakEnv = 0.0f, fineMixMoved = 0.0f;   // loudness keeper
        bool fineOn = false, fineEngaged = false;
    };
}
