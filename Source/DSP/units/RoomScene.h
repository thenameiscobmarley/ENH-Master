#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

/*  RAY ROOM's room, shared by its sound (RayRoom.h) and its screen (the renderer and the 2D view draw it):
    a room seen from above - two speakers at the front, you in the middle - its size from SPACE, and the
    state the unit publishes each block (levels, bands, the dots in flight). Pure maths, no JUCE. */
namespace enh::dsp::units::room
{
    inline constexpr int bands = 6, maxDots = 40;
    inline constexpr float maxHalfW = 12.5f, aspect = 0.62f;   // the biggest room, and depth for width

    /** The room's half width and half depth (metres) for SPACE 0 .. 10. */
    inline float halfWidth (float space) noexcept { const float s = std::clamp (space, 0.0f, 10.0f) / 10.0f; return 2.0f + 10.0f * std::pow (s, 1.3f); }
    inline float halfDepth (float space) noexcept { return aspect * halfWidth (space); }
    /** Where things stand (x across, y from the front wall -D to the back wall +D). */
    struct P { float x, y; };
    inline P speaker (int side, float W, float D) noexcept { return { (side == 0 ? -0.45f : 0.45f) * W, -0.62f * D }; }
    inline P listener (float, float D) noexcept { return { 0.0f, 0.30f * D }; }

    /** What the unit publishes (floats): see State. */
    struct State
    {
        float W = 4.0f, D = 2.5f, levelL = 0.0f, levelR = 0.0f, width = 0.0f, balance = 0.0f;
        std::array<float, bands> bandL {}, bandR {};
        int dots = 0;
        std::array<float, maxDots * 4> dot {};   // x, y, kind (0 crackle, 1 pop), age 0..1
        static constexpr int size = 6 + 2 * bands + 1 + maxDots * 4;
        void write (float* o) const noexcept
        {
            o[0] = W; o[1] = D; o[2] = levelL; o[3] = levelR; o[4] = width; o[5] = balance;
            for (int b = 0; b < bands; ++b) { o[6 + b] = bandL[(size_t) b]; o[6 + bands + b] = bandR[(size_t) b]; }
            o[6 + 2 * bands] = (float) dots;
            std::copy (dot.begin(), dot.end(), o + 7 + 2 * bands);
        }
        void read (const float* o) noexcept
        {
            W = o[0]; D = o[1]; levelL = o[2]; levelR = o[3]; width = o[4]; balance = o[5];
            for (int b = 0; b < bands; ++b) { bandL[(size_t) b] = o[6 + b]; bandR[(size_t) b] = o[6 + bands + b]; }
            dots = std::clamp ((int) o[6 + 2 * bands], 0, maxDots);
            std::copy (o + 7 + 2 * bands, o + size, dot.begin());
        }
    };

    /** A stroke of the picture, in room metres: from (x0, y0) to (x1, y1), `thick` (0 .. 1), `bright` (0 .. 1). */
    struct Stroke { float x0, y0, x1, y1, thick, bright; };

    /** The rays: from each speaker, one per band, fanned into the room and bouncing off the walls. What
        is playing sets them: a band's level its thickness and how far it reaches (SPACE makes the room -
        and so every ray - longer), the stereo balance and width turn and spread the fan, and each ray sways
        with its band. */
    inline void rays (const State& s, float time, std::vector<Stroke>& out)
    {
        out.clear();
        const float W = s.W, D = s.D, diag = std::sqrt (W * W + D * D);
        for (int side = 0; side < 2; ++side)
        {
            const auto sp = speaker (side, W, D);
            const auto& lv = side == 0 ? s.bandL : s.bandR;
            for (int b = 0; b < bands; ++b)
            {
                const float level = std::clamp (lv[(size_t) b], 0.0f, 1.0f);
                if (level < 0.01f) continue;
                // the fan: centred into the room (+y), turned toward the louder side, spread by the width
                const float spread = 0.35f + 0.65f * std::clamp (s.width, 0.0f, 1.0f);
                const float a = 1.5708f + (side == 0 ? 1.0f : -1.0f) * (0.15f + 0.95f * spread * ((float) b + 0.5f) / (float) bands)
                              - 0.55f * s.balance + 0.20f * level * std::sin (time * (0.6f + 0.21f * (float) b) + (float) b * 1.7f);
                float x = sp.x, y = sp.y, dx = std::cos (a), dy = std::sin (a);
                float left = diag * (0.35f + 1.4f * std::sqrt (level));   // how far this band's sound carries
                float bright = 0.35f + 0.65f * level;
                for (int bounce = 0; bounce < 4 && left > 0.01f; ++bounce)
                {
                    // to the nearest wall along the ray
                    float t = 1.0e9f;
                    if (dx > 1.0e-5f) t = std::min (t, (W - x) / dx); else if (dx < -1.0e-5f) t = std::min (t, (-W - x) / dx);
                    if (dy > 1.0e-5f) t = std::min (t, (D - y) / dy); else if (dy < -1.0e-5f) t = std::min (t, (-D - y) / dy);
                    const float step = std::min (t, left);
                    out.push_back ({ x, y, x + dx * step, y + dy * step, level, bright });
                    x += dx * step; y += dy * step; left -= step;
                    if (std::abs (x) >= W - 1.0e-4f) dx = -dx;
                    if (std::abs (y) >= D - 1.0e-4f) dy = -dy;
                    bright *= 0.62f;   // each wall takes some of it
                }
            }
        }
    }
}
