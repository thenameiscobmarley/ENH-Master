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
    MeshData chassisBody()
    {
        constexpr float r = 0.07f;
        return sweptRoundedRect (bodyHalfW - r, bodyHalfD - r, r, 4,
                                 { { 0.0f, bodyBottom }, { 0.0f, bodyTop - 0.035f }, { -0.035f, bodyTop } }, false); // top is covered by the panel; open so cutouts show their wells
    }

    MeshData rackEars()
    {
        MeshData mesh;

        for (float side : { -1.0f, 1.0f })
        {
            const float x0 = side > 0 ? bodyHalfW - 0.03f : -earOuterX;
            const float x1 = side > 0 ? earOuterX : -bodyHalfW + 0.03f;
            mesh.append (box ({ x0, bodyBottom, bodyHalfD - earThick }, { x1, bodyTop - 0.01f, bodyHalfD + 0.005f }));

            // Carry handle: two standoffs and a bar
            const float hx = side * 2.63f;
            for (float y : { 0.17f, 0.50f })
                mesh.append (box ({ hx - 0.035f, y - 0.03f, bodyHalfD }, { hx + 0.035f, y + 0.03f, bodyHalfD + 0.14f }));

            mesh.append (sweptRoundedRect (0, 0, 0.035f, 3, { { -0.035f, 0.14f }, { 0.0f, 0.14f }, { 0.0f, 0.53f }, { -0.035f, 0.53f } }, false),
                         Mat4::translation ({ hx, 0.0f, bodyHalfD + 0.14f }));
        }

        return mesh;
    }

    MeshData feet()
    {
        MeshData mesh;
        for (float x : { -2.2f, 2.2f })
            for (float z : { -1.1f, 1.1f })
                mesh.append (sweptRoundedRect (0, 0, 0.12f, 3, { { 0.0f, 0.0f }, { 0.0f, bodyBottom } }, false),
                             Mat4::translation ({ x, 0, z }));
        return mesh;
    }

    MeshData panelEdges()
    {
        constexpr float r = 0.012f;
        return sweptRoundedRect (panelHalfW - r, panelHalfD - r, r, 2,
                                 { { 0.0f, bodyTop - 0.01f }, { 0.0f, layout::panelTop - 0.009f }, { -0.009f, layout::panelTop } }, false);
    }

    static std::vector<Rect> panelHoles()
    {
        std::vector<Rect> holes { scopeRect };

        for (int i = 0; i < numVentSlots; ++i)
            holes.push_back (ventSlot (i));

        for (auto& c : controls)
            if (c.kind == ControlKind::toggle)
                holes.push_back (switchWell (c));

        return holes;
    }

    MeshData panelTop()
    {
        const Rect outer { 0.0f, 0.0f, panelHalfW - 0.009f, panelHalfD - 0.009f };
        return plateWithHoles (outer, layout::panelTop, panelHoles());
    }

    MeshData screws()
    {
        MeshData mesh;
        const auto head = dome (0.04f, 0.014f, 10, 2);

        for (float sx : { -1.0f, 1.0f })
            for (float sz : { -1.0f, 1.0f })
                mesh.append (head, Mat4::translation ({ sx * (panelHalfW - 0.09f), layout::panelTop, sz * (panelHalfD - 0.09f) }));

        for (float sx : { -1.0f, 1.0f })
            for (float y : { 0.17f, 0.50f })
                mesh.append (head, Mat4::translation ({ sx * 2.72f, y, bodyHalfD + 0.005f }) * Mat4::rotationX (0.5f * pi));

        return mesh;
    }

    MeshData knobFlange()
    {
        return sweptRoundedRect (0, 0, flangeRadius, 8,
                                 { { 0.0f, 0.0f }, { -0.012f, 0.035f }, { -0.022f, flangeTop } }, true);
    }

    MeshData knobCapBody()
    {
        return sweptRoundedRect (0, 0, capRadius, 7,
                                 { { 0.0f, flangeTop - 0.01f }, { 0.0f, capTop - 0.024f }, { -0.02f, capTop } }, true);
    }

    MeshData knobCapInsert()
    {
        return sweptRoundedRect (0, 0, 0.148f, 7, { { 0.0f, capTop - 0.01f }, { 0.0f, capTop + 0.004f } }, true);
    }

    MeshData knobPointer()
    {
        MeshData mesh;
        mesh.append (box ({ -0.010f, capTop + 0.004f, -0.140f }, { 0.010f, capTop + 0.008f, -0.035f }));
        mesh.append (box ({ -0.011f, flangeTop + 0.02f, -capRadius - 0.004f }, { 0.011f, capTop - 0.03f, -capRadius + 0.01f }));
        return mesh;
    }

    MeshData switchWellWalls()
    {
        MeshData mesh;
        for (auto& c : controls)
            if (c.kind == ControlKind::toggle)
                mesh.append (wellWalls (switchWell (c), layout::panelTop, switchWellDepth));
        return mesh;
    }

    MeshData switchWellFloors()
    {
        MeshData mesh;
        for (auto& c : controls)
            if (c.kind == ControlKind::toggle)
                mesh.append (horizontalQuad (switchWell (c), layout::panelTop - switchWellDepth));
        return mesh;
    }

    MeshData switchBlade()
    {
        return triangularBlade (0.072f, 0.23f, 0.018f);
    }

    MeshData switchHub()
    {
        auto cyl = sweptRoundedRect (0, 0, 0.034f, 3,
                                     { { -0.034f, -0.08f }, { 0.0f, -0.08f }, { 0.0f, 0.08f } }, true);
        MeshData mesh;
        mesh.append (cyl, Mat4::rotationZ (0.5f * pi));
        return mesh;
    }

    MeshData scopeWalls()
    {
        return wellWalls (scopeRect, layout::panelTop, scopeDepth);
    }

    MeshData scopeGlass()
    {
        return horizontalQuad (scopeRect, layout::panelTop - scopeDepth);
    }

    MeshData ventWalls()
    {
        MeshData mesh;
        for (int i = 0; i < numVentSlots; ++i)
            mesh.append (wellWalls (ventSlot (i), layout::panelTop, ventDepth));
        return mesh;
    }

    MeshData ventFloors()
    {
        MeshData mesh;
        for (int i = 0; i < numVentSlots; ++i)
            mesh.append (horizontalQuad (ventSlot (i), layout::panelTop - ventDepth));
        return mesh;
    }

    MeshData tablePlane()
    {
        return horizontalQuad ({ 0.0f, 0.0f, 24.0f, 24.0f }, 0.0f);
    }
}
