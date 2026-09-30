#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <thread>

/*  The wall behind the rack: a room in a house, in the daytime, baked once into a texture (the studioWall
    material only reads it back). Warm plaster, lit by the day; behind the rack a tall window in a painted
    white frame (three lights across, a transom, a deep sill); and through it, a little out of focus (the
    camera is focused on the rack), a view: sky and soft clouds, hills fading into haze, woods, a river
    with an old stone arch bridge over it and a slender suspension bridge far off, the water catching the
    sky. The daylight spills round the window onto the plaster, brightest at its edges.

    Painted in code, once, when the window opens (nothing fetched, nothing to license); read back as one
    mipmapped texture fetch - no cost per frame. Stored as sqrt (colour / range) for precision in the
    dark: decode c * c * range. The window's glass (the view) is far brighter than the room, as outdoors is. */
namespace pad::studiowall
{
    inline constexpr float x0 = -12.0f, width = 24.0f, height = 12.0f;   // world metres covered (y from the floor)
    inline constexpr float range = 4.0f;
    inline constexpr int texW = 2048, texH = 1024;

    /** Where the window is, on the wall (world x, and height above the floor). */
    inline constexpr float winX0 = -7.6f, winX1 = 7.6f, winY0 = 1.6f, winY1 = 11.4f, frameW = 0.34f;

    namespace detail
    {
        inline float smooth (float e0, float e1, float x) noexcept
        {
            const float t = std::clamp ((x - e0) / (e1 - e0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
        inline float hash (int x, int y) noexcept
        {
            const float h = std::sin ((float) x * 127.1f + (float) y * 311.7f) * 43758.5453f;
            return h - std::floor (h);
        }
        inline float noise (float x, float y) noexcept
        {
            const int ix = (int) std::floor (x), iy = (int) std::floor (y);
            const float fx = x - (float) ix, fy = y - (float) iy;
            const float ux = fx * fx * (3.0f - 2.0f * fx), uy = fy * fy * (3.0f - 2.0f * fy);
            const float a = hash (ix, iy) + (hash (ix + 1, iy) - hash (ix, iy)) * ux;
            const float b = hash (ix, iy + 1) + (hash (ix + 1, iy + 1) - hash (ix, iy + 1)) * ux;
            return a + (b - a) * uy;
        }
        inline float fbm (float x, float y, int oct) noexcept
        {
            float s = 0.0f, a = 0.5f, n = 0.0f;
            for (int o = 0; o < oct; ++o) { s += a * noise (x, y); n += a; x = x * 2.03f + 17.1f; y = y * 2.03f + 9.7f; a *= 0.5f; }
            return s / n;
        }

        /** The view out of the window, painted: u across (0 .. 1), v up (0 .. 1), linear colour. */
        inline std::array<float, 3> view (float u, float v) noexcept
        {
            // Sky: deep blue high up, pale and warm toward the horizon, soft cumulus
            const float horizon = 0.40f;
            const float t = std::clamp ((v - horizon) / (1.0f - horizon), 0.0f, 1.0f);
            float r = 1.55f + (0.62f - 1.55f) * std::pow (t, 0.7f);
            float g = 1.95f + (1.05f - 1.95f) * std::pow (t, 0.7f);
            float b = 2.35f + (2.10f - 2.35f) * std::pow (t, 0.7f);
            {
                const float cl = fbm (u * 5.0f, v * 11.0f - u * 1.5f, 5);
                const float cloud = smooth (0.52f, 0.72f, cl) * smooth (horizon + 0.02f, horizon + 0.25f, v);
                const float lit = 0.8f + 0.4f * smooth (0.5f, 0.8f, fbm (u * 9.0f + 3.0f, v * 18.0f, 3));
                r += cloud * (2.8f * lit - r) * 0.85f; g += cloud * (2.75f * lit - g) * 0.85f; b += cloud * (2.7f * lit - b) * 0.85f;
            }
            // Hills far off: blue in the haze
            const float hill = horizon + 0.035f + 0.05f * fbm (u * 3.0f, 1.3f, 4);
            if (v < hill)
            {
                const float k = 0.55f;
                r = r * (1.0f - k) + 0.72f * k; g = g * (1.0f - k) + 0.95f * k; b = b * (1.0f - k) + 1.20f * k;
            }
            // Woods: two bands of tree crowns, the near ones larger and darker, lit from above
            auto crowns = [] (float uu, float scale, float base, float amp) { return base + amp * (0.55f * fbm (uu * scale, 3.1f, 4) + 0.45f * std::abs (std::sin (uu * scale * 2.3f))); };
            const float farTrees = crowns (u, 6.0f, horizon - 0.01f, 0.07f);
            const float nearTrees = crowns (u + 0.37f, 3.2f, 0.27f, 0.14f);
            if (v < farTrees)
            {
                const float leaf = fbm (u * 70.0f, v * 70.0f, 3), top = smooth (farTrees - 0.05f, farTrees, v);
                r = 0.20f + 0.18f * leaf + 0.20f * top; g = 0.36f + 0.24f * leaf + 0.26f * top; b = 0.30f + 0.12f * leaf + 0.18f * top;   // (blued by distance)
            }
            // The river, reflecting the sky, with ripples, below the far woods
            const bool water = v < 0.20f && v > 0.06f + 0.02f * std::sin (u * 7.0f);
            if (water)
            {
                const float ripple = 0.85f + 0.3f * fbm (u * 90.0f, v * 400.0f, 2);
                r = 1.10f * ripple; g = 1.45f * ripple; b = 1.85f * ripple;
            }
            // The stone arch bridge across the river, left of centre: a deck, parapet and three arches
            {
                const float bx0 = 0.18f, bx1 = 0.52f, deck = 0.215f, deckT = 0.028f;
                if (u > bx0 && u < bx1 && v < deck + deckT && v > 0.10f)
                {
                    const float span = (bx1 - bx0) / 3.0f, k = std::fmod (u - bx0, span) / span - 0.5f;
                    const float archTop = deck - 0.015f - 0.06f * (1.0f - 4.0f * k * k);
                    const bool open = v < archTop;
                    if (! open)
                    {
                        const float stone = 0.8f + 0.25f * fbm (u * 220.0f, v * 220.0f, 2);
                        const float lit = v > deck ? 1.15f : 0.95f;
                        r = 0.95f * stone * lit; g = 0.86f * stone * lit; b = 0.72f * stone * lit;
                    }
                    else if (water)   // the arches' reflections in the water: darker rings
                    { r *= 0.75f; g *= 0.78f; b *= 0.82f; }
                }
            }
            // The suspension bridge far off to the right: two towers, the cables' sweep, a thin deck
            {
                const float t0 = 0.66f, t1 = 0.92f, deckV = horizon - 0.006f;
                const float ww = 0.0035f;
                const bool tower = (std::abs (u - t0) < ww || std::abs (u - t1) < ww) && v > deckV - 0.02f && v < deckV + 0.10f;
                bool cable = false;
                if (u > t0 - 0.12f && u < t1 + 0.12f)
                {
                    const float mid = 0.5f * (t0 + t1), half = 0.5f * (t1 - t0);
                    const float x = (u - mid) / half;
                    const float sag = std::abs (x) <= 1.0f ? deckV + 0.012f + 0.088f * x * x : deckV + 0.10f - 0.10f * std::min (1.0f, (std::abs (x) - 1.0f) * half / 0.12f);
                    cable = std::abs (v - sag) < 0.0022f;
                }
                const bool deckLine = u > t0 - 0.12f && u < t1 + 0.12f && std::abs (v - deckV) < 0.0035f;
                if (tower || cable || deckLine)
                { r = r * 0.35f + 0.55f; g = g * 0.35f + 0.52f; b = b * 0.35f + 0.55f; }
            }
            // The near trees in front of it all, and the bank
            if (v < nearTrees && (u < 0.12f || u > 0.84f || v < 0.075f))
            {
                const float leaf = fbm (u * 45.0f, v * 45.0f, 4), top = smooth (nearTrees - 0.08f, nearTrees, v);
                r = 0.10f + 0.16f * leaf + 0.30f * top; g = 0.19f + 0.26f * leaf + 0.40f * top; b = 0.07f + 0.08f * leaf + 0.12f * top;
            }
            if (v < 0.06f + 0.02f * std::sin (u * 7.0f) && ! (v > 0.06f))
            {
                const float grass = 0.8f + 0.3f * fbm (u * 120.0f, v * 60.0f, 2);
                r = 0.28f * grass; g = 0.46f * grass; b = 0.16f * grass;
            }
            return { r, g, b };
        }
    }

    inline std::vector<juce::uint8> bake (float wallZ, float floorY, float rackCentreY)
    {
        using namespace detail;
        (void) wallZ; (void) rackCentreY;
        std::vector<juce::uint8> px ((size_t) (texW * texH * 4));

        // The view, painted at its own resolution, then blurred (out of focus: the camera is on the rack)
        const int vw = 768, vh = 512;
        std::vector<std::array<float, 3>> img ((size_t) (vw * vh)), tmp ((size_t) (vw * vh));
        {
            auto rowsV = [&] (int j0, int j1) { for (int j = j0; j < j1; ++j) for (int i = 0; i < vw; ++i) img[(size_t) (j * vw + i)] = view (((float) i + 0.5f) / (float) vw, ((float) j + 0.5f) / (float) vh); };
            const int threads = (int) std::clamp (std::thread::hardware_concurrency(), 1u, 8u);
            std::vector<std::thread> workers;
            for (int t = 1; t < threads; ++t) workers.emplace_back (rowsV, vh * t / threads, vh * (t + 1) / threads);
            rowsV (0, vh / threads);
            for (auto& th : workers) th.join();
        }
        {
            // a box blur, three passes each way (close to a Gaussian): radius 3 px
            const int rad = 3;
            for (int pass = 0; pass < 3; ++pass)
            {
                for (int j = 0; j < vh; ++j)
                    for (int i = 0; i < vw; ++i)
                    {
                        std::array<float, 3> s {}; int n = 0;
                        for (int k = -rad; k <= rad; ++k) { const int x = std::clamp (i + k, 0, vw - 1); const auto& c = img[(size_t) (j * vw + x)]; s[0] += c[0]; s[1] += c[1]; s[2] += c[2]; ++n; }
                        tmp[(size_t) (j * vw + i)] = { s[0] / (float) n, s[1] / (float) n, s[2] / (float) n };
                    }
                for (int j = 0; j < vh; ++j)
                    for (int i = 0; i < vw; ++i)
                    {
                        std::array<float, 3> s {}; int n = 0;
                        for (int k = -rad; k <= rad; ++k) { const int y = std::clamp (j + k, 0, vh - 1); const auto& c = tmp[(size_t) (y * vw + i)]; s[0] += c[0]; s[1] += c[1]; s[2] += c[2]; ++n; }
                        img[(size_t) (j * vw + i)] = { s[0] / (float) n, s[1] / (float) n, s[2] / (float) n };
                    }
            }
        }
        auto viewAt = [&] (float u, float v)   // bilinear
        {
            const float x = std::clamp (u * (float) vw - 0.5f, 0.0f, (float) vw - 1.001f), y = std::clamp (v * (float) vh - 0.5f, 0.0f, (float) vh - 1.001f);
            const int ix = (int) x, iy = (int) y; const float fx = x - (float) ix, fy = y - (float) iy;
            std::array<float, 3> o {};
            for (int c = 0; c < 3; ++c)
            {
                const float a = img[(size_t) (iy * vw + ix)][(size_t) c] * (1 - fx) + img[(size_t) (iy * vw + ix + 1)][(size_t) c] * fx;
                const float b = img[(size_t) ((iy + 1) * vw + ix)][(size_t) c] * (1 - fx) + img[(size_t) ((iy + 1) * vw + ix + 1)][(size_t) c] * fx;
                o[(size_t) c] = a * (1 - fy) + b * fy;
            }
            return o;
        };

        constexpr int encN = 8192;
        std::vector<juce::uint8> encTab ((size_t) encN + 1);
        for (int k = 0; k <= encN; ++k)
            encTab[(size_t) k] = (juce::uint8) std::lround (255.0f * std::sqrt ((float) k / (float) encN));
        auto enc = [&] (float c) { return encTab[(size_t) std::clamp ((int) (c * ((float) encN / range)), 0, encN)]; };

        auto rows = [&] (int j0, int j1)
        {
            for (int j = j0; j < j1; ++j)
            {
                const float y = height * ((float) j + 0.5f) / (float) texH;   // above the floor
                for (int i = 0; i < texW; ++i)
                {
                    const float x = x0 + width * ((float) i + 0.5f) / (float) texW;
                    float r, g, b;
                    const bool inX = x > winX0 && x < winX1, inY = y > winY0 && y < winY1;
                    const float ux = (x - winX0) / (winX1 - winX0), vy = (y - winY0) / (winY1 - winY0);
                    const float bar = std::min ({ std::abs (x - winX0), std::abs (x - winX1), std::abs (y - winY0), std::abs (y - winY1) });
                    const float third = (winX1 - winX0) / 3.0f;
                    const float mull = std::min (std::abs (x - (winX0 + third)), std::abs (x - (winX0 + 2.0f * third)));
                    const float transom = std::abs (y - (winY0 + 0.72f * (winY1 - winY0)));
                    const float barDist = inX && inY ? std::min ({ bar, mull * 2.0f, transom * 2.0f }) : 1.0e9f;   // (glass: how far from the nearest bar)
                    const bool frame = (inX && inY && (bar < frameW || mull < 0.5f * frameW || transom < 0.5f * frameW))
                                    || (! (inX && inY) && x > winX0 - frameW && x < winX1 + frameW && y > winY0 - frameW && y < winY1 + frameW);
                    const float sillTop = winY0 - frameW, sillBot = sillTop - 0.40f;
                    const bool sill = y > sillBot && y <= sillTop && x > winX0 - frameW - 0.45f && x < winX1 + frameW + 0.45f;

                    // Plaster everywhere first: warm off-white, mottled, lit by the window (brightest beside it),
                    // darker toward the ceiling and the room's far corners
                    const float mottle = 0.93f + 0.05f * fbm (x * 1.6f, y * 1.6f, 3) + 0.03f * noise (x * 22.0f, y * 22.0f);
                    const float wdx = x < winX0 ? winX0 - x : x > winX1 ? x - winX1 : 0.0f;
                    const float wdy = y < winY0 ? winY0 - y : y > winY1 ? y - winY1 : 0.0f;
                    const float spill = std::exp (-(wdx * wdx + wdy * wdy) / 16.0f);
                    float light = (0.46f + 0.85f * spill) * (0.62f + 0.38f * smooth (0.0f, 2.2f, y)) * (1.0f - 0.34f * smooth (8.5f, 12.0f, y));
                    light *= 1.0f - 0.35f * smooth (8.0f, 12.0f, std::abs (x));
                    // the sill's shadow on the wall below it
                    if (y < sillBot && y > sillBot - 0.9f && x > winX0 - frameW - 0.45f && x < winX1 + frameW + 0.45f) light *= 0.82f + 0.18f * smooth (0.0f, 0.9f, sillBot - y);
                    r = 0.91f * mottle * light; g = 0.85f * mottle * light; b = 0.75f * mottle * light;

                    if (inX && inY && ! frame)
                    {
                        // The view, a little shaded next to the bars (their depth), with a faint sheen of the room on the glass
                        const auto c = viewAt (ux, vy);
                        const float nearBar = 0.80f + 0.20f * smooth (0.0f, 0.22f, barDist - 0.5f * frameW);
                        const float sheen = 0.035f * std::exp (-std::pow ((ux * 0.9f - vy + 0.25f) / 0.10f, 2.0f));
                        r = (0.92f * c[0] + 0.02f) * nearBar + sheen; g = (0.93f * c[1] + 0.02f) * nearBar + sheen; b = (0.95f * c[2] + 0.02f) * nearBar + sheen;
                    }
                    else if (frame)
                    {
                        // White painted wood with a bevel: a lit edge toward the room's light, a shadowed one away,
                        // a fine dark line where it meets the glass
                        const float inner = inX && inY ? 1.0f : 0.0f;
                        const float d = inX && inY ? std::min ({ bar, mull, transom }) : std::min ({ std::abs (x - (winX0 - frameW)), std::abs (x - (winX1 + frameW)), std::abs (y - (winY0 - frameW)), std::abs (y - (winY1 + frameW)) });
                        const float bevel = smooth (0.0f, 0.06f, d);
                        const float putty = inner > 0.5f ? 1.0f - 0.35f * smooth (0.03f, 0.0f, std::abs (d - (bar < frameW ? frameW : 0.5f * frameW))) : 1.0f;
                        const float lit = (0.78f + 0.18f * inner) * (0.82f + 0.18f * bevel) * putty;
                        r = 0.90f * lit; g = 0.88f * lit; b = 0.84f * lit;
                    }
                    else if (sill)
                    {
                        // The sill: its top catching the day, its front edge in shade
                        const float t = (y - sillBot) / (sillTop - sillBot);
                        const float lit = t > 0.35f ? 1.02f : 0.62f + 0.2f * t;
                        r = 0.90f * lit; g = 0.88f * lit; b = 0.84f * lit;
                    }

                    // Linen curtains either side of the window, gathered in soft folds, the day glowing through them
                    // where they hang over the glass; a thin rail above
                    {
                        const float cl0 = winX0 - 1.9f, cl1 = winX0 + 1.05f, cr0 = winX1 - 1.05f, cr1 = winX1 + 1.9f;
                        const float top = winY1 + 0.30f, hem = 0.95f + 0.05f * std::sin (x * 3.1f);   // (below the texture's top edge: it clamps)
                        const bool onL = x > cl0 && x < cl1, onR = x > cr0 && x < cr1;
                        if ((onL || onR) && y < top && y > hem)
                        {
                            const float x0c = onL ? cl0 : cr0;
                            const float fold = std::sin ((x - x0c) * 6.2832f / 0.52f + 0.35f * std::sin (y * 0.45f));
                            const float shade = 0.74f + 0.20f * fold + 0.06f * noise (x * 30.0f, y * 4.0f);
                            const float edgeSide = onL ? smooth (cl1, cl1 - 0.25f, x) : smooth (cr0, cr0 + 0.25f, x);   // the inner hem
                            float cr = 0.86f * shade * (light * 1.05f + 0.05f), cg = 0.82f * shade * (light * 1.05f + 0.05f), cb = 0.74f * shade * (light * 1.05f + 0.05f);
                            if (inX && inY)   // over the glass: the day behind shows through the weave, warm
                            {
                                const auto c = viewAt (ux, vy);
                                const float through = 0.30f;
                                cr += through * (0.6f * c[0] + 0.5f); cg += through * (0.6f * c[1] + 0.45f); cb += through * (0.6f * c[2] + 0.35f);
                            }
                            const float a = std::min (1.0f, 0.4f + edgeSide);
                            r = r + (cr - r) * a; g = g + (cg - g) * a; b = b + (cb - b) * a;
                        }
                        if (y > top && y < top + 0.08f && x > cl0 - 0.2f && x < cr1 + 0.2f) { r = 0.16f; g = 0.14f; b = 0.12f; }   // the rail
                    }

                    // A framed print on the right wall: black frame, white mat, a waveform in ink; a soft shadow
                    {
                        const float fx0 = 9.95f, fx1 = 11.75f, fy0 = 5.30f, fy1 = 7.70f, fw = 0.07f, mat = 0.22f;
                        if (x > fx0 + 0.06f && x < fx1 + 0.14f && y < fy0 - 0.02f && y > fy0 - 0.20f) { r *= 0.8f; g *= 0.8f; b *= 0.8f; }   // (its shadow)
                        if (x > fx0 && x < fx1 && y > fy0 && y < fy1)
                        {
                            const bool fr = x < fx0 + fw || x > fx1 - fw || y < fy0 + fw || y > fy1 - fw;
                            const bool mt = ! fr && (x < fx0 + fw + mat || x > fx1 - fw - mat || y < fy0 + fw + mat || y > fy1 - fw - mat);
                            if (fr) { r = 0.05f; g = 0.05f; b = 0.055f; }
                            else if (mt) { r = 0.90f * light * 1.1f; g = 0.89f * light * 1.1f; b = 0.86f * light * 1.1f; }
                            else
                            {
                                const float px0 = fx0 + fw + mat, px1 = fx1 - fw - mat, py0 = fy0 + fw + mat, py1 = fy1 - fw - mat;
                                const float u = (x - px0) / (px1 - px0), v = (y - py0) / (py1 - py0);
                                const float wave = 0.5f + 0.28f * std::sin (u * 21.0f) * std::sin (u * 3.1f + 0.4f) * (0.4f + 0.6f * std::sin (u * 3.14159f));
                                const float ink = smooth (0.018f, 0.004f, std::abs (v - wave));
                                const float paper = 0.80f + 0.06f * v;
                                r = (paper - 0.72f * ink) * light * 1.1f; g = (paper - 0.70f * ink) * light * 1.1f; b = (paper * 0.96f - 0.62f * ink) * light * 1.1f;
                            }
                        }
                    }

                    // The quartz upstand where the wall meets the shelf, its top edge catching the light
                    if (y < 0.32f)
                    {
                        const float vein = std::abs (std::sin (x * 1.9f + std::sin (x * 0.7f) * 2.0f + y * 3.0f));
                        const float q = (0.84f + 0.04f * vein) * (0.55f + 0.45f * light);
                        const float lip = smooth (0.26f, 0.31f, y);
                        r = q * (0.96f + 0.30f * lip); g = q * (0.95f + 0.30f * lip); b = q * (0.93f + 0.30f * lip);
                    }
                    else if (y < 0.40f) { r *= 0.86f; g *= 0.86f; b *= 0.86f; }   // (a soft line of shadow over it)
                    auto* p = px.data() + ((size_t) j * texW + (size_t) i) * 4;
                    p[0] = enc (r); p[1] = enc (g); p[2] = enc (b); p[3] = 255;
                }
            }
        };
        (void) floorY;
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
