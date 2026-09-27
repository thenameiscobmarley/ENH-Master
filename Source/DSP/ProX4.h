#pragma once

#include <array>
#include <cmath>
#include "UnitKit.h"

namespace enh::dsp
{
    /** LATINSPHIEL PRO X4 - a smart four-band tube enhancer with a PID controller.
        (Designed in the Rack Unit Designer by the owner; the sound built to its panel.)

        Each side (the left half of the panel for the left channel, the right half for the right) splits
        the sound into four bands (under 200 Hz, 200 Hz - 1 kHz, 1 - 5 kHz, over 5 kHz, Linkwitz-Riley,
        summing flat) and runs each through its own valve stage:
          - DRIVE (the big knob) - how hard the band's valve is driven;
          - TONE (the red pointer) - the band's level, -10 .. +10 dB: a four-band EQ around the valves;
          - MIX (the blue cap) - how much of the valve's harmonics are added to the band.
        MAIN, for both sides:
          - POPULATE - how dense the added harmonics are (0: none, 5: as the valves make them, 10: twice);
          - SATURATE - every valve driven harder or softer (-6 .. +12 dB);
          - WIDEN - the harmonics spread in stereo (the direct sound keeps its image);
          - CRISP - the top two bands' harmonics brought forward (clarity, never a treble lift).
        Switches: PWR (in), MONO (the left half's settings for both sides, and a mono output), X2 (the
        effect doubled), PID.

        Each valve is driven against its band's own level, so its colour is the same loud or quiet: DRIVE 40
        adds about -28 dB of harmonics (clearly there), 100 with SATURATE up about -10 dB (a lot).

        PID: the harmonic density of each band (its added harmonics against its own level, in dB) is the
        process variable; the DRIVE knob sets the target (0 .. 100 -> -40 .. -10 dB). The controller
        turns each band's drive up or down (24 dB at most either way) to hold it there, whatever the
        material: dense mixes, sparse ones, sustained or percussive. PROPORTIONAL (0 - 10), INTEGRAL (0 - 1) and
        DERIVATIVE (0 - 4) set how firmly, how persistently and how quickly it corrects.

        The output is matched in loudness to what came in (over ~1 s): the knobs change the colour, not
        the level. Anti-aliased valve curves, no latency, no allocation. Bit-for-bit out while PWR is off. */
    class ProX4
    {
    public:
        static constexpr int numBands = 4;

        struct Side
        {
            std::array<float, numBands> drive { 40.0f, 40.0f, 40.0f, 40.0f };   // 0 .. 100
            std::array<float, numBands> toneDb {};                              // -10 .. +10
            std::array<float, numBands> mix { 50.0f, 50.0f, 50.0f, 50.0f };     // 0 .. 100 %
        };
        struct Settings
        {
            bool power = false, mono = false, x2 = false, pid = false;
            float p = 1.0f, i = 0.1f, d = 0.4f;                              // PID: 0-10, 0-1, 0-4
            float populate = 5.0f, saturate = 5.0f, widen = 0.0f, crisp = 0.0f;   // 0 .. 10
            std::array<Side, 2> side {};
        };
        struct Readout
        {
            float densityDb = -120.0f;                          // the bands' harmonic density, averaged (the dB+ meter)
            std::array<float, numBands> pidDb {};               // what the PID adds to each band's drive
            std::array<float, numBands> pvDb {}, spDb {};       // process variable and setpoint per band
        };

        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            for (auto& s : split) s.setup (sr, 200.0, 1000.0, 5000.0);
            for (auto& c : level) for (auto& l : c) l.setup (sr, 0.15);
            matchK = 1.0f - (float) std::exp (-1.0 / (1.0 * sr));
            densK = 1.0f - (float) std::exp (-1.0 / (0.1 * sr));
            fadeStep = (float) (1.0 / (0.030 * sr));
            reset();
        }

        void reset() noexcept
        {
            for (auto& s : split) s.reset();
            for (auto& c : sat) for (auto& s : c) s.reset();
            for (auto& c : glides) for (auto& g : c) g.set (0.0f);
            for (auto& b : resPow) b = {};
            for (auto& b : sigPow) b = {};
            pidOut = {}; pidInt = {}; pidPrevErr = {};
            inPow = outPow = 1.0e-9f;
            matchGain = 1.0f;
            fade = 0.0f;
            primed = false;
            readout = {};
        }

        const Readout& getReadout() const noexcept { return readout; }

        void process (float* const* ch, int numChannels, int n, const Settings& s) noexcept
        {
            const float want = s.power ? 1.0f : 0.0f;
            if (numChannels < 1 || n <= 0 || (fade == 0.0f && want == 0.0f))
            {
                readout.densityDb = -120.0f;
                return;
            }
            if (fade == 0.0f)
                resetDsp();   // coming in: starts clean (the fade covers the start)

            updateKnobs (s, n);
            const int nc = std::min (numChannels, 2);
            const float populate = std::clamp (s.populate, 0.0f, 10.0f) / 5.0f * (s.x2 ? 2.0f : 1.0f);
            const float widen = std::clamp (s.widen, 0.0f, 10.0f) / 10.0f;
            const float crisp = 1.0f + 0.12f * std::clamp (s.crisp, 0.0f, 10.0f);
            // Loudness: as loud as what came in (over ~1 s; 12 dB at most either way), gliding across the block
            const float mgFrom = matchGain;
            if (inPow > 1.0e-9f && outPow > 1.0e-12f)
                matchGain = std::clamp (std::sqrt (inPow / outPow), 0.25f, 4.0f);
            const float mgStep = (matchGain - mgFrom) / (float) n;

            for (int i = 0; i < n; ++i)
            {
                fade = want > fade ? std::min (want, fade + fadeStep) : std::max (want, fade - fadeStep);
                std::array<float, 2> dry {}, direct {}, harm {};
                for (int c = 0; c < nc; ++c)
                {
                    const float x = ch[c][i];
                    dry[(size_t) c] = x;
                    float b[numBands];
                    split[(size_t) c].split (x, b);
                    const int k = s.mono ? 0 : c;   // MONO: the left half's knobs for both sides
                    for (int band = 0; band < numBands; ++band)
                    {
                        const float g = glides[(size_t) k][(size_t) band].at (i);                          // drive (gain)
                        const float tone = glides[(size_t) k][(size_t) (numBands + band)].at (i);          // band level (gain)
                        const float mix = glides[(size_t) k][(size_t) (2 * numBands + band)].at (i);       // 0 .. 1
                        const float v = b[band];
                        // (driven against the band's own level: the same colour loud or quiet)
                        const float lvl = level[(size_t) c][(size_t) band].process (v);
                        float res = kit::harmonicsAt (sat[(size_t) c][(size_t) band], v, lvl, g, 0.18f) * populate * mix;
                        if (band >= 2) res *= crisp;
                        if (band == 3) res *= 0.5f;   // (over 5 kHz its harmonics are all air: half, or pushed hard it fizzes)
                        direct[(size_t) c] += v * tone;
                        harm[(size_t) c] += res * tone;
                        resPow[(size_t) c][(size_t) band] += densK * (res * res - resPow[(size_t) c][(size_t) band]);
                        sigPow[(size_t) c][(size_t) band] += densK * (v * v - sigPow[(size_t) c][(size_t) band]);
                    }
                }
                if (nc == 2)
                {
                    // WIDEN: the harmonics' sides lifted, the direct sound's image untouched
                    const float m = 0.5f * (harm[0] + harm[1]), sd = 0.5f * (harm[0] - harm[1]) * (1.0f + 2.0f * widen);
                    harm[0] = m + sd;
                    harm[1] = m - sd;
                }
                for (int c = 0; c < nc; ++c)
                {
                    const float wet = direct[(size_t) c] + harm[(size_t) c];
                    inPow += matchK * (dry[(size_t) c] * dry[(size_t) c] - inPow);
                    outPow += matchK * (wet * wet - outPow);
                    dry[(size_t) c] = dry[(size_t) c] + fade * (wet * (mgFrom + mgStep * (float) (i + 1)) - dry[(size_t) c]);
                }
                if (s.mono && nc == 2)
                    dry[0] = dry[1] = 0.5f * (dry[0] + dry[1]);
                for (int c = 0; c < nc; ++c)
                    ch[c][i] = dry[(size_t) c];
            }

            primed = true;

            controlPid (s, (float) n / (float) sr);
        }

    private:
        void resetDsp() noexcept
        {
            for (auto& sp : split) sp.reset();
            for (auto& c : sat) for (auto& x : c) x.reset();
            for (auto& c : level) for (auto& l : c) l.reset();
            inPow = outPow = 1.0e-9f;
            matchGain = 1.0f;
        }

        void updateKnobs (const Settings& s, int n) noexcept
        {
            const float satDb = (std::clamp (s.saturate, 0.0f, 10.0f) - 5.0f) * (s.saturate > 5.0f ? 2.4f : 1.2f);
            for (int k = 0; k < 2; ++k)
                for (int band = 0; band < numBands; ++band)
                {
                    const auto& sd = s.side[(size_t) k];
                    const float driveDb = -12.0f + 0.30f * std::clamp (sd.drive[(size_t) band], 0.0f, 100.0f) + satDb
                                        + (s.pid ? pidOut[(size_t) band] : 0.0f);
                    auto target = [&] (int slot, float v)
                    {
                        auto& g = glides[(size_t) k][(size_t) slot];
                        if (! primed) g.set (v);
                        g.target (v, n);
                    };
                    target (band, dbToGain (std::clamp (driveDb, -30.0f, 36.0f)));
                    target (numBands + band, dbToGain (std::clamp (sd.toneDb[(size_t) band], -10.0f, 10.0f)));
                    target (2 * numBands + band, std::clamp (sd.mix[(size_t) band], 0.0f, 100.0f) / 100.0f);
                }
        }

        /** Per band (both sides together): density in dB against the DRIVE knob's target. */
        void controlPid (const Settings& s, float dt) noexcept
        {
            const float kp = 0.15f * std::clamp (s.p, 0.0f, 10.0f);
            const float ki = 3.0f * std::clamp (s.i, 0.0f, 1.0f);
            const float kd = 0.01f * std::clamp (s.d, 0.0f, 4.0f);
            float densSum = 0.0f;
            int densCount = 0;
            for (int band = 0; band < numBands; ++band)
            {
                const float r = resPow[0][(size_t) band] + resPow[1][(size_t) band];
                const float g = sigPow[0][(size_t) band] + sigPow[1][(size_t) band];
                const float pv = g > 1.0e-8f ? 10.0f * std::log10 ((r + 1.0e-14f) / g) : -120.0f;
                const float sp = -40.0f + 0.30f * std::clamp (s.side[0].drive[(size_t) band], 0.0f, 100.0f);
                readout.pvDb[(size_t) band] = pv;
                readout.spDb[(size_t) band] = sp;
                if (g > 1.0e-8f) { densSum += pv; ++densCount; }
                if (! s.pid || g <= 1.0e-8f || dt <= 0.0f)
                {
                    if (! s.pid) { pidOut[(size_t) band] *= 0.98f; pidInt[(size_t) band] *= 0.98f; }
                    readout.pidDb[(size_t) band] = pidOut[(size_t) band];
                    continue;
                }
                const float err = std::clamp (sp - pv, -30.0f, 30.0f);
                pidInt[(size_t) band] = std::clamp (pidInt[(size_t) band] + ki * err * dt, -24.0f, 24.0f);   // (anti-windup)
                const float deriv = (err - pidPrevErr[(size_t) band]) / dt;
                pidPrevErr[(size_t) band] = err;
                const float out = std::clamp (kp * err + pidInt[(size_t) band] + kd * deriv, -24.0f, 24.0f);
                // The drive moves at most 30 dB/s: the controller steers, it never jumps
                pidOut[(size_t) band] += std::clamp (out - pidOut[(size_t) band], -30.0f * dt, 30.0f * dt);
                readout.pidDb[(size_t) band] = pidOut[(size_t) band];
            }
            readout.densityDb = densCount > 0 ? densSum / (float) densCount : -120.0f;
        }

        double sr = 48000.0;
        std::array<kit::Split4, 2> split {};
        std::array<std::array<kit::SoftSat, numBands>, 2> sat {};
        std::array<std::array<kit::Level, numBands>, 2> level {};
        std::array<std::array<kit::Glide, 3 * numBands>, 2> glides {};   // per side: drive, tone, mix per band
        std::array<std::array<float, numBands>, 2> resPow {}, sigPow {};
        std::array<float, numBands> pidOut {}, pidInt {}, pidPrevErr {};
        float inPow = 1.0e-9f, outPow = 1.0e-9f, matchGain = 1.0f, matchK = 0.0f, densK = 0.0f;
        float fade = 0.0f, fadeStep = 0.0f;
        bool primed = false;
        Readout readout;
    };
}
