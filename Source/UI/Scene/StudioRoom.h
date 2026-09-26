#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <thread>
#include <vector>

/*  What the rack's metal, chrome and glass reflect: the room in front of it, baked once into an
    equirectangular texture (HardwareKit's envColor reads it when uEnvMix is 1). The key light's window
    up to the left, with its glazing bars; a warm floor lamp to the right; walnut walls and a wooden
    floor in the dark; the lamp-lit slat wall behind the rack.

    The analytic studio it replaces was a bright, pinkish white room; the rack stands in a warm, dim
    one now, so its reflections were lying. One texture fetch per pixel, no cost to speak of.
    Stored as sqrt (colour / range) like the wall: decode c * c * range. */
namespace pad::studioroom
{
    inline constexpr float range = 4.0f;
    inline constexpr int texW = 2048, texH = 1024;  // sharp enough to mirror in a wet clear coat, like still water

    inline std::vector<juce::uint8> bake()
    {
        std::vector<juce::uint8> px ((size_t) (texW * texH * 4));

        auto smooth = [] (float e0, float e1, float x)
        {
            const float t = std::clamp ((x - e0) / (e1 - e0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        };
        auto norm = [] (float x, float y, float z)
        {
            const float n = std::sqrt (x * x + y * y + z * z);
            return std::array<float, 3> { x / n, y / n, z / n };
        };
        auto dot = [] (const std::array<float, 3>& a, float x, float y, float z) { return a[0] * x + a[1] * y + a[2] * z; };

        // The window the key light comes through (CameraRig's lightDir), and a lamp to the right
        const auto window = norm (-0.50f, 0.62f, 0.72f);
        const auto lamp = norm (0.85f, 0.05f, 0.55f);
        // The window's own frame: right and up across its face
        const auto wr = norm (window[2], 0.0f, -window[0]);
        const std::array<float, 3> wu { window[1] * wr[2] - window[2] * wr[1], window[2] * wr[0] - window[0] * wr[2], window[0] * wr[1] - window[1] * wr[0] };

        const auto pendant = norm (0.10f, 0.93f, 0.35f);   // a warm pendant lamp overhead, in front of the rack
        const auto window2 = norm (0.95f, 0.30f, -0.05f);  // a smaller window to the right, cooler light
        const auto w2r = norm (window2[2], 0.0f, -window2[0]);
        const std::array<float, 3> w2u { window2[1] * w2r[2] - window2[2] * w2r[1], window2[2] * w2r[0] - window2[0] * w2r[2], window2[0] * w2r[1] - window2[1] * w2r[0] };

        const float keyAz = -0.05f;   // the big soft white light: just left of straight behind the viewer
                                      // (the rack's faces mirror about +-6 degrees around it)

        const float pi = 3.14159265f;
        auto rows = [&] (int j0, int j1)
        {
        for (int j = j0; j < j1; ++j)
        {
            const float theta = pi * ((float) j + 0.5f) / (float) texH;   // 0 = straight up
            const float y = std::cos (theta), s = std::sin (theta);
            for (int i = 0; i < texW; ++i)
            {
                const float phi = 2.0f * pi * (((float) i + 0.5f) / (float) texW - 0.5f);
                const float x = s * std::sin (phi), z = s * std::cos (phi);   // u = 0.5 looks at +z (the room); the seam is behind the rack

                // The room: dim walnut walls, a darker ceiling, a wooden floor catching the window
                float r = 0.125f, g = 0.085f, b = 0.058f;
                const float up = smooth (0.2f, 1.0f, y);
                r *= 1.0f - 0.45f * up; g *= 1.0f - 0.45f * up; b *= 1.0f - 0.40f * up;
                if (y < 0.0f)
                {
                    const float plank = 0.85f + 0.15f * std::sin (x / std::max (-y, 0.05f) * 9.0f);
                    const float sunOnFloor = std::exp (-std::pow ((x / std::max (-y, 0.05f) + 0.8f) * 0.6f, 2.0f)) * smooth (0.0f, -0.4f, y);
                    r = (0.090f + 0.20f * sunOnFloor) * plank;
                    g = (0.052f + 0.14f * sunOnFloor) * plank;
                    b = (0.028f + 0.08f * sunOnFloor) * plank;
                }

                // Behind the rack (the reflection of what it stands against): the slat wall in its lamp's pool
                if (z < 0.0f)
                {
                    const float pool = std::exp (-(x * x * 3.0f + (y - 0.05f) * (y - 0.05f) * 6.0f)) * smooth (0.0f, -0.6f, z);
                    r += 0.30f * pool; g += 0.17f * pool; b += 0.07f * pool;
                }

                // The window: a bright pane with glazing bars and a soft edge, the sky warm near the bottom
                {
                    const float c = dot (window, x, y, z);
                    if (c > 0.6f)
                    {
                        const float u = dot (wr, x, y, z) / c, v = dot (wu, x, y, z) / c;   // on the window's plane
                        const float frame = smooth (0.40f, 0.36f, std::abs (u)) * smooth (0.34f, 0.30f, std::abs (v));
                        const float bars = std::min ({ smooth (0.004f, 0.010f, std::abs (u)), smooth (0.006f, 0.014f, std::abs (u - 0.2f)),
                                                       smooth (0.006f, 0.014f, std::abs (u + 0.2f)), smooth (0.006f, 0.014f, std::abs (v)) });
                        const float sky = 0.5f + 0.5f * v / 0.34f;
                        const float pane = frame * bars;
                        r += pane * (3.4f - 0.3f * sky);
                        g += pane * (3.1f - 0.1f * sky);
                        b += pane * (2.6f + 0.3f * sky);
                        const float halo = std::exp (-std::max (0.0f, 1.0f - c) * 14.0f) * (1.0f - pane);
                        r += 0.35f * halo; g += 0.29f * halo; b += 0.22f * halo;
                    }
                }

                // Shelves along the walls (front half of the room): dark boards, record spines catching light
                if (z > 0.1f && y > -0.25f && y < 0.45f)
                {
                    const float boards = smooth (0.02f, 0.0f, std::abs (std::fmod (y + 1.0f, 0.18f) - 0.09f) - 0.07f);
                    const float spines = 0.5f + 0.5f * std::sin (std::atan2 (x, z) * 260.0f);
                    const float lit = 0.06f * spines * (1.0f - boards);
                    r = r * (1.0f - 0.5f * boards) + lit * 1.0f; g = g * (1.0f - 0.5f * boards) + lit * 0.75f; b = b * (1.0f - 0.5f * boards) + lit * 0.55f;
                }

                // The second window: smaller, cooler, four panes
                {
                    const float c = dot (window2, x, y, z);
                    if (c > 0.8f)
                    {
                        const float u = dot (w2r, x, y, z) / c, v = dot (w2u, x, y, z) / c;
                        const float frame = smooth (0.22f, 0.20f, std::abs (u)) * smooth (0.26f, 0.24f, std::abs (v));
                        const float bars = std::min (smooth (0.005f, 0.011f, std::abs (u)), smooth (0.005f, 0.011f, std::abs (v)));
                        const float pane = frame * bars;
                        r += pane * 1.7f; g += pane * 1.9f; b += pane * 2.2f;
                    }
                }

                // A big soft white light behind the viewer, as in a product photo: face-on panels mirror it
                // as a broad glow that bends over every knob and moves as you do - the wet look.
                // It runs floor to ceiling, so every unit - top, middle or bottom of the curved case - mirrors
                // it the same way: only the direction around matters, not the height. Soft all over (a
                // diffused white light, not a hard strip), so its reflection is a broad glow sweeping across
                // the rack, brightest to the left of centre and fading smoothly to the right
                {
                    const float az = std::atan2 (x, z);
                    const float tall = smooth (0.97f, 0.80f, std::abs (y));   // fades only straight up / down
                    const float d = (az - keyAz) / 0.075f;
                    const float key = std::exp (-0.5f * d * d) + 0.35f * std::exp (-0.5f * d * d * 0.16f);   // core + wide skirt
                    const float light = key * tall * (1.0f - 0.2f * std::abs (y));
                    r += light * 3.1f; g += light * 3.05f; b += light * 2.95f;
                }

                // The pendant: a bright bulb in a dark cone, and its glow
                {
                    const float c = dot (pendant, x, y, z);
                    const float bulb = smooth (0.9992f, 0.9997f, c);
                    const float glow = std::exp (-std::max (0.0f, 1.0f - c) * 180.0f);
                    r += 3.6f * bulb + 0.45f * glow;
                    g += 2.7f * bulb + 0.30f * glow;
                    b += 1.5f * bulb + 0.14f * glow;
                }

                // The floor lamp: a warm shade, glowing
                {
                    const float c = dot (lamp, x, y, z);
                    const float shade = smooth (0.9975f, 0.9990f, c);
                    const float glow = std::exp (-std::max (0.0f, 1.0f - c) * 60.0f);
                    r += 2.8f * shade + 0.30f * glow;
                    g += 1.6f * shade + 0.16f * glow;
                    b += 0.6f * shade + 0.05f * glow;
                }

                auto enc = [] (float v) { return (juce::uint8) std::lround (255.0f * std::sqrt (std::clamp (v / range, 0.0f, 1.0f))); };
                auto* p = px.data() + ((size_t) j * texW + (size_t) i) * 4;
                p[0] = enc (r);
                p[1] = enc (g);
                p[2] = enc (b);
                p[3] = 255;
            }
        }
        };

        // Rows in parallel: each thread writes its own rows only
        const int threads = (int) std::clamp (std::thread::hardware_concurrency(), 1u, 8u);
        std::vector<std::thread> workers;
        for (int t = 1; t < threads; ++t)
            workers.emplace_back (rows, texH * t / threads, texH * (t + 1) / threads);
        rows (0, texH / threads);
        for (auto& th : workers)
            th.join();
        return px;
    }

    /** The clear coat's smudges and wipe marks, baked into a small tiling texture (r: smudge, g: wipe),
        so the coat reads one texel instead of working three noises out per pixel. The pattern is the
        shader's own: value noise over 16 x 16 cells, the smudge in two octaves, the wipes stretched. */
    inline constexpr int smudgeSize = 256;

    inline std::vector<juce::uint8> bakeSmudge()
    {
        constexpr int n = smudgeSize, cells = 16;
        auto hash = [] (int x, int y)
        {
            const float h = std::sin ((float) (x & 1023) * 127.1f + (float) (y & 1023) * 311.7f) * 43758.5453f;
            return h - std::floor (h);
        };
        auto noise = [&] (float x, float y, int period)   // tiling value noise, `period` cells across
        {
            const int ix = (int) std::floor (x), iy = (int) std::floor (y);
            const float fx = x - (float) ix, fy = y - (float) iy;
            const float ux = fx * fx * (3.0f - 2.0f * fx), uy = fy * fy * (3.0f - 2.0f * fy);
            auto h = [&] (int a, int b) { return hash (((a % period) + period) % period, ((b % period) + period) % period); };
            const float a = h (ix, iy) + (h (ix + 1, iy) - h (ix, iy)) * ux;
            const float b = h (ix, iy + 1) + (h (ix + 1, iy + 1) - h (ix, iy + 1)) * ux;
            return a + (b - a) * uy;
        };
        std::vector<juce::uint8> px ((size_t) (n * n * 4));
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i)
            {
                const float x = (float) i / (float) n * (float) cells, y = (float) j / (float) n * (float) cells;
                const float smudge = noise (x, y, cells) * 0.6f + noise (x * 3.0f, y * 3.0f, cells * 3) * 0.4f;
                const float wipe = noise (x * 0.5f + y * 2.0f, y * 0.25f, cells);
                auto* p = px.data() + ((size_t) j * n + (size_t) i) * 4;
                p[0] = (juce::uint8) std::lround (255.0f * std::clamp (smudge, 0.0f, 1.0f));
                p[1] = (juce::uint8) std::lround (255.0f * std::clamp (wipe, 0.0f, 1.0f));
                p[2] = 0;
                p[3] = 255;
            }
        return px;
    }
}

