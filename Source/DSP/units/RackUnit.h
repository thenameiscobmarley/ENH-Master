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
      - the ears: its output is held under +6 dBFS peaks by a soft ceiling (the rack's own limiter follows);
      - its knobs glide: a knob moved (automation jumping end to end, a preset) eases to where it was set over
        about 80 ms, the unit rendering 4 samples at a time while it does - no zipper, no click; with every knob
        where it was set, a block renders in one piece, as it always did (setParamCount: how many it has; 0: off).
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
            for (auto& d : lateDry) d.assign ((size_t) std::max (1, maxLatencySamples()), 0.0f);
            for (auto& d : dryLate) d.assign ((size_t) std::max (1, maxBlock), 0.0f);
            resetAll();
        }

        void resetAll() { fade = 0.0f; meterValue = 0.0f; primed = false; clearLateDry(); resetUnit(); }
        /** A unit that needs time to listen ahead (VOCAL IDENTITY PROCESSOR) delays its sound by this many samples;
            the base delays the dry sound it fades and blends with by the same. 0: none (every other unit). */
        virtual int latencySamples() const noexcept { return 0; }
        /** The most it can ever need (sizes the dry delay once, in prepare). */
        virtual int maxLatencySamples() const noexcept { return 0; }
        /** Its latency as the rack sees it: only while it is on (off, it is bit-for-bit out, no delay). */
        int activeLatency() const noexcept { return poweredOn ? latencySamples() : 0; }
        /** How many parameters it reads (its rows in DesignedUnits.h): the ones that glide. */
        void setParamCount (int n) { last.assign ((size_t) std::max (0, n), 0.0f); now.assign (last.size(), 0.0f); primed = false; }
        /** MULTIPLY and STRENGTH (this release's new units): where the two knobs are among its parameters, and which of
            its knobs MULTIPLY scales (from their low end - or their middle, either side of 0 - as far as their printed end) - its amounts, never its mix,
            its gain or a frequency or a time. STRENGTH (0 - 200 %) is how much of its change to the sound is kept:
            under 100 % less of it, over 100 % more. (Message thread, before it runs.) */
        void setModifiers (int multiplyAt, int strengthAt, std::vector<unsigned char> scaled, std::vector<float> lows, std::vector<float> highs)
        {
            modMultiply = multiplyAt; modStrength = strengthAt;
            modScaled = std::move (scaled); modLo = std::move (lows); modHi = std::move (highs);
            modParams.assign (modScaled.size(), 0.0f);
        }

        void process (float* const* ch, int numChannels, int n, const float* p) noexcept
        {
            const float want = p[0] > 0.5f ? 1.0f : 0.0f;
            poweredOn = want > 0.0f;
            const int nc = std::min (numChannels, 2);
            if (nc < 1 || n <= 0 || (fade == 0.0f && want == 0.0f) || n > (int) dry[0].size())
            {
                if (want == 0.0f) meterValue = 0.0f;
                return;
            }
            if (fade == 0.0f)
            {
                resetUnit();   // coming in: from a clean state (the fade covers the start)
                clearLateDry();
            }
            for (int c = 0; c < nc; ++c)
                std::copy_n (ch[c], n, dry[(size_t) c].data());
            const int lat = std::min (latencySamples(), (int) lateDry[0].size());
            if (lat > 0)
            {
                // (the dry STRENGTH blends with, as late as its sound; POWER still fades from the dry as it is now)
                if (lat != lateLen) { clearLateDry(); lateLen = lat; }
                for (int c = 0; c < nc; ++c)
                    for (int i = 0, pos = latePos; i < n; ++i, pos = pos + 1 == lat ? 0 : pos + 1)
                    {
                        dryLate[(size_t) c][(size_t) i] = lateDry[(size_t) c][(size_t) pos];
                        lateDry[(size_t) c][(size_t) pos] = dry[(size_t) c][(size_t) i];
                    }
                latePos = (int) ((latePos + n) % lat);
            }
            // A mono host: the unit still sees a stereo pair (the same on both sides)
            float* io[2] { ch[0], nc > 1 ? ch[1] : mono.data() };
            if (nc == 1) std::copy_n (ch[0], n, mono.data());   // (sized in prepare: nothing allocated here)
            const float* q = p;
            if (modMultiply >= 0 && modParams.size() == last.size())
            {
                // MULTIPLY: its amounts scaled before the unit sees them
                const float mul = std::clamp (p[modMultiply], 0.25f, 3.0f);
                for (size_t i = 0; i < modParams.size(); ++i)
                {
                    // (from its low end; a knob either side of 0 - TONE, TILT - from its middle)
                    const float pivot = modLo[i] < 0.0f && modHi[i] > 0.0f ? 0.0f : modLo[i];
                    modParams[i] = modScaled[i] ? std::clamp (pivot + (p[i] - pivot) * mul, modLo[i], modHi[i]) : p[i];
                }
                q = modParams.data();
            }
            const float strengthWant = modStrength >= 0 && ! strengthInside() ? std::clamp (p[modStrength] * 0.01f, 0.0f, 2.0f) : 1.0f;
            renderGliding (io, n, q);
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
            const float strengthK = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));
            for (int i = 0; i < n; ++i)
            {
                fade = want > fade ? std::min (want, fade + fadeStep) : std::max (want, fade - fadeStep);
                strengthNow += strengthK * (strengthWant - strengthNow);   // (STRENGTH glides: no click)
                for (int c = 0; c < nc; ++c)
                {
                    const float d = dry[(size_t) c][(size_t) i], dl = lat > 0 ? dryLate[(size_t) c][(size_t) i] : d;
                    float y = dl + strengthNow * (io[c][i] - dl);   // STRENGTH: how much of its change is kept
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
        /** A unit that applies STRENGTH itself (reads it among its parameters) instead of the base's blend
            (VOCAL IDENTITY PROCESSOR: a pitch can't be blended with the dry, only moved less or more). */
        virtual bool strengthInside() const noexcept { return false; }

    protected:
        virtual void prepareUnit (double sampleRate, int maxBlock) = 0;
        virtual void resetUnit() = 0;
        /** The unit's sound, in place, on a stereo pair; `p` its parameters (p[0] POWER). */
        virtual void render (float* const* io, int n, const float* p) noexcept = 0;

        double sr = 48000.0;
        float meterValue = 0.0f;
        void setMeter (float v) noexcept { meterValue += 0.3f * (std::clamp (v, 0.0f, 1.0f) - meterValue); }

    private:
        std::vector<float> last, now; bool primed = false;   // (last: kept for its size; now: the values rendered)
        void renderGliding (float* const* io, int n, const float* p) noexcept
        {
            const size_t count = last.size();
            bool moving = false;
            if (primed)
                for (size_t i = 1; i < count && ! moving; ++i)
                    moving = std::abs (p[i] - now[i]) > 1.0e-6f * std::max (1.0f, std::abs (p[i]));
            if (! primed || ! moving)
            {
                for (size_t i = 0; i < count; ++i) now[i] = p[i];
                render (io, n, p);
                primed = count > 0;
                return;
            }
            // gliding: each knob eases toward where it was set (a time constant of 25 ms - there in about 80 ms),
            // the unit rendering 4 samples at a time meanwhile (steps too small to hear, even a 24 dB gain move);
            // it carries on across blocks until every knob has arrived, then renders whole blocks again
            constexpr int slice = 4;
            const float a = 1.0f - std::exp (-(float) slice / (0.025f * (float) sr));
            now[0] = p[0];   // (POWER fades on its own)
            for (int from = 0; from < n; from += slice)
            {
                const int m = std::min (slice, n - from);
                for (size_t i = 1; i < count; ++i)
                {
                    const float d = p[i] - now[i];
                    now[i] = std::abs (d) < 1.0e-4f * std::max (1.0f, std::abs (p[i])) ? p[i] : now[i] + a * d;
                }
                float* part[2] { io[0] + from, io[1] + from };
                render (part, m, now.data());
            }
        }
        std::array<std::vector<float>, 2> dry {};
        std::vector<float> mono;
        int modMultiply = -1, modStrength = -1;
        std::vector<unsigned char> modScaled;
        std::vector<float> modLo, modHi, modParams;
        float strengthNow = 1.0f;
        float fade = 0.0f, fadeStep = 0.0f;
        bool poweredOn = false;
        std::array<std::vector<float>, 2> lateDry {}, dryLate {};
        int latePos = 0, lateLen = 0;
        void clearLateDry() noexcept { for (auto& d : lateDry) std::fill (d.begin(), d.end(), 0.0f); latePos = 0; }
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
