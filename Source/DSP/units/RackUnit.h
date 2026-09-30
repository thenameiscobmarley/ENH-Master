#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>
#include "../DspMath.h"
#include "../UnitKit.h"

/*  The rack's newer units share this: a unit only writes its sound (render); the base looks after the rest -
      - POWER (its first parameter): off, it is bit-for-bit out; on and off it fades over 30 ms, no click;
      - a guard: anything not a number resets the unit and lets the dry sound through for that block;
      - the ears: its output is held under +6 dBFS peaks by a soft ceiling (the rack's own limiter follows).
    Parameters come as the raw values of its rows in DesignedUnits.h (their own units), p[0] = POWER.
    Each unit is made once, on the heap (the engine lives on callers' stacks in the tests). */
namespace enh::dsp::units
{
    class RackUnit
    {
    public:
        virtual ~RackUnit() = default;

        void prepare (double sampleRate, int maxBlock)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            for (auto& d : dry) d.assign ((size_t) std::max (1, maxBlock), 0.0f);
            mono.assign ((size_t) std::max (1, maxBlock), 0.0f);
            fadeStep = (float) (1.0 / (0.030 * sr));
            prepareUnit (sr, std::max (1, maxBlock));
            resetAll();
        }

        void resetAll() { fade = 0.0f; meterValue = 0.0f; resetUnit(); }

        void process (float* const* ch, int numChannels, int n, const float* p) noexcept
        {
            const float want = p[0] > 0.5f ? 1.0f : 0.0f;
            const int nc = std::min (numChannels, 2);
            if (nc < 1 || n <= 0 || (fade == 0.0f && want == 0.0f) || n > (int) dry[0].size())
            {
                if (want == 0.0f) meterValue = 0.0f;
                return;
            }
            if (fade == 0.0f)
                resetUnit();   // coming in: from a clean state (the fade covers the start)
            for (int c = 0; c < nc; ++c)
                std::copy_n (ch[c], n, dry[(size_t) c].data());
            // A mono host: the unit still sees a stereo pair (the same on both sides)
            float* io[2] { ch[0], nc > 1 ? ch[1] : mono.data() };
            if (nc == 1) std::copy_n (ch[0], n, mono.data());   // (sized in prepare: nothing allocated here)
            render (io, n, p);
            bool finite = true;
            for (int c = 0; c < 2 && finite; ++c)
                for (int i = 0; i < n; ++i)
                    if (! std::isfinite (io[c][i])) { finite = false; break; }
            if (! finite)
            {
                resetUnit();
                for (int c = 0; c < nc; ++c) std::copy_n (dry[(size_t) c].data(), n, ch[c]);
                return;
            }
            for (int i = 0; i < n; ++i)
            {
                fade = want > fade ? std::min (want, fade + fadeStep) : std::max (want, fade - fadeStep);
                for (int c = 0; c < nc; ++c)
                {
                    const float d = dry[(size_t) c][(size_t) i];
                    float y = io[c][i];
                    const float a = std::abs (y);
                    if (a > 1.5f) y = std::copysign (1.5f + 0.5f * std::tanh ((a - 1.5f) / 0.5f), y);   // (the ceiling: +6 dBFS, soft)
                    ch[c][i] = d + fade * (y - d);
                }
            }
        }

        /** How hard it is working, 0 .. 1 (its display and LEDs). */
        float meter() const noexcept { return meterValue; }
        /** A unit with a picture to show (RAY ROOM's room) writes its state here after process (audio thread);
            returns how many floats (0: none). */
        virtual int displayState (float*, int) const noexcept { return 0; }

    protected:
        virtual void prepareUnit (double sampleRate, int maxBlock) = 0;
        virtual void resetUnit() = 0;
        /** The unit's sound, in place, on a stereo pair; `p` its parameters (p[0] POWER). */
        virtual void render (float* const* io, int n, const float* p) noexcept = 0;

        double sr = 48000.0;
        float meterValue = 0.0f;
        void setMeter (float v) noexcept { meterValue += 0.3f * (std::clamp (v, 0.0f, 1.0f) - meterValue); }

    private:
        std::array<std::vector<float>, 2> dry {};
        std::vector<float> mono;
        float fade = 0.0f, fadeStep = 0.0f;
    };

    // --- small shared pieces --------------------------------------------------------------------------
    /** A fractional delay line (cubic interpolation), power-of-two sized, on the heap. */
    struct DelayLine
    {
        std::vector<float> buf; int mask = 0, w = 0;
        void setMax (int samples) { int s = 1; while (s < samples + 4) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
        float tap (float d) const noexcept   // d samples ago (>= 1)
        {
            d = std::clamp (d, 1.0f, (float) mask - 3.0f);
            const float pos = (float) w - d; const int i = (int) std::floor (pos); const float f = pos - (float) i;
            auto at = [&] (int k) { return buf[(size_t) ((i + k) & mask)]; };
            const float y0 = at (-1), y1 = at (0), y2 = at (1), y3 = at (2);
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
    };

    /** A one-pole low-pass (a smoother, a damping filter). */
    struct OnePole { float z = 0.0f; float process (float x, float k) noexcept { z += k * (x - z); return z; } };
    inline float onePoleK (double sr, double hz) { return 1.0f - (float) std::exp (-2.0 * 3.141592653589793 * hz / sr); }

    /** A slowly wandering LFO (sine), phase 0..1. */
    struct Lfo { double ph = 0.0; float next (double hz, double sr) noexcept { ph += hz / sr; ph -= std::floor (ph); return (float) std::sin (6.283185307179586 * ph); } };

    /** First-order allpass (a phase shift): y = -g x + x1 + g y1. */
    struct Allpass1 { float x1 = 0, y1 = 0; float process (float x, float g) noexcept { const float y = -g * x + x1 + g * y1; x1 = x; y1 = y; return y; } };

    inline float dbToGainF (float db) noexcept { return std::pow (10.0f, db / 20.0f); }
}
