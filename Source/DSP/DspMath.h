#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>

namespace enh::dsp
{
    inline constexpr double pi = 3.14159265358979323846;

    inline float dbToGain (float db) noexcept      { return std::pow (10.0f, db * 0.05f); }
    inline float powerToDb (float p) noexcept      { return 10.0f * std::log10 (p + 1.0e-12f); }
    inline float saturate01 (float x) noexcept     { return std::clamp (x, 0.0f, 1.0f); }

    /** One-pole coefficient for a time constant (seconds) at a given update rate. */
    inline float onePole (double seconds, double rate) noexcept
    {
        return seconds <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (seconds * rate));
    }

    /** 0 below the knee, quadratic through it, then linear. */
    inline float softRamp (float x, float knee) noexcept
    {
        const float h = 0.5f * knee;
        if (x <= -h) return 0.0f;
        if (x >=  h) return x;
        const float t = x + h;
        return t * t / (2.0f * knee);
    }

    /** Gaussian bump in octaves around a centre frequency. */
    inline float octaveBell (double hz, double centreHz, double widthOctaves) noexcept
    {
        const double d = std::log2 (hz / centreHz) / widthOctaves;
        return (float) std::exp (-d * d);
    }

    //==============================================================================
    struct BiquadCoeffs
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

        static BiquadCoeffs normalised (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
        {
            return { (float) (b0 / a0), (float) (b1 / a0), (float) (b2 / a0), (float) (a1 / a0), (float) (a2 / a0) };
        }

        static double clampHz (double sr, double hz) noexcept { return std::clamp (hz, 5.0, 0.49 * sr); }

        static BiquadCoeffs lowPass (double sr, double hz, double q) noexcept
        {
            const double w = 2.0 * pi * clampHz (sr, hz) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
            return normalised ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + a, -2 * c, 1 - a);
        }

        static BiquadCoeffs highPass (double sr, double hz, double q) noexcept
        {
            const double w = 2.0 * pi * clampHz (sr, hz) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
            return normalised ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + a, -2 * c, 1 - a);
        }

        /** Band-pass with 0 dB peak gain. */
        static BiquadCoeffs bandPass (double sr, double hz, double q) noexcept
        {
            const double w = 2.0 * pi * clampHz (sr, hz) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
            return normalised (a, 0.0, -a, 1 + a, -2 * c, 1 - a);
        }

        static BiquadCoeffs peaking (double sr, double hz, double q, double db) noexcept
        {
            const double A = std::pow (10.0, db / 40.0);
            const double w = 2.0 * pi * clampHz (sr, hz) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
            return normalised (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
        }

        static BiquadCoeffs lowShelf (double sr, double hz, double q, double db) noexcept
        {
            const double A = std::pow (10.0, db / 40.0), sA = std::sqrt (A);
            const double w = 2.0 * pi * clampHz (sr, hz) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
            return normalised (A * ((A + 1) - (A - 1) * c + 2 * sA * a),
                               2 * A * ((A - 1) - (A + 1) * c),
                               A * ((A + 1) - (A - 1) * c - 2 * sA * a),
                               (A + 1) + (A - 1) * c + 2 * sA * a,
                               -2 * ((A - 1) + (A + 1) * c),
                               (A + 1) + (A - 1) * c - 2 * sA * a);
        }

        static BiquadCoeffs highShelf (double sr, double hz, double q, double db) noexcept
        {
            const double A = std::pow (10.0, db / 40.0), sA = std::sqrt (A);
            const double w = 2.0 * pi * clampHz (sr, hz) / sr, c = std::cos (w), a = std::sin (w) / (2.0 * q);
            return normalised (A * ((A + 1) + (A - 1) * c + 2 * sA * a),
                               -2 * A * ((A - 1) + (A + 1) * c),
                               A * ((A + 1) + (A - 1) * c - 2 * sA * a),
                               (A + 1) - (A - 1) * c + 2 * sA * a,
                               2 * ((A - 1) - (A + 1) * c),
                               (A + 1) - (A - 1) * c - 2 * sA * a);
        }
    };

    /** Transposed direct form II state. */
    struct BiquadState
    {
        float z1 = 0.0f, z2 = 0.0f;

        inline float process (const BiquadCoeffs& c, float x) noexcept
        {
            const float y = c.b0 * x + z1;
            z1 = c.b1 * x - c.a1 * y + z2;
            z2 = c.b2 * x - c.a2 * y;
            return y;
        }

        void reset() noexcept { z1 = z2 = 0.0f; }
    };

    /** Peaking EQ whose centre/Q are fixed and only gain changes: cheap per-tick redesign. */
    struct PeakingDesigner
    {
        double cosW = 1.0, alpha = 0.0;

        void setup (double sr, double hz, double q) noexcept
        {
            const double w = 2.0 * pi * BiquadCoeffs::clampHz (sr, hz) / sr;
            cosW = std::cos (w);
            alpha = std::sin (w) / (2.0 * q);
        }

        BiquadCoeffs make (float db) const noexcept
        {
            const double A = std::pow (10.0, (double) db / 40.0);
            return BiquadCoeffs::normalised (1 + alpha * A, -2 * cosW, 1 - alpha * A, 1 + alpha / A, -2 * cosW, 1 - alpha / A);
        }
    };

    /** An EQ on a TPT state-variable filter (Cytomic's "linear trap" SVF): a bell or a high shelf whose
        gain can move from block to block without the one-sample spikes a direct-form biquad makes when
        its coefficients change under it (its state is voltage-like, so it stays valid). The same
        analogue prototypes as the RBJ bell and shelf. */
    struct SvfEqCoeffs
    {
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;

        static SvfEqCoeffs bell (double sr, double hz, double q, double db) noexcept
        {
            const double A = std::pow (10.0, db / 40.0);
            const double g = std::tan (pi * BiquadCoeffs::clampHz (sr, hz) / sr), k = 1.0 / (q * A);
            const double a1 = 1.0 / (1.0 + g * (g + k));
            return { (float) a1, (float) (g * a1), (float) (g * g * a1), 1.0f, (float) (k * (A * A - 1.0)), 0.0f };
        }

        static SvfEqCoeffs highShelf (double sr, double hz, double q, double db) noexcept
        {
            const double A = std::pow (10.0, db / 40.0);
            const double g = std::tan (pi * BiquadCoeffs::clampHz (sr, hz) / sr) * std::sqrt (A), k = 1.0 / q;
            const double a1 = 1.0 / (1.0 + g * (g + k));
            return { (float) a1, (float) (g * a1), (float) (g * g * a1), (float) (A * A), (float) (k * (1.0 - A) * A), (float) (1.0 - A * A) };
        }

        static SvfEqCoeffs lowShelf (double sr, double hz, double q, double db) noexcept
        {
            const double A = std::pow (10.0, db / 40.0);
            const double g = std::tan (pi * BiquadCoeffs::clampHz (sr, hz) / sr) / std::sqrt (A), k = 1.0 / q;
            const double a1 = 1.0 / (1.0 + g * (g + k));
            return { (float) a1, (float) (g * a1), (float) (g * g * a1), 1.0f, (float) (k * (A - 1.0)), (float) (A * A - 1.0) };
        }
    };

    struct SvfEqState
    {
        float ic1 = 0.0f, ic2 = 0.0f;

        inline float process (const SvfEqCoeffs& c, float v0) noexcept
        {
            const float v3 = v0 - ic2;
            const float v1 = c.a1 * ic1 + c.a2 * v3;
            const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
        }

        void reset() noexcept { ic1 = ic2 = 0.0f; }
    };

    /** Topology-preserving-transform state-variable filter (Cytomic / Zavalishin).
        Stays clean while its frequency moves, so it is used wherever the centre is adaptive. */
    struct SvfCoeffs
    {
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 1.4142f;

        static SvfCoeffs make (double sr, double hz, double q) noexcept
        {
            const double g = std::tan (pi * BiquadCoeffs::clampHz (sr, hz) / sr);
            const double kk = 1.0 / q;
            const double a1 = 1.0 / (1.0 + g * (g + kk));
            return { (float) a1, (float) (g * a1), (float) (g * g * a1), (float) kk };
        }
    };

    struct SvfState
    {
        float ic1 = 0.0f, ic2 = 0.0f;

        struct Out { float low, band, high; };   // band is normalised to 0 dB peak

        inline Out process (const SvfCoeffs& c, float v0) noexcept
        {
            const float v3 = v0 - ic2;
            const float v1 = c.a1 * ic1 + c.a2 * v3;
            const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return { v2, c.k * v1, v0 - c.k * v1 - v2 };
        }

        void reset() noexcept { ic1 = ic2 = 0.0f; }
    };

    /** Attack/release follower of signal power. */
    struct PowerFollower
    {
        float attack = 0.0f, release = 0.0f, env = 0.0f;

        void setup (double rate, double attackSeconds, double releaseSeconds) noexcept
        {
            attack = onePole (attackSeconds, rate);
            release = onePole (releaseSeconds, rate);
        }

        inline void push (float power) noexcept
        {
            const float k = power > env ? attack : release;
            env = k * env + (1.0f - k) * power;
        }

        float db() const noexcept { return powerToDb (env); }
    };

    /** N biquads fed the same input, stored band by band in plain arrays so the compiler runs several
        bands per instruction. Same arithmetic in the same order as BiquadState::process, so the
        results are bit-identical - it is only faster. */
    template <int N>
    struct BiquadBank
    {
        alignas (16) std::array<float, N> b0 {}, b1 {}, b2 {}, a1 {}, a2 {}, z1 {}, z2 {};

        void set (int k, const BiquadCoeffs& c) noexcept
        {
            const auto i = (size_t) k;
            b0[i] = c.b0; b1[i] = c.b1; b2[i] = c.b2; a1[i] = c.a1; a2[i] = c.a2;
        }

        void reset() noexcept { z1.fill (0.0f); z2.fill (0.0f); }

        /** y[k] = band k's output for input x, for the first `count` bands. */
        inline void process (float x, float* __restrict y, int count) noexcept
        {
            float* __restrict s1 = z1.data();
            float* __restrict s2 = z2.data();
            for (int k = 0; k < count; ++k)
            {
                const float o = b0[(size_t) k] * x + s1[k];
                s1[k] = b1[(size_t) k] * x - a1[(size_t) k] * o + s2[k];
                s2[k] = b2[(size_t) k] * x - a2[(size_t) k] * o;
                y[k] = o;
            }
        }
    };

    /** N PowerFollowers in plain arrays (vectorisable; same arithmetic as PowerFollower::push). */
    template <int N>
    struct FollowerBank
    {
        alignas (16) std::array<float, N> attack {}, release {}, env {};

        void setup (int k, double rate, double attackSeconds, double releaseSeconds) noexcept
        {
            attack[(size_t) k] = onePole (attackSeconds, rate);
            release[(size_t) k] = onePole (releaseSeconds, rate);
        }

        void reset() noexcept { env.fill (0.0f); }

        inline void push (const float* __restrict power, int count) noexcept
        {
            float* __restrict e = env.data();
            for (int k = 0; k < count; ++k)
            {
                const float kk = power[k] > e[k] ? attack[(size_t) k] : release[(size_t) k];
                e[k] = kk * e[k] + (1.0f - kk) * power[k];
            }
        }

        float db (int k) const noexcept { return powerToDb (env[(size_t) k]); }
    };
}
