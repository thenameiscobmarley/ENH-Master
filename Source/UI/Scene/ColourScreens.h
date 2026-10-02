#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

/*  The colour screens (CHROMA SPACE, HYPERCUBE: DSP/units/Sims4.h). Every other screen in the rack is one
    channel of glow tinted by its shader; these two are drawn in colour, as light: each line is splatted into
    a float buffer additively (where lines cross they bloom), the buffer fades rather than clears (trails, as a
    phosphor's), and it is tone-mapped to 8 bits for the texture (the 3D rack's colourScreen material) or an
    image (the 2D rack). Cheap: a few thousand short splats a frame, no paths. */
namespace pad::colourscreen
{
    enum class Kind { none, chroma, hypercube, tuner };
    inline Kind kindOf (std::string_view key) noexcept { return key == "chroma" ? Kind::chroma : key == "hypercube" ? Kind::hypercube : key == "tuner" ? Kind::tuner : Kind::none; }
    inline const char* powerId (Kind k) noexcept { return k == Kind::chroma ? "chPower" : k == Kind::hypercube ? "hcPower" : k == Kind::tuner ? "tnPower" : ""; }

    struct Rgb { float r, g, b; };
    inline Rgb hsv (float h, float s, float v) noexcept
    {
        h = (h - std::floor (h)) * 6.0f;
        const int i = (int) h; const float f = h - (float) i, p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
        switch (i % 6) { case 0: return { v, t, p }; case 1: return { q, v, p }; case 2: return { p, v, t }; case 3: return { p, q, v }; case 4: return { t, p, v }; default: return { v, p, q }; }
    }
    /** A colour from a hue (0..1, the note) and a warmth (0 warm .. 1 cool): the warm half of the wheel (reds,
        ambers, golds) or the cool half (cyans, blues, violets), the note choosing where in it. */
    inline Rgb warmCool (float hue, float warmth, float value, float sat = 0.85f) noexcept
    {
        const float warmHue = 0.97f + 0.16f * hue, coolHue = 0.46f + 0.30f * hue;
        return hsv (warmHue + (coolHue - warmHue) * std::clamp (warmth, 0.0f, 1.0f), sat, value);
    }

    class Canvas
    {
    public:
        int w = 0, h = 0;
        std::vector<float> buf;               // linear light, rgb
        std::vector<std::uint8_t> rgba;       // tone-mapped, for the texture / image

        void resize (int width, int height) { if (width == w && height == h) return; w = width; h = height; buf.assign ((size_t) (w * h * 3), 0.0f); rgba.assign ((size_t) (w * h * 4), 0); }
        /** Everything dimmed toward black: `keep` of it stays (0 clears). */
        void fade (float keep) noexcept { for (auto& v : buf) v *= keep; }
        void dot (int x, int y, Rgb c, float a) noexcept
        {
            if (x < 0 || y < 0 || x >= w || y >= h) return;
            float* p = &buf[(size_t) ((y * w + x) * 3)];
            p[0] += c.r * a; p[1] += c.g * a; p[2] += c.b * a;
        }
        /** A soft point: a bright core, a glow round it (`size` about the core's radius in pixels). */
        void splat (float x, float y, Rgb c, float a, float size = 1.0f) noexcept
        {
            const int xi = (int) std::lround (x), yi = (int) std::lround (y);
            if (size <= 1.2f)
            {
                dot (xi, yi, c, a);
                const float o = a * 0.30f, d = a * 0.12f;
                dot (xi + 1, yi, c, o); dot (xi - 1, yi, c, o); dot (xi, yi + 1, c, o); dot (xi, yi - 1, c, o);
                dot (xi + 1, yi + 1, c, d); dot (xi - 1, yi - 1, c, d); dot (xi + 1, yi - 1, c, d); dot (xi - 1, yi + 1, c, d);
                return;
            }
            const int r = (int) std::ceil (size + 1.0f);
            for (int yy = -r; yy <= r; ++yy)
                for (int xx = -r; xx <= r; ++xx)
                {
                    const float d2 = (float) (xx * xx + yy * yy) / (size * size);
                    if (d2 < 2.2f) dot (xi + xx, yi + yy, c, a * std::exp (-1.6f * d2));
                }
        }
        void line (float x0, float y0, float x1, float y1, Rgb c, float a, float size = 1.0f) noexcept
        {
            const float len = std::hypot (x1 - x0, y1 - y0);
            const int steps = std::max (1, (int) (len * 0.9f));
            const float per = a / std::max (1.0f, (float) steps / std::max (1.0f, len));   // (the same light a pixel, however long the line)
            for (int k = 0; k <= steps; ++k)
            {
                const float t = (float) k / (float) steps;
                splat (x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, c, per * 0.55f, size);
            }
        }
        /** Tone-mapped (1 - e^-x: bright crossings bloom instead of clipping) to 8-bit RGBA, opaque. */
        void toRgba() noexcept
        {
            for (int i = 0; i < w * h; ++i)
            {
                for (int c = 0; c < 3; ++c)
                {
                    const float v = 1.0f - std::exp (-1.4f * buf[(size_t) (i * 3 + c)]);
                    rgba[(size_t) (i * 4 + c)] = (std::uint8_t) std::lround (255.0f * std::sqrt (std::clamp (v, 0.0f, 1.0f)));
                }
                rgba[(size_t) (i * 4 + 3)] = 255;
            }
        }
    };

    // ------------------------------------------------------------------------------------------------
    /** CHROMA SPACE: what has just played as strands of coloured light scrolling leftward - warm colours when
        it is warm, cool when it is bright, the note moving the colour round its half of the wheel - and a box
        round each piece captured as a chop (brighter while it plays); older chops in the bank along the bottom. */
    inline void chroma (Canvas& cv, const float* s, float time)
    {
        const float W = (float) cv.w, H = (float) cv.h, px = std::max (1.0f, W / 400.0f);
        cv.fade (0.55f);
        const int n = std::clamp ((int) s[0], 2, 48);
        const float window = (float) n / 30.0f;   // (seconds shown)
        const float space = std::clamp (s[172], 0.0f, 1.0f);
        for (int j = 0; j < 9; ++j)
        {
            const float yBase = H * (0.10f + 0.70f * ((float) j + 0.5f) / 9.0f);
            float lx = 0.0f, ly = yBase;
            for (int i = 0; i < n; ++i)
            {
                const float hue = s[1 + 3 * i], warm = s[2 + 3 * i], lv = std::clamp (s[3 + 3 * i], 0.0f, 1.0f);
                const float x = W * (float) i / (float) (n - 1);
                const float y = yBase + std::sin ((float) i * 0.35f + time * 1.7f + (float) j * 0.8f) * H * (0.012f + 0.05f * lv) * (1.0f + space)
                              + std::sin ((float) i * 0.11f - time * 0.6f + (float) j) * H * 0.02f;
                const auto c = warmCool (hue + 0.045f * (float) j * std::sin (hue * 6.2832f + (float) j), warm, 1.0f, 0.75f + 0.2f * lv);
                if (i > 0) cv.line (lx, ly, x, y, c, 0.18f + 0.9f * lv, px * (0.9f + 0.6f * lv));
                lx = x; ly = y;
            }
        }
        // the chops: a box round each (where it was in time), and the bank
        const int nc = std::clamp ((int) s[145], 0, 6);
        int banked = 0;
        for (int k = 0; k < nc; ++k)
        {
            const float age = s[146 + 4 * k], len = s[147 + 4 * k], hue = s[148 + 4 * k], flash = std::clamp (s[149 + 4 * k], 0.0f, 1.0f);
            const auto c = warmCool (hue, s[171], 1.0f, 0.6f);
            const float a = 0.35f + 1.2f * flash, th = px * (1.0f + 0.8f * flash);
            if (age < window)
            {
                const float x1 = W * (1.0f - age / window), x0 = std::max (0.0f, x1 - W * len / window), y0 = H * 0.06f, y1 = H * 0.84f;
                cv.line (x0, y0, x1, y0, c, a, th); cv.line (x1, y0, x1, y1, c, a, th); cv.line (x1, y1, x0, y1, c, a, th); cv.line (x0, y1, x0, y0, c, a, th);
                cv.line (x0, y0, x0 + 8.0f * px, y0 - 5.0f * px, c, a, th);   // (a tab: a captured chop)
            }
            const float bx = W * (0.08f + 0.14f * (float) banked++), by = H * 0.935f, bw = W * 0.11f, bh = H * 0.05f;   // (the bank)
            cv.line (bx, by - bh, bx + bw, by - bh, c, a, px); cv.line (bx + bw, by - bh, bx + bw, by + bh, c, a, px);
            cv.line (bx + bw, by + bh, bx, by + bh, c, a, px); cv.line (bx, by + bh, bx, by - bh, c, a, px);
            if (flash > 0.05f) for (float y = by - bh + 2.0f; y < by + bh - 1.0f; y += 2.0f) cv.line (bx + 2.0f, y, bx + bw - 2.0f, y, c, 0.25f * flash, 1.0f);
        }
        // now: a thin line at the right edge in the colour playing now
        const auto now = warmCool (s[170], s[171], 1.0f, 0.5f);
        cv.line (W - 2.0f * px, H * 0.04f, W - 2.0f * px, H * 0.86f, now, 0.6f, px);
    }

    // ------------------------------------------------------------------------------------------------
    /** HYPERCUBE: a neon vector cube - its 12 edges and one diagonal across each face - drawn as a CRT vector
        beam (soft phosphor bloom, a little flicker, the beam's pixels never quite where they were last time). It
        morphs with what plays: into any blend of two or three of 30 forms, each with its own random shape (a
        water droplet, a puddle with a random rim, a torus, a wave sheet... or, when the music is hard, stars,
        shards, crystals, zigzags), with a material on top (liquid wobble, ripples, shattering, rubber), always
        the music's vibe: calm music, slow liquid forms; hard music, sharp ones, and jumps on the big hits. Quiet,
        it comes home to the purple cube. One colour at a time, fading between a hundred.
        MORPH: how far it wanders; REACT: how hard the music pushes it; PALETTE: its colour; BEAM: focus and
        afterglow - each with an AUTO switch (the music decides). */
    namespace hc
    {
        using V3 = std::array<float, 3>;
        inline float hash (unsigned x) noexcept { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return (float) (x & 0xffffffu) / 16777215.0f; }
        inline float noise1 (float x, unsigned seed) noexcept
        {
            const float fl = std::floor (x); const int i = (int) fl; const float f = x - fl, u = f * f * (3.0f - 2.0f * f);
            const float a = hash (seed + (unsigned) i * 1013u), b = hash (seed + (unsigned) (i + 1) * 1013u);
            return (a + (b - a) * u) * 2.0f - 1.0f;
        }
        inline float tri (float x) noexcept { return 2.0f * std::abs (x - std::floor (x + 0.5f)) * 2.0f - 1.0f; }
        constexpr int numForms = 30, firstSharp = 16;
        struct Form { int kind = 0; std::array<float, 4> a {}; unsigned seed = 1; };
        /** Where cube point p (-1..1) goes under form f. */
        inline V3 apply (const Form& f, V3 p, float t, int line) noexcept
        {
            float x = p[0], y = p[1], z = p[2];
            const auto& a = f.a;
            const float r = std::sqrt (x * x + y * y + z * z) + 1.0e-6f, rr = std::sqrt (x * x + z * z) + 1.0e-6f;
            const float th = std::atan2 (z, x), ph = std::asin (std::clamp (y / r, -1.0f, 1.0f));
            const float dx = x / r, dy = y / r, dz = z / r;
            switch (f.kind)
            {
                // the soft ones: round, liquid, slow
                case 0: return { dx, dy, dz };                                                                                   // sphere
                case 1: { const float k = 0.5f * (1.0f + dy), w = 1.0f - (0.45f + 0.3f * a[0]) * k * k;                            // a water droplet
                          return { dx * w, dy * (1.1f + 0.3f * a[1]) + 0.15f, dz * w }; }
                case 2: { const float rim = 1.0f + (0.2f + 0.3f * a[0]) * noise1 (th * (1.0f + 3.0f * a[1]) + 10.0f * a[2], f.seed);   // a puddle, its rim random
                          const float R = (1.0f + 0.4f * a[2]) * rim * std::min (1.0f, rr * 1.3f);
                          return { x / rr * R, -0.8f + 0.06f * y + 0.04f * std::sin (rr * 9.0f - t * 1.6f), z / rr * R }; }
                case 3: { const float R = 0.72f, q = 0.25f + 0.2f * a[0], v = ph * 2.0f + th * a[1];                             // a torus
                          return { (R + q * std::cos (v)) * std::cos (th), q * std::sin (v), (R + q * std::cos (v)) * std::sin (th) }; }
                case 4: return { x * 1.15f, 0.35f * std::sin (x * (2.0f + 2.0f * a[0]) + z * (1.0f + 2.0f * a[1]) + t * (0.4f + a[2])), z * 1.15f };   // a wave sheet
                case 5: return { x, 0.32f * std::sin (rr * (5.0f + 6.0f * a[0]) - t * 1.8f) * std::exp (-rr * 0.8f), z };          // ripple rings
                case 6: { const float yy = dy - (0.5f + 0.4f * a[0]) * 0.35f * (1.0f - dy) * (1.0f - dy), sp = 1.0f + (0.4f + 0.5f * a[1]) * 0.5f * (1.0f - yy);   // melting
                          return { dx * sp, std::max (-0.95f, yy), dz * sp }; }
                case 7: { const float b = 1.0f + 0.35f * std::sin (3.0f * x + 6.0f * a[0] + t * 0.7f) * std::sin (3.0f * y + 6.0f * a[1]) * std::sin (3.0f * z + 6.0f * a[2] + t * 0.5f);   // a blob
                          return { dx * b, dy * b, dz * b }; }
                case 8: { const float ang = th + y * (2.0f + 3.0f * a[0]), rad = 0.25f + 0.6f * 0.5f * (y + 1.0f);                // a spiral
                          return { rad * std::cos (ang), y, rad * std::sin (ang) }; }
                case 9: return { x / rr * 0.8f, y * 1.05f, z / rr * 0.8f };                                                     // a cylinder
                case 10: { const float w = 0.8f * (1.0f - y) * 0.5f + 0.05f; return { x / rr * w * 1.4f, y, z / rr * w * 1.4f }; }   // a cone
                case 11: return { x, 0.55f * (x * x - z * z) * (0.6f + a[0]), z };                                               // a saddle
                case 12: { const float j = std::sin (t * (1.4f + a[0]) + y * 3.0f); return { x * (1.0f + 0.22f * j), y * (1.0f - 0.18f * j), z * (1.0f + 0.22f * j) }; }   // jelly
                case 13: return { dx * 1.2f, dy * (0.2f + 0.2f * a[0]), dz * 1.2f };                                              // a discus
                case 14: { const float ang = y * (1.2f + 2.0f * a[0]), c = std::cos (ang), s2 = std::sin (ang); return { x * c - z * s2, y, x * s2 + z * c }; }   // a soft twist
                case 15: return { dx * (1.0f + 0.3f * a[0]), dy * (0.7f + 0.3f * a[1]), dz * (1.0f + 0.3f * a[2]) };             // a pebble
                // the sharp ones: angular, fast
                case 16: { const float k = 3.0f + std::round (4.0f * a[0]);                                                     // a star
                           const float sp = 0.55f + 0.8f * std::pow (std::max (0.0f, std::cos (k * th) * std::cos (k * ph)), 2.0f + 4.0f * a[1]);
                           return { dx * sp, dy * sp, dz * sp }; }
                case 17: { const float s2 = 1.35f / (std::abs (x) + std::abs (y) + std::abs (z) + 1.0e-6f); return { x * s2, y * s2, z * s2 }; }   // an octahedron
                case 18: { const float w = (1.0f - y) * 0.5f * 1.5f; return { x * w, y, z * w }; }                                // a pyramid
                case 19: { const float o = 0.9f * (0.4f + a[0]);                                                                 // shards: the lines fly apart
                           return { x * 0.8f + o * (hash (f.seed + (unsigned) line * 7u) - 0.5f), y * 0.8f + o * (hash (f.seed + (unsigned) line * 13u) - 0.5f), z * 0.8f + o * (hash (f.seed + (unsigned) line * 29u) - 0.5f) }; }
                case 20: { const float q = std::max ({ std::abs (dx), std::abs (dy), std::abs (dz) }) + 1.0e-6f, k = 1.15f / q * (0.85f + 0.15f * a[0]); return { dx * k, dy * k, dz * k }; }   // a crystal block (faces flattened)
                case 21: return { x + 0.12f * tri (y * (1.0f + 1.5f * a[0])), y, z + 0.12f * tri (x * (1.0f + 1.5f * a[1])) };     // a zigzag
                case 22: { const float o = 0.6f + 0.6f * a[0]; return { x * (1.0f + o * hash (f.seed + (unsigned) line)), y * (1.0f + o * hash (f.seed + (unsigned) line * 3u)), z * (1.0f + o * hash (f.seed + (unsigned) line * 5u)) }; }   // exploding
                case 23: { const float ang = (y > 0.0f ? 1.0f : -1.0f) * (0.4f + 0.6f * a[0]), c = std::cos (ang), s2 = std::sin (ang); return { x * c - z * s2, y, x * s2 + z * c }; }   // a split twist: top and bottom turned apart
                case 24: { const float seg = 2.094395f, a0 = std::round (th / seg) * seg, w = std::cos (seg * 0.5f) / std::max (0.3f, std::cos (th - a0));   // a prism
                           return { std::cos (th) * w * 0.9f, y * 1.1f, std::sin (th) * w * 0.9f }; }
                case 25: { const float m = std::max ({ std::abs (dx), std::abs (dy), std::abs (dz) }), sp = 0.75f + (0.6f + 0.6f * a[0]) * std::pow (m, 10.0f); return { dx * sp, dy * sp, dz * sp }; }   // six spikes
                case 26: return { x * (1.0f + 0.3f * a[0]), y * (0.35f + 0.2f * a[1]) + x * 0.35f * (a[2] - 0.5f), z * (1.0f + 0.3f * a[0]) };   // a tilted slab
                case 27: { const float w = (1.0f - std::abs (y)) * 1.2f; return { x * w, y * 1.3f, z * w }; }                     // a diamond
                case 28: { const float ax = std::abs (x), ay = std::abs (y), az = std::abs (z), m = std::max ({ ax, ay, az });     // a cross
                           return { x * (ax >= m ? 1.5f : 0.4f), y * (ay >= m ? 1.5f : 0.4f), z * (az >= m ? 1.5f : 0.4f) }; }
                default: { const float w = 0.25f + 0.75f * std::pow (std::abs (y), 0.7f + a[0]); return { x * w * 1.1f, y * 1.1f, z * w * 1.1f }; }   // an hourglass: pinched at the waist
            }
        }
        /** A target: two or three forms blended, and a material (0 none, 1 liquid, 2 ripple, 3 shatter, 4 rubber). */
        struct Target { std::array<Form, 3> f {}; std::array<float, 3> w { 1.0f, 0.0f, 0.0f }; int material = 0; };
        inline Target randomTarget (unsigned& seed, float hard) noexcept
        {
            Target tg;
            auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return hash (seed); };
            const int n = 1 + (int) (rnd() * 2.999f);
            float sum = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                const bool sharp = rnd() < std::clamp (hard * 1.3f - 0.15f, 0.05f, 0.95f);   // (the vibe: hard music, sharp forms)
                tg.f[(size_t) i].kind = sharp ? firstSharp + (int) (rnd() * (float) (numForms - firstSharp) * 0.999f) : (int) (rnd() * (float) firstSharp * 0.999f);
                for (auto& v : tg.f[(size_t) i].a) v = rnd();
                tg.f[(size_t) i].seed = (unsigned) (rnd() * 1.0e6f);
                tg.w[(size_t) i] = i == 0 ? 1.0f : i < n ? 0.15f + 0.35f * rnd() : 0.0f; sum += tg.w[(size_t) i];   // (one leads: a shape you can name, the others colour it)
            }
            for (auto& w : tg.w) w /= sum;
            tg.material = hard > 0.5f ? (rnd() < 0.5f ? 3 : 4) : (rnd() < 0.5f ? 1 : 2);
            if (rnd() < 0.2f) tg.material = 0;
            return tg;
        }
        inline V3 shapeOf (const Target& tg, V3 p, float t, int line) noexcept
        {
            V3 o { 0.0f, 0.0f, 0.0f };
            for (int i = 0; i < 3; ++i) if (tg.w[(size_t) i] > 0.0f) { const auto q = apply (tg.f[(size_t) i], p, t, line); for (int k = 0; k < 3; ++k) o[(size_t) k] += tg.w[(size_t) i] * q[(size_t) k]; }
            return o;
        }
        /** A hundred colours: 0 the neon purple; the rest round the wheel by the golden angle, each its own saturation. */
        inline Rgb paletteColour (int i) noexcept
        {
            if (i <= 0) return hsv (0.78f, 0.78f, 1.0f);
            return hsv (std::fmod (0.78f + (float) i * 0.618034f, 1.0f), 0.5f + 0.45f * hash ((unsigned) i * 7919u), 1.0f);
        }
    }

    struct CubeState
    {
        float rx = 0.35f, ry = 0.5f;
        hc::Target a, b;
        float blend = 1.0f, blendSpeed = 0.3f, sinceNew = 0.0f, sinceJump = 10.0f, morph = 0.0f, pulse = 0.0f;
        unsigned seed = 0x5eedu;
        int colA = 0, colB = 0; float colBlend = 1.0f, sinceCol = 0.0f;
        float shatter = 0.0f;
    };

    inline void hypercube (Canvas& cv, CubeState& st, const float* s, float time, float dt)
    {
        using hc::V3;
        const float W = (float) cv.w, H = (float) cv.h, px = std::max (1.0f, W / 448.0f);
        const float* band = s;
        const float hit = s[24], level = s[26], hard = std::clamp (s[165], 0.0f, 1.0f);
        const bool autoMorph = s[161] > 0.5f, autoReact = s[162] > 0.5f, autoPalette = s[163] > 0.5f, autoBeam = s[164] > 0.5f;
        const float energy = std::clamp (0.6f * level + 0.4f * hard, 0.0f, 1.0f);
        const float morphKnob = autoMorph ? std::clamp (0.4f + 0.6f * energy, 0.0f, 1.0f) : s[157];
        const float react = autoReact ? 0.35f + 0.6f * hard : s[158];
        const float beam = autoBeam ? 0.75f - 0.45f * hard : s[160];   // (calm: long, soft afterglow; hard: tight and quick)
        const bool quiet = level < 0.12f;

        // where it is going: home to the cube when quiet; new shapes slowly, and on the big hits of hard music
        st.morph += (1.0f - std::exp (-dt / 1.2f)) * ((quiet ? 0.0f : morphKnob) - st.morph);
        st.sinceNew += dt; st.sinceJump += dt;
        const float period = (14.0f - 10.0f * hard) * (1.5f - morphKnob);
        auto newTarget = [&] (float seconds)
        {
            if (st.blend < 1.0f) st.a = st.b;   // (the one under way becomes the start)
            st.b = hc::randomTarget (st.seed, hard);
            st.blend = 0.0f; st.blendSpeed = 1.0f / seconds; st.sinceNew = 0.0f;
        };
        if (st.seed == 0x5eedu) { st.seed = (unsigned) (time * 1000.0f) + 77u; st.a = hc::randomTarget (st.seed, hard); st.b = st.a; }
        if (st.sinceNew > period) newTarget (hard > 0.5f ? 1.2f : 3.5f);
        if (hit > 0.8f && hard > 0.4f && st.sinceJump > 0.6f && ! quiet) { newTarget (0.25f); st.sinceJump = 0.0f; st.shatter = 1.0f; }
        st.blend = std::min (1.0f, st.blend + dt * st.blendSpeed);
        if (st.blend >= 1.0f) st.a = st.b;
        float e = st.blend * st.blend * (3.0f - 2.0f * st.blend);
        if (st.b.material == 4) e += 0.18f * std::sin (st.blend * 11.0f) * std::exp (-st.blend * 4.0f) * (1.0f - st.blend);   // (rubber: it overshoots, springs back)
        st.pulse = std::max (hit, st.pulse * std::exp (-dt / 0.18f));
        st.shatter *= std::exp (-dt / 0.4f);
        st.ry += dt * (0.18f + 0.55f * energy); st.rx = 0.35f + 0.15f * std::sin (time * 0.13f);

        // its colour: PALETTE's, or (AUTO) wandering through the hundred - home to purple when quiet
        const int wantCol = quiet ? 0 : autoPalette ? st.colB : (int) std::lround (s[159] * 99.0f);
        st.sinceCol += dt;
        if (autoPalette && ! quiet && st.sinceCol > 12.0f - 8.0f * hard) { st.sinceCol = 0.0f; unsigned k = st.seed + 991u; st.seed = st.seed * 1664525u + 1013904223u; (void) k; const int next = 1 + (int) (hc::hash (st.seed) * 98.99f); if (next != st.colB) { st.colA = st.colB; st.colB = next; st.colBlend = 0.0f; } }
        if (! autoPalette || quiet) if (wantCol != st.colB) { st.colA = st.colB; st.colB = wantCol; st.colBlend = 0.0f; }
        st.colBlend = std::min (1.0f, st.colBlend + dt / 1.5f);
        const auto ca = hc::paletteColour (st.colA), cb = hc::paletteColour (st.colB);
        const Rgb col { ca.r + (cb.r - ca.r) * st.colBlend, ca.g + (cb.g - ca.g) * st.colBlend, ca.b + (cb.b - ca.b) * st.colBlend };

        // the beam: BEAM is its focus and its afterglow
        cv.fade (0.22f + 0.48f * beam);
        const float lineSize = px * (1.4f - 0.6f * beam), jitter = px * 0.35f * (1.2f - beam);
        const float cyw = std::cos (st.ry), syw = std::sin (st.ry), cp = std::cos (st.rx), sp = std::sin (st.rx);
        const float scale = std::min (W, H) * 0.25f * (1.0f + 0.12f * react * st.pulse);
        auto project = [&] (V3 q, float& sx, float& sy)
        {
            const float x1 = cyw * q[0] + syw * q[2], z1 = -syw * q[0] + cyw * q[2];
            const float y1 = cp * q[1] - sp * z1, z2 = sp * q[1] + cp * z1, persp = 3.2f / (3.2f + z2);
            sx = 0.5f * W + x1 * scale * persp; sy = 0.5f * H - y1 * scale * persp;
        };
        // the lines: the 12 edges and a diagonal across each face
        static const std::array<std::array<V3, 2>, 18> lines = []
        {
            std::array<std::array<V3, 2>, 18> L {};
            int n = 0;
            for (int ax = 0; ax < 3; ++ax)
                for (int i = 0; i < 4; ++i)
                {
                    V3 a0 {}, a1 {};
                    const int u = (ax + 1) % 3, v = (ax + 2) % 3;
                    a0[(size_t) ax] = -1.0f; a1[(size_t) ax] = 1.0f;
                    a0[(size_t) u] = a1[(size_t) u] = (i & 1) ? 1.0f : -1.0f;
                    a0[(size_t) v] = a1[(size_t) v] = (i & 2) ? 1.0f : -1.0f;
                    L[(size_t) n++] = { a0, a1 };
                }
            for (int ax = 0; ax < 3; ++ax)
                for (int sgn = -1; sgn <= 1; sgn += 2)
                {
                    const int u = (ax + 1) % 3, v = (ax + 2) % 3;
                    V3 a0 {}, a1 {};
                    a0[(size_t) ax] = a1[(size_t) ax] = (float) sgn;
                    a0[(size_t) u] = -1.0f; a0[(size_t) v] = -1.0f; a1[(size_t) u] = 1.0f; a1[(size_t) v] = 1.0f;
                    L[(size_t) n++] = { a0, a1 };
                }
            return L;
        }();
        constexpr int steps = 26;
        const float flicker = 0.92f + 0.08f * std::sin (time * 113.0f);
        unsigned js = (unsigned) (time * 60.0f) * 2654435761u;
        for (int li = 0; li < 18; ++li)
        {
            float lx = 0.0f, ly = 0.0f;
            for (int k = 0; k <= steps; ++k)
            {
                const float u = (float) k / (float) steps;
                V3 p;
                for (int c = 0; c < 3; ++c) p[(size_t) c] = lines[(size_t) li][0][(size_t) c] + (lines[(size_t) li][1][(size_t) c] - lines[(size_t) li][0][(size_t) c]) * u;
                // its shape: the cube, morphed as far as MORPH (and the music) take it
                V3 q = p;
                if (st.morph > 0.002f)
                {
                    const auto sa = hc::shapeOf (st.a, p, time, li), sb = hc::shapeOf (st.b, p, time, li);
                    const float mk = std::sqrt (st.morph);   // (half way on the knob is well into the new shape)
                    for (int c = 0; c < 3; ++c) q[(size_t) c] = p[(size_t) c] + (sa[(size_t) c] + (sb[(size_t) c] - sa[(size_t) c]) * e - p[(size_t) c]) * mk;
                    // its material
                    const int mat = st.b.material;
                    if (mat == 1) for (int c = 0; c < 3; ++c) q[(size_t) c] += st.morph * 0.05f * std::sin (time * 1.3f + p[(size_t) ((c + 1) % 3)] * 3.0f + (float) c);
                    if (mat == 2) q[1] += st.morph * 0.06f * std::sin (std::sqrt (q[0] * q[0] + q[2] * q[2]) * 9.0f - time * 3.0f);
                    if (mat == 3) for (int c = 0; c < 3; ++c) q[(size_t) c] += st.shatter * st.morph * 0.3f * (hc::hash ((unsigned) li * 97u + (unsigned) c * 13u + st.seed) - 0.5f);
                }
                // REACT: each part pushed out by its band (the lows at the bottom, the highs at the top)
                const int bi = std::clamp ((int) ((q[1] + 1.0f) * 0.5f * 15.0f), 0, 15);
                const float push = 1.0f + react * 0.15f * band[bi];
                for (auto& c : q) c *= push;
                // (always on the glass: what reaches past the cube's size is drawn in softly)
                const float m = std::sqrt (q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
                if (m > 1.25f) { const float k = (1.25f + 0.4f * std::tanh ((m - 1.25f) / 0.4f)) / m; for (auto& c : q) c *= k; }
                float sx, sy;
                project (q, sx, sy);
                // a vector beam: never quite on the same pixel
                js = js * 1664525u + 1013904223u; sx += jitter * (hc::hash (js) - 0.5f);
                js = js * 1664525u + 1013904223u; sy += jitter * (hc::hash (js) - 0.5f);
                if (k > 0)
                {
                    cv.line (lx, ly, sx, sy, col, 1.15f * flicker, lineSize);             // the beam
                    cv.line (lx, ly, sx, sy, col, 0.22f * flicker, lineSize * 2.6f);      // its bloom
                }
                lx = sx; ly = sy;
            }
        }
    }

    class Canvas;
    /** RACK TUNER: its 60,000 settings as a turning cloud of small cubes, a cluster for each unit, its colour
        warm to cool by what the unit does; a tune sweeps through them, and the chosen ones light up and send a
        beam to the rack. (UI/RackTuner.cpp: it knows the bank.) */
    void tunerCloud (Canvas& cv, float time);

    /** A made-up state at time t (before the engine has published one; screenshots). */
    inline void demo (Kind k, float t, float* o)
    {
        std::fill (o, o + 192, 0.0f);
        if (k == Kind::chroma)
        {
            o[0] = 48.0f;
            for (int i = 0; i < 48; ++i)
            {
                const float tt = t - (float) (47 - i) / 30.0f;
                o[1 + 3 * i] = 0.5f + 0.5f * std::sin (tt * 0.9f); o[2 + 3 * i] = 0.5f + 0.5f * std::sin (tt * 0.37f + 1.0f);
                o[3 + 3 * i] = std::max (0.0f, std::sin (tt * 7.0f)) * (0.5f + 0.5f * std::sin (tt * 0.5f));
            }
            o[145] = 4.0f;
            for (int c = 0; c < 4; ++c) { o[146 + 4 * c] = std::fmod (t * 0.6f + 0.4f * (float) c, 3.0f); o[147 + 4 * c] = 0.18f; o[148 + 4 * c] = 0.25f * (float) c; o[149 + 4 * c] = c == (int) std::fmod (t * 2.0f, 4.0f) ? 1.0f : 0.0f; }
            o[170] = 0.5f + 0.5f * std::sin (t * 0.9f); o[171] = 0.5f + 0.5f * std::sin (t * 0.37f + 1.0f); o[172] = 0.6f; o[174] = 0.5f;
        }
        else if (k == Kind::hypercube)
        {
            for (int b = 0; b < 16; ++b) o[b] = 0.35f + 0.35f * std::max (0.0f, std::sin (t * (1.3f + 0.21f * (float) b) + (float) b * 0.7f));
            for (int q = 0; q < 8; ++q) o[16 + q] = 0.6f * std::sin ((float) q * 1.7f);
            o[24] = std::max (0.0f, std::sin (t * 4.0f) - 0.8f) * 5.0f; o[25] = 0.5f; o[26] = 0.6f; o[27] = 0.7f; o[28] = 64.0f;
            for (int i = 0; i < 64; ++i) { const float a = (float) i / 64.0f * 6.2832f; o[29 + 2 * i] = 0.4f * std::sin (a * 3.0f + t); o[30 + 2 * i] = 0.4f * std::sin (a * 2.0f + t * 0.7f); }
            o[157] = 0.5f; o[158] = 0.6f; o[159] = 0.0f; o[160] = 0.5f; o[161] = 1.0f; o[162] = 1.0f; o[163] = 1.0f; o[164] = 1.0f;
            o[165] = 0.5f + 0.4f * std::sin (t * 0.21f);   // (the vibe wanders, calm to hard)
        }
    }
}
