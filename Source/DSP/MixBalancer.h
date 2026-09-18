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

        Attacks are respected: a band's cut is held back while that band is in a fresh transient, so
        footsteps and gunshots keep their front edge. Zero latency; minimum-phase bells and shelves.
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
            bool active = false;
        };

        static constexpr std::array<float, numBands> centreHz { 70.0f, 200.0f, 500.0f, 1300.0f, 3500.0f, 9000.0f };

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
            toTick = tickEvery;
            deepestCutDb = 0.0f;
        }

        void process (float* const* data, int numChannels, int n, const Settings& s) noexcept
        {
            const int ch = std::min (chans, numChannels);
            if (ch <= 0)
                return;

            if (! s.active)
            {
                // True bypass; the analysis keeps listening so it is ready when it comes back in
                if (anyGain())
                {
                    gainDb = {};
                    for (int b = 0; b < numBands; ++b) design (b, 0.0f);
                    for (auto& c : filters) for (auto& f : c) f.reset();
                }
                for (int i = 0; i < n; ++i)
                    listen (ch == 2 ? 0.5f * (data[0][i] + data[1][i]) : data[0][i]);
                return;
            }

            for (int i = 0; i < n; ++i)
            {
                listen (ch == 2 ? 0.5f * (data[0][i] + data[1][i]) : data[0][i]);
                if (--toTick <= 0)
                {
                    controlTick (s);
                    toTick = tickEvery;
                }

                if (! engaged)
                    continue;
                for (int c = 0; c < ch; ++c)
                {
                    float w = data[c][i];
                    for (int b = 0; b < numBands; ++b)
                        w = filters[(size_t) c][(size_t) b].process (coeffs[(size_t) b], w);
                    data[c][i] = w;
                }
            }
        }

        /** Per band: what it is doing (dB, + lift / - cut) and its level now (dB), for the display. */
        float getGainDb (int band) const noexcept  { return gainDb[(size_t) band]; }
        float getLevelDb (int band) const noexcept { return powerToDb (fast[(size_t) band]); }
        float getDeepestCutDb() const noexcept     { return deepestCutDb; }

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
                deepest = std::min (deepest, g);
                if (std::abs (g - designedDb[(size_t) b]) > 0.05f)
                    design (b, g);
            }
            deepestCutDb = -deepest;
            engaged = anyGain() || anyState();
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
    };
}
