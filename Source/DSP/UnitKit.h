#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include "DspMath.h"

/*  Building blocks shared by the units made in the designer (PRO X4, VELVETIZER): band splits that sum
    flat, an anti-aliased saturator, and a gliding value for knobs. */
namespace enh::dsp::kit
{
    /** Linkwitz-Riley 4th order: two cascaded Butterworth sections each side (see SpectralLeveler.h). */
    struct Lr4
    {
        SvfState lp1, lp2, hp1, hp2;
        void reset() noexcept { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
        float low (const SvfCoeffs& c, float x) noexcept  { return lp2.process (c, lp1.process (c, x).low).low; }
        float high (const SvfCoeffs& c, float x) noexcept { return hp2.process (c, hp1.process (c, x).high).high; }
    };

    /** The 2nd-order allpass an LR4 split adds, for the branch that skips that split. */
    struct Allpass
    {
        SvfState st;
        void reset() noexcept { st.reset(); }
        float process (const SvfCoeffs& c, float x) noexcept { return x - 2.0f * st.process (c, x).band; }
    };

    /** Three bands (f1 < f2) summing flat: split at f2; the low half splits again at f1, the high half
        goes through f1's allpass so all three share one phase. */
    struct Split3
    {
        SvfCoeffs c1, c2;
        Lr4 s2, s1;
        Allpass ap1;
        void setup (double sr, double f1, double f2) noexcept { c1 = SvfCoeffs::make (sr, f1, 0.70710678); c2 = SvfCoeffs::make (sr, f2, 0.70710678); }
        void reset() noexcept { s2.reset(); s1.reset(); ap1.reset(); }
        void split (float x, float* b) noexcept
        {
            const float lo = s2.low (c2, x), hi = s2.high (c2, x);
            b[0] = s1.low (c1, lo);
            b[1] = s1.high (c1, lo);
            b[2] = ap1.process (c1, hi);
        }
    };

    /** Four bands (f1 < f2 < f3) summing flat. */
    struct Split4
    {
        SvfCoeffs c1, c2, c3;
        Lr4 s2, s1, s3;
        Allpass ap1, ap3;
        void setup (double sr, double f1, double f2, double f3) noexcept
        {
            c1 = SvfCoeffs::make (sr, f1, 0.70710678); c2 = SvfCoeffs::make (sr, f2, 0.70710678); c3 = SvfCoeffs::make (sr, f3, 0.70710678);
        }
        void reset() noexcept { s2.reset(); s1.reset(); s3.reset(); ap1.reset(); ap3.reset(); }
        void split (float x, float* b) noexcept
        {
            const float lo = ap3.process (c3, s2.low (c2, x)), hi = ap1.process (c1, s2.high (c2, x));
            b[0] = s1.low (c1, lo);
            b[1] = s1.high (c1, lo);
            b[2] = s3.low (c3, hi);
            b[3] = s3.high (c3, hi);
        }
    };

    /** A soft saturator (the cubic curve: linear at small levels, flat at +-2/3) with first-order
        antiderivative anti-aliasing: the difference of the curve's integral over the step, so harmonics
        above half the sample rate fold back far less than a plain waveshaper's. `bias` tilts it (even
        harmonics, a valve's asymmetry); the curve's own offset at rest is taken off. */
    struct SoftSat
    {
        float xPrev = 0.0f, fPrev = 0.0f, uPrev = 0.0f;
        void reset() noexcept { xPrev = 0.0f; fPrev = 0.0f; uPrev = 0.0f; }

        static float curve (float x) noexcept { return std::abs (x) < 1.0f ? x - x * x * x / 3.0f : (x > 0.0f ? 2.0f / 3.0f : -2.0f / 3.0f); }
        static float integral (float x) noexcept
        {
            const float a = std::abs (x);
            return a < 1.0f ? x * x * 0.5f - x * x * x * x / 12.0f : a * (2.0f / 3.0f) - 0.25f;
        }

        /** y ~ curve (x + bias) - curve (bias), anti-aliased. */
        float process (float x, float bias) noexcept
        {
            const float u = x + bias, d = u - xPrev;
            const float fu = integral (u);
            const float y = std::abs (d) > 1.0e-5f ? (fu - fPrev) / d : curve (0.5f * (u + xPrev));
            xPrev = u;
            fPrev = fu;
            return y - curve (bias);
        }

        /** Only what the curve adds (its harmonics): the anti-aliasing averages over the step, so even its
            straight part comes out as (x + previous x) / 2 - half a sample late. Taking that off leaves
            the non-linear part alone; measured against x itself, that half-sample step read as "harmonics"
            (level-independent, a treble tilt) and the PRO X4's PID steered on it. */
        float residual (float x, float bias) noexcept
        {
            const float y = process (x, bias);
            // (the curve's slope where the bias sets it working: 1 - bias^2 - not 1, which read as 3 %
            // of the signal itself counted as harmonics at a 0.18 bias)
            const float slope = std::abs (bias) < 1.0f ? 1.0f - bias * bias : 0.0f;
            const float r = y - slope * 0.5f * (x + uPrev);
            uPrev = x;
            return r;
        }
    };

    /** A signal's level, as a sine's peak would read it (RMS x 1.41): it rises in about a millisecond (so an
        attack never finds the saturator driven for a quiet level and gets crushed) and falls over `seconds`.
        What the saturators below are driven against, so their colour is the same at any volume. */
    struct Level
    {
        float ms = 0.0f, up = 0.0f, down = 0.0f;
        void setup (double sr, double seconds) noexcept
        {
            up = 1.0f - (float) std::exp (-1.0 / (0.001 * sr));
            down = 1.0f - (float) std::exp (-1.0 / (seconds * sr));
        }
        void reset() noexcept { ms = 0.0f; }
        float process (float x) noexcept { const float p = x * x; ms += (p > ms ? up : down) * (p - ms); return 1.41421356f * std::sqrt (ms); }
    };

    /** The harmonics a saturator adds to `x`, driven relative to its level (`level`, from Level): `drive` 1
        puts a sine's peaks at the curve's knee - about -22 dB of harmonics - whatever the volume. The
        result scales back with the level. Quieter than -80 dBFS nothing is driven (no noise-floor fizz). */
    inline float harmonicsAt (SoftSat& sat, float x, float level, float drive, float bias) noexcept
    {
        const float l = std::max (level, 1.0e-4f);
        return l * sat.residual (x / l * drive, bias) / drive;
    }

    /** A knob's value gliding to where it was set, across each block (no zipper noise). */
    struct Glide
    {
        float now = 0.0f, from = 0.0f, step = 0.0f;
        void set (float v) noexcept { now = from = v; step = 0.0f; }
        void target (float v, int n) noexcept { from = now; step = n > 0 ? (v - now) / (float) n : 0.0f; now = v; }
        float at (int i) const noexcept { return from + step * (float) (i + 1); }
    };
}
