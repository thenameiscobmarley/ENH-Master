#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "DspMath.h"
#include "LoudnessMeter.h"

namespace enh::dsp
{
    /** EAR GUARD - always on, like the speaker protection: the sound never suddenly gets much louder than
        it has been.

        The output limiter keeps every peak under full scale, but full scale can be far louder than what
        you set your headset's volume for: a quiet game the rack has been lifting, then a blast - peak-
        limited, and still 20 dB louder than a moment ago. A jump that *lasts* is what hurts ears.

        It measures loudness the way LUFS do (K-weighted) over the same 400 ms a momentary loudness meter
        uses, and keeps it within 12 dB (about four times as loud) of how loud the sound has usually been:
        "usually" is that loudness followed over about ten seconds going up and thirty coming down, only while
        there is sound; a break far under it (a pause in the music, a quiet menu) does not lower it at all, so
        what comes back after the break is not a jump. It learns a jump only slowly (at most 0.75 dB a second
        while holding one).

        It works as an energy budget: the output's last 400 ms may hold only so much energy, and with 5 ms of
        look-ahead it knows exactly what is about to play, so it lets through as much of it as still fits.
        A sustained blast is held at +12 dB from its first moments; a gunshot 50 ms long fits most of its
        energy in the budget and is barely touched; loud material that stays loud, and ordinary ups and
        downs, are never touched at all - a DAW mastering session is not affected.

        5 ms of latency (the look-ahead), reported to the host. */
    class EarGuard
    {
    public:
        static constexpr float defaultJumpDb = 12.0f;   // how far over its usual loudness the sound may get
        static constexpr float maxReductionDb = 30.0f;

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            kWeighting (sr, pre, rlb);
            attackK = (float) std::exp (-1.0 / (0.0008 * sr));
            releaseK = (float) std::exp (-1.0 / (1.5 * sr));
            lookahead = decisionEvery * std::max (2, (int) std::lround (0.005 * sr / decisionEvery));   // whole blocks
            for (auto& d : delay) d.assign ((size_t) lookahead, 0.0f);
            kDelay.assign ((size_t) lookahead, 0.0f);
            windowBlocks = std::max (4, (int) std::lround (0.4 * sr / decisionEvery));
            outBlocks.assign ((size_t) windowBlocks, 0.0);
            inBlocks.assign ((size_t) windowBlocks, 0.0);
            const double every = decisionEvery / sr;
            usualUpK = (float) std::exp (-every / 10.0);
            usualDownK = (float) std::exp (-every / 30.0);   // a quieter stretch becomes the new normal only slowly
            learnK = (float) std::exp (-every / 0.3);
            learnDecisions = (int) (2.0 / every);
            maxRisePerDecision = (float) (0.75 * every);
            reset();
        }

        void reset()
        {
            for (auto& s : preState) s = {};
            for (auto& s : rlbState) s = {};
            for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
            std::fill (kDelay.begin(), kDelay.end(), 0.0f);
            std::fill (outBlocks.begin(), outBlocks.end(), 0.0);
            std::fill (inBlocks.begin(), inBlocks.end(), 0.0);
            outSum = inSum = aheadPower = blockOut = blockIn = 0.0;
            delayPos = blockPos = inBlock = 0;
            usualDb = -200.0f;
            gain = targetGain = 1.0f;
            untilDecision = decisionEvery;
            learning = 0;
            heard = false;
        }

        /** In place, after everything else and before the output limiter. */
        void process (float* const* ch, int numChannels, int numSamples) noexcept
        {
            const int nc = std::min (2, numChannels);
            for (int i = 0; i < numSamples; ++i)
            {
                // The K-weighted power of what has just come in (heard 5 ms from now)
                float power = 0.0f;
                for (int c = 0; c < nc; ++c)
                {
                    const float k = rlbState[(size_t) c].process (rlb, preState[(size_t) c].process (pre, ch[c][i]));
                    power += k * k;
                }

                // What is heard now leaves the look-ahead; what came in joins it
                auto& kd = kDelay[(size_t) delayPos];
                const float heardPower = kd;
                aheadPower += (double) power - (double) kd;
                kd = power;

                if (--untilDecision <= 0)
                {
                    untilDecision = decisionEvery;
                    decide();
                }
                const float k = targetGain < gain ? attackK : releaseK;
                gain = k * gain + (1.0f - k) * targetGain;

                for (int c = 0; c < nc; ++c)
                {
                    auto& d = delay[(size_t) c][(size_t) delayPos];
                    const float x = ch[c][i];
                    ch[c][i] = d * gain;
                    d = x;
                }
                if (++delayPos >= lookahead)
                    delayPos = 0;

                // The last 400 ms of what was heard (after the gain) and of what came to be heard (before it)
                blockOut += (double) heardPower * (double) gain * (double) gain;
                blockIn += (double) heardPower;
                if (++inBlock >= decisionEvery)
                {
                    inBlock = 0;
                    outSum += blockOut - outBlocks[(size_t) blockPos];
                    inSum += blockIn - inBlocks[(size_t) blockPos];
                    outBlocks[(size_t) blockPos] = blockOut;
                    inBlocks[(size_t) blockPos] = blockIn;
                    blockOut = blockIn = 0.0;
                    if (++blockPos >= windowBlocks)
                    {
                        // Once a window: add the sums up again exactly (running sums drift over hours)
                        blockPos = 0;
                        outSum = inSum = 0.0;
                        for (int j = 0; j < windowBlocks; ++j)
                        {
                            outSum += outBlocks[(size_t) j];
                            inSum += inBlocks[(size_t) j];
                        }
                        aheadPower = 0.0;
                        for (float v : kDelay)
                            aheadPower += (double) v;
                    }
                }
            }
        }

        /** How far over its usual loudness the sound may jump (the EAR GUARD method: 12, 15 or 18 dB). */
        void setJumpDb (float db) noexcept { jumpDb = std::clamp (db, 6.0f, 24.0f); }

        int getLatencySamples() const noexcept { return lookahead; }
        float getReductionDb() const noexcept  { return -20.0f * std::log10 (std::max (gain, 1.0e-6f)); }
        float getUsualLufs() const noexcept    { return usualDb; }

    private:
        void decide() noexcept
        {
            const double windowSamples = (double) windowBlocks * decisionEvery;
            const float momentaryDb = 10.0f * (float) std::log10 (std::max (inSum, 0.0) / windowSamples + 1.0e-20) - 0.691f;

            // "Usually": starts at the first real sound, then follows the momentary loudness - only while
            // there is sound (under -70 LUFS, the loudness meters' own gate, is a pause, not a new normal), and learns a jump only slowly
            if (momentaryDb > -70.0f)
            {
                if (! heard)
                {
                    heard = true;
                    usualDb = momentaryDb;
                    learning = learnDecisions;
                }
                if (learning > 0)
                {
                    // The first two seconds of sound: learn how loud it is (quickly upward), never hold
                    --learning;
                    usualDb = momentaryDb > usualDb ? learnK * usualDb + (1.0f - learnK) * momentaryDb : usualDb;
                    targetGain = 1.0f;
                    return;
                }
                // A break (far under what has been playing: a pause in the music, a quiet menu) is not a new
                // normal - the volume was set for what played before it, and when that comes back it is not a
                // jump. Only a stretch within 20 dB of it moves "usually" down, and slowly.
                if (momentaryDb > usualDb - 20.0f)
                {
                    const float k = momentaryDb > usualDb ? usualUpK : usualDownK;
                    float next = k * usualDb + (1.0f - k) * momentaryDb;
                    if (targetGain < 0.99f)
                        next = std::min (next, usualDb + maxRisePerDecision);
                    usualDb = next;
                }
            }
            if (! heard)
            {
                targetGain = 1.0f;
                return;
            }

            // The budget: the heard 400 ms may hold at most this much K-weighted energy. Over the look-ahead,
            // its oldest blocks leave the window and what is in the look-ahead enters it: let through as
            // much of that as fits (never more than all of it)
            const double allowed = std::pow (10.0, ((double) usualDb + (double) jumpDb + 0.691) / 10.0) * windowSamples;
            double leaving = 0.0;
            for (int j = 0, n = lookahead / decisionEvery; j < n; ++j)
                leaving += outBlocks[(size_t) ((blockPos + j) % windowBlocks)];
            const double room = allowed - std::max (0.0, outSum - leaving);
            const double g2 = room / std::max (aheadPower, 1.0e-20);
            const double floorGain = std::pow (10.0, -maxReductionDb / 20.0);
            targetGain = g2 >= 1.0 ? 1.0f : (float) std::max (floorGain, std::sqrt (std::max (g2, 0.0)));
        }

        static constexpr int decisionEvery = 16;
        double sr = 48000.0;
        BiquadCoeffs pre, rlb;
        std::array<BiquadState, 2> preState {}, rlbState {};
        std::array<std::vector<float>, 2> delay;
        std::vector<float> kDelay;
        std::vector<double> outBlocks, inBlocks;
        double outSum = 0.0, inSum = 0.0, aheadPower = 0.0, blockOut = 0.0, blockIn = 0.0;
        int lookahead = 240, delayPos = 0, windowBlocks = 1200, blockPos = 0, inBlock = 0;
        float attackK = 0.0f, releaseK = 0.0f, usualUpK = 0.0f, usualDownK = 0.0f, learnK = 0.0f, maxRisePerDecision = 0.0f;
        float usualDb = -200.0f, gain = 1.0f, targetGain = 1.0f, jumpDb = defaultJumpDb;
        int untilDecision = decisionEvery, learning = 0, learnDecisions = 6000;
        bool heard = false;
    };
}
