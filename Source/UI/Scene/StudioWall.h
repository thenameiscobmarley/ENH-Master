#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <thread>

/*  The studio wall behind the rack, baked once into a texture (the studioWall material only reads it
    back): vertical walnut slats with dark gaps between them, a warm pool of lamp light behind the rack,
    the window's three soft shafts crossing it (the same windowBeam as HardwareKit's lighting, at the
    wall's depth), and darker toward the floor, the ceiling and the far sides.

    It never changes, so working it out per pixel every frame (as a shader) cost ~1.8 ms a frame on an
    integrated GPU; read from a mipmapped texture it costs one fetch, and the mip chain antialiases the
    slats for free. Stored as sqrt (colour / wallRange) for precision in the dark: decode c * c * range. */
namespace pad::studiowall
{
    inline constexpr float x0 = -12.0f, width = 24.0f, height = 12.0f;   // world metres covered (y from the floor)
    inline constexpr float range = 0.6f;
    inline constexpr int texW = 2048, texH = 512;   // the slats run up the wall: height needs less

    inline std::vector<juce::uint8> bake (float wallZ, float floorY, float rackCentreY)
    {
        std::vector<juce::uint8> px ((size_t) (texW * texH * 4));

        auto hash = [] (float a, float b)
        {
            const float h = std::sin (a * 127.1f + b * 311.7f) * 43758.5453f;
            return h - std::floor (h);
        };
        auto smooth = [] (float e0, float e1, float x)
        {
            const float t = std::clamp ((x - e0) / (e1 - e0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        };
        // HardwareKit's windowBeam (Materials.cpp), at the wall
        const float bx = 0.80f, by = 0.56f, bz = 0.20f, bn = std::sqrt (bx * bx + by * by + bz * bz);

        // Separable where it can be (per column, per row), tables where it cannot: ~20x faster than
        // working every pixel out in full, so the bake does not hold up opening the window
        struct Column { float gap, tone, phase, sc, cc, poolX, fadeX, beamX; };
        std::vector<Column> cols ((size_t) texW);
        for (int i = 0; i < texW; ++i)
        {
            const float x = x0 + width * ((float) i + 0.5f) / (float) texW;
            const float sx = x * 6.5f, cell = std::floor (sx), f = sx - cell;   // slats 15 cm wide, 2 cm gaps
            const float qx = x / 4.6f;
            cols[(size_t) i] = { smooth (0.11f, 0.15f, f) * smooth (1.0f, 0.97f, f), 0.86f + 0.24f * hash (cell, 7.0f),
                                 hash (cell, 3.0f) * 40.0f, std::sin (cell), std::cos (cell), std::exp (-qx * qx),
                                 1.0f - 0.45f * smooth (3.0f, 10.0f, std::abs (x)), x * bx / bn };
        }

        constexpr int sinN = 4096;
        std::vector<float> sinTab ((size_t) sinN);
        for (int k = 0; k < sinN; ++k)
            sinTab[(size_t) k] = std::sin (6.2831853f * (float) k / (float) sinN);
        auto fastSin = [&] (float v)
        {
            float t = v * (1.0f / 6.2831853f);
            t -= std::floor (t);
            return sinTab[(size_t) ((int) (t * (float) sinN) & (sinN - 1))];
        };

        // The beam depends only on how far across the shafts a point is: tabulate it
        constexpr int beamN = 4096;
        const float acrossMin = -30.0f, acrossMax = 30.0f;
        std::vector<float> beamTab ((size_t) beamN);
        for (int k = 0; k < beamN; ++k)
        {
            const float across = acrossMin + (acrossMax - acrossMin) * (float) k / (float) (beamN - 1);
            float b = smooth (1.95f, 1.05f, std::abs (across - 0.35f))
                    + 0.80f * smooth (1.35f, 0.55f, std::abs (across + 1.75f))
                    + 0.65f * smooth (1.25f, 0.45f, std::abs (across - 2.85f));
            b += 0.05f * smooth (3.2f, 1.0f, std::abs (across - 0.35f));
            beamTab[(size_t) k] = std::clamp (b, 0.0f, 1.6f);
        }

        constexpr int encN = 4096;
        std::vector<juce::uint8> encTab ((size_t) encN + 1);
        for (int k = 0; k <= encN; ++k)
            encTab[(size_t) k] = (juce::uint8) std::lround (255.0f * std::sqrt ((float) k / (float) encN));
        auto enc = [&] (float c) { return encTab[(size_t) std::clamp ((int) (c * ((float) encN / range)), 0, encN)]; };

        auto rows = [&] (int j0, int j1)
        {
        for (int j = j0; j < j1; ++j)
        {
            const float y = floorY + height * ((float) j + 0.5f) / (float) texH;
            const float qy = (y - rackCentreY) / 3.4f, poolY = std::exp (-qy * qy);
            const float sy = std::sin (y * 3.1f), cy = std::cos (y * 3.1f), y23 = y * 23.0f;
            const float fadeY = (0.45f + 0.55f * smooth (0.0f, 1.2f, y - floorY)) * (1.0f - 0.60f * smooth (2.5f, 8.0f, y - rackCentreY));
            const float hazeY = 0.008f * (1.0f - smooth (4.0f, 11.0f, y - floorY));
            const float acrossY = (y * by + wallZ * bz) / bn;

            for (int i = 0; i < texW; ++i)
            {
                const auto& c = cols[(size_t) i];
                // sin (y * 3.1 + cell) by the angle-sum rule: no trig in the loop but one table read
                const float inner = sy * c.cc + cy * c.sc;
                const float grain = 0.93f + 0.07f * fastSin (y23 + c.phase + inner * 2.0f);
                const float shade = c.tone * grain * (0.18f + 0.82f * c.gap);

                // Light: the lamp's pool, the window (as lightCol / amb do for the hardware), a haze of the shafts
                const float pool = c.poolX * poolY;
                const float across = c.beamX + acrossY;
                const float beam = beamTab[(size_t) std::clamp ((int) ((across - acrossMin) / (acrossMax - acrossMin) * (float) (beamN - 1)), 0, beamN - 1)];
                const float lit = std::min (beam, 1.0f);
                const float lamp = 0.32f + 1.45f * pool, ambK = 0.22f * (0.58f + 0.42f * lit);
                const float lr = lamp + 0.42f * (0.62f + 1.48f * lit) + ambK * 0.55f;
                const float lg = 0.70f * lamp + 0.42f * (0.58f + 1.30f * lit) + ambK * 0.48f;
                const float lb = 0.42f * lamp + 0.42f * (0.60f + 0.98f * lit) + ambK * 0.51f;

                const float haze = hazeY * beam, fade = fadeY * c.fadeX;
                auto* p = px.data() + ((size_t) j * texW + (size_t) i) * 4;
                p[0] = enc ((0.20f * shade * lr + haze) * fade);
                p[1] = enc ((0.115f * shade * lg + haze * 0.90f) * fade);
                p[2] = enc ((0.068f * shade * lb + haze * 0.74f) * fade);
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
}
