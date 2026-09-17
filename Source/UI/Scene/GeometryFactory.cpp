#include "GeometryFactory.h"

namespace pad::geo
{
    using gfx::Vec3;
    using gfx::Mat4;
    using namespace layout;

    constexpr float pi = 3.14159265f;

    MeshData sweptRoundedRect (float halfW, float halfD, float radius, int segs,
                               const std::vector<ProfilePoint>& profile, bool capTop)
    {
        MeshData mesh;
        jassert (profile.size() >= 1);

        struct RingPoint { float cx, cz, dx, dz; };
        std::vector<RingPoint> ring;

        const float centres[4][2] { { halfW, halfD }, { -halfW, halfD }, { -halfW, -halfD }, { halfW, -halfD } };

        for (int c = 0; c < 4; ++c)
            for (int s = 0; s <= segs; ++s)
            {
                const float a = ((float) c + (float) s / (float) segs) * 0.5f * pi;
                ring.push_back ({ centres[c][0], centres[c][1], std::cos (a), std::sin (a) });
            }

        const auto n = ring.size();

        auto pointAt = [&] (const RingPoint& rp, float outset, float y)
        {
            const float r = std::max (0.0f, radius + outset);
            return Vec3 { rp.cx + rp.dx * r, y, rp.cz + rp.dz * r };
        };

        for (size_t k = 0; k + 1 < profile.size(); ++k)
        {
            const auto p0 = profile[k], p1 = profile[k + 1];
            const float dd = p1.outset - p0.outset, dy = p1.y - p0.y;
            const float len = std::max (1.0e-6f, std::sqrt (dd * dd + dy * dy));
            const float nOut = dy / len, nUp = -dd / len;

            const auto base = (juce::uint32) mesh.vertices.size();

            for (auto& rp : ring)
            {
                const Vec3 normal { rp.dx * nOut, nUp, rp.dz * nOut };
                const auto a = pointAt (rp, p0.outset, p0.y);
                const auto b = pointAt (rp, p1.outset, p1.y);
                mesh.addVertex (a, normal, a.x, a.z);
                mesh.addVertex (b, normal, b.x, b.z);
            }

            for (size_t j = 0; j < n; ++j)
            {
                const auto j1 = (j + 1) % n;
                mesh.addQuad (base + (juce::uint32) (j * 2), base + (juce::uint32) (j1 * 2),
                              base + (juce::uint32) (j1 * 2 + 1), base + (juce::uint32) (j * 2 + 1));
            }
        }

        if (capTop)
        {
            const auto last = profile.back();
            const auto centre = mesh.addVertex ({ 0, last.y, 0 }, { 0, 1, 0 }, 0, 0);
            const auto base = (juce::uint32) mesh.vertices.size();

            for (auto& rp : ring)
            {
                const auto p = pointAt (rp, last.outset, last.y);
                mesh.addVertex (p, { 0, 1, 0 }, p.x, p.z);
            }

            for (size_t j = 0; j < n; ++j)
                mesh.addTriangle (centre, base + (juce::uint32) j, base + (juce::uint32) ((j + 1) % n));
        }

        return mesh;
    }

    MeshData plateWithHoles (const Rect& outer, float y, const std::vector<Rect>& holes)
    {
        std::vector<float> xs { outer.minX(), outer.maxX() }, zs { outer.minZ(), outer.maxZ() };

        for (auto& h : holes)
        {
            xs.push_back (h.minX()); xs.push_back (h.maxX());
            zs.push_back (h.minZ()); zs.push_back (h.maxZ());
        }

        auto sortUnique = [] (std::vector<float>& v)
        {
            std::sort (v.begin(), v.end());
            v.erase (std::unique (v.begin(), v.end(), [] (float a, float b) { return std::abs (a - b) < 1.0e-5f; }), v.end());
        };

        sortUnique (xs);
        sortUnique (zs);

        MeshData mesh;

        auto emitQuad = [&] (float x0, float x1, float z0, float z1)
        {
            const Vec3 up { 0, 1, 0 };
            const auto a = mesh.addVertex ({ x0, y, z0 }, up, x0, z0);
            const auto b = mesh.addVertex ({ x1, y, z0 }, up, x1, z0);
            const auto c = mesh.addVertex ({ x1, y, z1 }, up, x1, z1);
            const auto d = mesh.addVertex ({ x0, y, z1 }, up, x0, z1);
            mesh.addQuad (a, d, c, b);
        };

        for (size_t zi = 0; zi + 1 < zs.size(); ++zi)
        {
            const float z0 = zs[zi], z1 = zs[zi + 1], zc = 0.5f * (z0 + z1);
            int spanStart = -1;

            for (size_t xi = 0; xi + 1 < xs.size(); ++xi)
            {
                const float xc = 0.5f * (xs[xi] + xs[xi + 1]);
                bool inside = false;

                for (auto& h : holes)
                    if (xc > h.minX() && xc < h.maxX() && zc > h.minZ() && zc < h.maxZ())
                        inside = true;

                if (! inside && spanStart < 0)
                    spanStart = (int) xi;

                if (inside && spanStart >= 0)
                {
                    emitQuad (xs[(size_t) spanStart], xs[xi], z0, z1);
                    spanStart = -1;
                }
            }

            if (spanStart >= 0)
                emitQuad (xs[(size_t) spanStart], xs.back(), z0, z1);
        }

        return mesh;
    }

    MeshData wellWalls (const Rect& h, float topY, float depth)
    {
        MeshData mesh;
        const float y0 = topY - depth, y1 = topY;

        auto wall = [&] (Vec3 a, Vec3 b, Vec3 normal)
        {
            const auto i0 = mesh.addVertex ({ a.x, y0, a.z }, normal, 0, 0);
            const auto i1 = mesh.addVertex ({ b.x, y0, b.z }, normal, 1, 0);
            const auto i2 = mesh.addVertex ({ b.x, y1, b.z }, normal, 1, 1);
            const auto i3 = mesh.addVertex ({ a.x, y1, a.z }, normal, 0, 1);
            mesh.addQuad (i0, i1, i2, i3);
        };

        wall ({ h.minX(), 0, h.minZ() }, { h.maxX(), 0, h.minZ() }, { 0, 0, 1 });
        wall ({ h.maxX(), 0, h.maxZ() }, { h.minX(), 0, h.maxZ() }, { 0, 0, -1 });
        wall ({ h.minX(), 0, h.maxZ() }, { h.minX(), 0, h.minZ() }, { 1, 0, 0 });
        wall ({ h.maxX(), 0, h.minZ() }, { h.maxX(), 0, h.maxZ() }, { -1, 0, 0 });
        return mesh;
    }

    MeshData horizontalQuad (const Rect& r, float y)
    {
        MeshData mesh;
        const Vec3 up { 0, 1, 0 };
        const auto a = mesh.addVertex ({ r.minX(), y, r.minZ() }, up, 0, 0);
        const auto b = mesh.addVertex ({ r.maxX(), y, r.minZ() }, up, 1, 0);
        const auto c = mesh.addVertex ({ r.maxX(), y, r.maxZ() }, up, 1, 1);
        const auto d = mesh.addVertex ({ r.minX(), y, r.maxZ() }, up, 0, 1);
        mesh.addQuad (a, d, c, b);
        return mesh;
    }

    MeshData box (Vec3 lo, Vec3 hi)
    {
        MeshData mesh;

        auto face = [&] (Vec3 n, Vec3 a, Vec3 b, Vec3 c, Vec3 d)
        {
            const auto i0 = mesh.addVertex (a, n, 0, 0);
            const auto i1 = mesh.addVertex (b, n, 1, 0);
            const auto i2 = mesh.addVertex (c, n, 1, 1);
            const auto i3 = mesh.addVertex (d, n, 0, 1);
            mesh.addQuad (i0, i1, i2, i3);
        };

        face ({ 0, 1, 0 },  { lo.x, hi.y, lo.z }, { hi.x, hi.y, lo.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z });
        face ({ 0, -1, 0 }, { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { lo.x, lo.y, hi.z });
        face ({ 0, 0, 1 },  { lo.x, lo.y, hi.z }, { hi.x, lo.y, hi.z }, { hi.x, hi.y, hi.z }, { lo.x, hi.y, hi.z });
        face ({ 0, 0, -1 }, { lo.x, lo.y, lo.z }, { hi.x, lo.y, lo.z }, { hi.x, hi.y, lo.z }, { lo.x, hi.y, lo.z });
        face ({ 1, 0, 0 },  { hi.x, lo.y, lo.z }, { hi.x, lo.y, hi.z }, { hi.x, hi.y, hi.z }, { hi.x, hi.y, lo.z });
        face ({ -1, 0, 0 }, { lo.x, lo.y, lo.z }, { lo.x, lo.y, hi.z }, { lo.x, hi.y, hi.z }, { lo.x, hi.y, lo.z });
        return mesh;
    }

    MeshData flatAnnulus (float r0, float r1, int segments)
    {
        MeshData mesh;
        for (int s = 0; s <= segments; ++s)
        {
            const float a = 2.0f * pi * (float) s / (float) segments;
            const float c = std::cos (a), sn = std::sin (a);
            mesh.addVertex ({ c * r0, 0, sn * r0 }, { 0, 1, 0 }, c * r0, sn * r0);
            mesh.addVertex ({ c * r1, 0, sn * r1 }, { 0, 1, 0 }, c * r1, sn * r1);
        }
        for (juce::uint32 s = 0; s < (juce::uint32) segments; ++s)
            mesh.addQuad (s * 2, s * 2 + 2, s * 2 + 3, s * 2 + 1);
        return mesh;
    }

    MeshData dome (float radius, float height, int segments, int rings)
    {
        MeshData mesh;

        for (int r = 0; r <= rings; ++r)
        {
            const float phi = 0.5f * pi * (float) r / (float) rings;
            const float ringR = radius * std::cos (phi), y = height * std::sin (phi);

            for (int s = 0; s <= segments; ++s)
            {
                const float a = 2.0f * pi * (float) s / (float) segments;
                const Vec3 n = gfx::normalise ({ std::cos (phi) * std::cos (a) / radius, std::sin (phi) / height, std::cos (phi) * std::sin (a) / radius });
                mesh.addVertex ({ ringR * std::cos (a), y, ringR * std::sin (a) }, n, (float) s / (float) segments, (float) r / (float) rings);
            }
        }

        const auto stride = (juce::uint32) segments + 1;
        for (juce::uint32 r = 0; r < (juce::uint32) rings; ++r)
            for (juce::uint32 s = 0; s < (juce::uint32) segments; ++s)
                mesh.addQuad (r * stride + s, r * stride + s + 1, (r + 1) * stride + s + 1, (r + 1) * stride + s);

        return mesh;
    }

    MeshData triangularBlade (float b, float h, float t)
    {
        MeshData mesh;
        const float tip = 0.012f, base = -0.02f;

        // Outline in (x, y), counter-clockwise; extruded along z by ±t
        const std::array<std::pair<float, float>, 4> outline {{ { -b, base }, { b, base }, { tip, h }, { -tip, h } }};

        for (float side : { 1.0f, -1.0f })
        {
            const Vec3 n { 0, 0, side };
            std::array<juce::uint32, 4> idx {};
            for (size_t i = 0; i < 4; ++i)
                idx[i] = mesh.addVertex ({ outline[i].first, outline[i].second, side * t }, n, outline[i].first, outline[i].second);
            mesh.addQuad (idx[0], idx[1], idx[2], idx[3]);
        }

        for (size_t i = 0; i < 4; ++i)
        {
            const auto p0 = outline[i], p1 = outline[(i + 1) % 4];
            const float dx = p1.first - p0.first, dy = p1.second - p0.second;
            const Vec3 n = gfx::normalise ({ dy, -dx, 0 });

            const auto a = mesh.addVertex ({ p0.first, p0.second, -t }, n, 0, 0);
            const auto bb = mesh.addVertex ({ p0.first, p0.second, t }, n, 1, 0);
            const auto c = mesh.addVertex ({ p1.first, p1.second, t }, n, 1, 1);
            const auto d = mesh.addVertex ({ p1.first, p1.second, -t }, n, 0, 1);
            mesh.addQuad (a, bb, c, d);
        }

        return mesh;
    }

    //==============================================================================
    // World-space parts
    MeshData chassisBody()
    {
        constexpr float r = 0.05f;
        const float halfD = 0.5f * chassisDepth;
        MeshData mesh;
        mesh.append (sweptRoundedRect (chassisHalfW - r, halfD - r, r, 3,
                                       { { 0.0f, chassisBottom }, { 0.0f, chassisTop - 0.03f }, { -0.03f, chassisTop } }, false),
                     Mat4::translation ({ 0.0f, 0.0f, chassisFrontZ - halfD }));
        return mesh;
    }

    static float chassisCentreZ() noexcept { return chassisFrontZ - 0.5f * chassisDepth; }

    // Lid parts are built with the lid surface at y = 0 (draw translated to chassisTop)
    MeshData lidTop()
    {
        std::vector<Rect> holes;
        for (int i = 0; i < numLidVents; ++i)
            holes.push_back (lidVent (i));

        return plateWithHoles ({ 0.0f, chassisCentreZ(), chassisHalfW - 0.03f, 0.5f * chassisDepth - 0.03f }, 0.0f, holes);
    }

    MeshData lidVentWalls()
    {
        MeshData mesh;
        for (int i = 0; i < numLidVents; ++i)
            mesh.append (wellWalls (lidVent (i), 0.0f, lidVentDepth));
        return mesh;
    }

    MeshData lidVentFloors()
    {
        MeshData mesh;
        for (int i = 0; i < numLidVents; ++i)
            mesh.append (horizontalQuad (lidVent (i), -lidVentDepth));
        return mesh;
    }

    MeshData feet()
    {
        MeshData mesh;
        for (float x : { -1.95f, 1.95f })
            for (float z : { chassisFrontZ - 0.25f, chassisBackZ + 0.25f })
                mesh.append (sweptRoundedRect (0, 0, 0.11f, 3, { { 0.0f, 0.0f }, { 0.0f, chassisBottom }, { -0.02f, chassisBottom + 0.005f } }, true),
                             Mat4::translation ({ x, 0, z }));
        return mesh;
    }

    MeshData tablePlane()
    {
        return horizontalQuad ({ 0.0f, 0.0f, 24.0f, 24.0f }, 0.0f);
    }

    MeshData unitQuad()
    {
        return horizontalQuad ({ 0.0f, 0.0f, 1.0f, 1.0f }, 0.0f);
    }

    //==============================================================================
    // Panel-local parts (y = out of the faceplate)
    static std::vector<Rect> faceplateHoles()
    {
        std::vector<Rect> holes { displayRect };
        for (auto& slot : earSlots)
            holes.push_back (slot);
        return holes;
    }

    MeshData faceplateEdges()
    {
        constexpr float r = 0.022f;
        return sweptRoundedRect (faceHalfW - r, faceHalfH - r, r, 2,
                                 { { 0.0f, -faceThick }, { 0.0f, -0.016f }, { -0.016f, 0.0f } }, false);
    }

    MeshData faceplateTop()
    {
        return plateWithHoles ({ 0.0f, 0.0f, faceHalfW - 0.016f, faceHalfH - 0.016f }, 0.0f, faceplateHoles());
    }

    MeshData displayWalls()  { return wellWalls (displayRect, 0.0f, displayDepth); }
    MeshData displayGlass()  { return horizontalQuad (displayRect, -displayDepth); }

    MeshData displayBezel()
    {
        // Frame around the display cutout. Profile runs outer edge -> inner lip so normals face correctly.
        constexpr float r = 0.02f;
        MeshData mesh;
        mesh.append (sweptRoundedRect (displayRect.hw - r, displayRect.hd - r, r, 3,
                                       { { 0.07f, 0.0f }, { 0.07f, 0.012f }, { 0.055f, 0.026f }, { 0.02f, 0.026f }, { 0.0f, 0.012f }, { 0.0f, 0.0f } }, false),
                     Mat4::translation ({ displayRect.cx, 0.0f, displayRect.cz }));
        return mesh;
    }

    MeshData earSlotWalls()
    {
        MeshData mesh;
        for (auto& slot : earSlots)
            mesh.append (wellWalls (slot, 0.0f, faceThick));
        return mesh;
    }

    MeshData earSlotFloors()
    {
        MeshData mesh;
        for (auto& slot : earSlots)
            mesh.append (horizontalQuad (slot, -faceThick));
        return mesh;
    }

    MeshData screwHeads()
    {
        MeshData mesh;
        const auto head = sweptRoundedRect (0, 0, 0.05f, 3, { { 0.0f, 0.0f }, { 0.0f, 0.012f }, { -0.02f, 0.03f } }, true);
        for (auto& slot : earSlots)
            mesh.append (head, Mat4::translation ({ slot.cx + (slot.cx > 0 ? -0.02f : 0.02f), 0.0f, slot.cz }));
        return mesh;
    }

    //==============================================================================
    // Knob (local to knob centre)
    MeshData knobBezel()
    {
        return sweptRoundedRect (0, 0, bezelRadius, 9, { { 0.0f, 0.0f }, { 0.0f, 0.010f }, { -0.012f, 0.020f } }, true);
    }

    MeshData knobSkirt()
    {
        return sweptRoundedRect (0, 0, skirtRadius, 10,
                                 { { 0.0f, 0.018f }, { -0.012f, 0.058f }, { -0.030f, dialTop } }, true);
    }

    MeshData knobCap()
    {
        return sweptRoundedRect (0, 0, capRadius, 10,
                                 { { 0.0f, dialTop - 0.005f }, { 0.0f, capTop - 0.022f }, { -0.022f, capTop } }, true);
    }

    MeshData knobCapInsert()
    {
        return sweptRoundedRect (0, 0, capRadius - 0.028f, 8, { { 0.0f, capTop - 0.008f }, { 0.0f, capTop + 0.006f } }, true);
    }

    //==============================================================================
    // Toggle switch (local to switch centre)
    MeshData switchPlate()
    {
        constexpr float r = 0.035f;
        return sweptRoundedRect (switchPlateHalfW - r, switchPlateHalfD - r, r, 3,
                                 { { 0.0f, 0.0f }, { 0.0f, 0.012f }, { -0.012f, 0.022f } }, true);
    }

    MeshData switchBushing()
    {
        MeshData mesh;
        // Round washer from the pivot out towards the rectangular plate, then the threaded bushing + nut
        mesh.append (sweptRoundedRect (0, 0, 0.095f, 8, { { 0.0f, 0.020f }, { 0.0f, 0.030f }, { -0.012f, 0.038f } }, true));
        mesh.append (sweptRoundedRect (0, 0, 0.068f, 3, { { 0.0f, 0.036f }, { 0.0f, 0.062f }, { -0.010f, 0.070f } }, true));
        mesh.append (sweptRoundedRect (0, 0, 0.048f, 8, { { 0.0f, 0.068f }, { 0.0f, 0.100f }, { -0.008f, 0.108f } }, true));
        return mesh;
    }

    MeshData switchLever()
    {
        // Rectangular chrome pole rising from the pivot, gently tapered, flat chamfered end
        constexpr float halfW = 0.024f, halfD = 0.016f, r = 0.005f, length = 0.36f;
        return sweptRoundedRect (halfW - r, halfD - r, r, 2,
                                 { { -r, -0.03f }, { 0.0f, -0.03f }, { -0.005f, length - 0.012f }, { -0.009f, length } }, true);
    }
}
