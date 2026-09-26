#include "GeometryFactory.h"
#include <algorithm>

namespace pad::geo
{
    using gfx::Vec3;
    using gfx::Mat4;
    using namespace layout;

    using namespace hwk::geo;
    using layout::pi;

    //==============================================================================
    /** An ear screw as it really looks: a thin washer, then a smooth pan head (the Phillips recess is
        drawn separately, sitting on its crown). 128 segments round, so it stays round up close. */
    static MeshData rackScrewHead()
    {
        MeshData mesh;
        mesh.append (lathe (0.058f, { { 0.0f, 0.0f }, { 0.0f, 0.004f }, { -0.003f, 0.0055f } }, 128, true));
        mesh.append (lathe (0.049f, { { 0.0f, 0.004f }, { 0.0f, 0.013f }, { -0.005f, 0.022f }, { -0.014f, 0.029f },
                                      { -0.026f, 0.0325f }, { -0.040f, 0.0335f } }, 128, true));
        return mesh;
    }

    //==============================================================================
    // Unit body: panel-local, so it curves with its unit. The panel is at y = 0 and the
    // body runs back toward -y; +z is down the panel.
    MeshData unitBody (float halfH, float halfW)
    {
        constexpr float r = 0.045f;
        const float d = unitBodyDepth;
        MeshData mesh;

        // Swept profile: a small chamfer at the front, a seam a third of the way back, and a
        // chamfer at the rear, so the body reads as folded metal rather than a plain box.
        mesh.append (sweptRoundedRect (halfW - r, halfH - r, r, 3,
                                       { { 0.0f, 0.0f }, { 0.0f, -0.012f }, { -0.012f, -0.028f },
                                         { -0.012f, -d * 0.34f }, { -0.019f, -d * 0.36f },
                                         { -0.019f, -d * 0.40f }, { -0.012f, -d * 0.42f },
                                         { -0.012f, -d + 0.05f }, { -0.055f, -d } }, false));
        return mesh;
    }

    /** Slots in the top face of a unit's body (panel-local; the top face is at z = -halfH). */
    static std::vector<Rect> ventRects (float halfH)
    {
        std::vector<Rect> holes;
        for (int i = 0; i < numLidVents; ++i)
        {
            auto v = lidVent (i);
            // The vent field is laid out in (x, depth); place it on the top face of this unit
            holes.push_back ({ v.cx, v.cz, v.hw, v.hd });
        }
        (void) halfH;
        return holes;
    }

    MeshData unitVents (float halfH)
    {
        return plateWithHoles ({ 0.0f, -unitBodyDepth * 0.5f - 0.02f, chassisHalfW - 0.05f, unitBodyDepth * 0.5f - 0.06f },
                               0.0f, ventRects (halfH));
    }

    MeshData unitVentWalls (float halfH)
    {
        MeshData mesh;
        for (auto& v : ventRects (halfH))
            mesh.append (wellWalls (v, 0.0f, lidVentDepth));
        return mesh;
    }

    MeshData unitVentFloors (float halfH)
    {
        MeshData mesh;
        for (auto& v : ventRects (halfH))
            mesh.append (horizontalQuad (v, -lidVentDepth));
        return mesh;
    }

    MeshData unitBodyScrews (float halfH)
    {
        MeshData mesh;
        const auto head = sweptRoundedRect (0, 0, 0.030f, 3, { { 0.0f, -0.002f }, { 0.0f, 0.004f }, { -0.011f, 0.011f } }, true);
        // On the body's top face (z = 0 here; drawn offset by the unit's half height), at depths into
        // the case. (They used to be placed with those depths along z - down the faceplate - so every
        // unit showed two stray screw heads on its front.)
        for (float x : { -(chassisHalfW - 0.12f), chassisHalfW - 0.12f })
            for (float depth : { -unitBodyDepth * 0.30f, -unitBodyDepth * 0.78f })
                mesh.append (head, Mat4::translation ({ x, depth, 0.0f }) * Mat4::rotationX (-0.5f * pi));
        (void) halfH;
        return mesh;
    }

    //==============================================================================
    // The case: two cheeks swept along the arc the units sit on, rails behind the gaps,
    // and the surface it all stands on.
    namespace
    {
        /** Point on the arc at arc-length s from the bottom of the stack, offset `out` from
            the panel plane (+out = toward the viewer) and `back` into the case. */
        gfx::Vec3 arcPoint (float s, float out)
        {
            const float a = (s - 0.5f * totalArcLength()) / arcRadius;
            const float r = arcRadius - out;
            return { 0.0f, arcCentreY + r * std::sin (a), arcCentreZ - r * std::cos (a) };
        }
    }

    namespace
    {
        /** The case's frame at a point on the arc: x across, T along the arc (up), O out toward the viewer. */
        Mat4 caseFrame (float s, float out)
        {
            const float a = (s - 0.5f * totalArcLength()) / arcRadius;
            const auto o = arcPoint (s, out);
            Mat4 m = Mat4::identity();
            m.at (0, 0) = 1.0f; m.at (1, 0) = 0.0f;         m.at (2, 0) = 0.0f;           // x
            m.at (0, 1) = 0.0f; m.at (1, 1) = std::cos (a); m.at (2, 1) = std::sin (a);   // y -> T
            m.at (0, 2) = 0.0f; m.at (1, 2) = -std::sin (a); m.at (2, 2) = std::cos (a);  // z -> O
            m.at (0, 3) = o.x;  m.at (1, 3) = o.y;          m.at (2, 3) = o.z;
            return m;
        }

        /** A cheek's cross-section: points (dx from its inner face, out toward the viewer) with their
            normals, in strips - a strip's points share smooth normals, strips meet at hard edges. */
        struct SectionPoint { float dx, out, nx, nout; };
        std::vector<std::vector<SectionPoint>> cheekSection()
        {
            constexpr float W = caseCheekW, F = caseFront, D = caseDepth;
            constexpr float c = 0.012f, r = 0.035f, rb = 0.02f, g0 = 0.45f * W;
            std::vector<std::vector<SectionPoint>> strips;
            strips.push_back ({ { 0.0f, -D, -1.0f, 0.0f }, { 0.0f, F - c, -1.0f, 0.0f } });                       // inner face
            strips.push_back ({ { 0.0f, F - c, -0.7071f, 0.7071f }, { c, F, -0.7071f, 0.7071f } });               // chamfer
            // Front face with two routed V-grooves running down it
            const float g1 = 0.62f * W;
            strips.push_back ({ { c, F, 0.0f, 1.0f }, { g0 - 0.03f, F, 0.0f, 1.0f } });
            for (float g : { g0 - 0.03f, g1 })
            {
                strips.push_back ({ { g, F, -0.83f, 0.55f }, { g + 0.010f, F - 0.016f, -0.83f, 0.55f } });
                strips.push_back ({ { g + 0.010f, F - 0.016f, 0.83f, 0.55f }, { g + 0.020f, F, 0.83f, 0.55f } });
                strips.push_back ({ { g + 0.020f, F, 0.0f, 1.0f }, { g == g1 ? W - r : g1, F, 0.0f, 1.0f } });
            }
            std::vector<SectionPoint> round;                                                                      // rounded front edge
            for (int k = 0; k <= 8; ++k)
            {
                const float th = 0.5f * pi * (1.0f - (float) k / 8.0f);
                round.push_back ({ W - r + r * std::cos (th), F - r + r * std::sin (th), std::cos (th), std::sin (th) });
            }
            strips.push_back (round);
            strips.push_back ({ { W, F - r, 1.0f, 0.0f }, { W, -D + rb, 1.0f, 0.0f } });                          // outer face
            std::vector<SectionPoint> back;
            for (int k = 0; k <= 4; ++k)
            {
                const float th = -0.5f * pi * (float) k / 4.0f;
                back.push_back ({ W - rb + rb * std::cos (th), -D + rb + rb * std::sin (th), std::cos (th), std::sin (th) });
            }
            strips.push_back (back);
            strips.push_back ({ { W - rb, -D, 0.0f, -1.0f }, { 0.0f, -D, 0.0f, -1.0f } });                        // back face
            return strips;
        }
    }

    /*  The case, in walnut: two solid cheeks swept along the arc - a chamfer on the inner front edge, a
        routed groove down the front, the outer front edge rounded over - with their end grain showing
        top and bottom. The crown, plinth, feet, back board and brass corners are separate meshes. */
    MeshData caseCheeks()
    {
        MeshData mesh;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;
        const auto strips = cheekSection();

        for (float side : { -1.0f, 1.0f })
        {
            for (auto& strip : strips)
            {
                for (int i = 0; i <= caseArcSteps; ++i)
                {
                    const float s = s0 + (s1 - s0) * (float) i / (float) caseArcSteps;
                    const float a = (s - 0.5f * totalArcLength()) / arcRadius;
                    for (auto& p : strip)
                    {
                        const auto w = arcPoint (s, p.out);
                        mesh.addVertex ({ side * (caseSideX + p.dx), w.y, w.z },
                                        { side * p.nx, -std::sin (a) * p.nout, std::cos (a) * p.nout }, s, p.dx);
                    }
                }
                // Stitch the strip's copies at consecutive arc steps
                const auto n = (juce::uint32) strip.size();
                const auto base = (juce::uint32) mesh.vertices.size() - n * (juce::uint32) (caseArcSteps + 1);
                for (juce::uint32 i = 0; i < (juce::uint32) caseArcSteps; ++i)
                    for (juce::uint32 k = 0; k + 1 < n; ++k)
                    {
                        const auto a = base + i * n + k, b = a + 1, c = a + n, d = c + 1;
                        mesh.addQuad (a, b, d, c);
                    }
            }

            // End grain, top and bottom: the section filled, facing along the arc
            for (float s : { s0, s1 })
            {
                const float a = (s - 0.5f * totalArcLength()) / arcRadius;
                const float sign = s > s0 ? 1.0f : -1.0f;
                const Vec3 normal { 0.0f, sign * std::cos (a), sign * std::sin (a) };
                std::vector<Vec3> outline;
                for (auto& strip : strips)
                    for (auto& p : strip)
                    {
                        const auto w = arcPoint (s, p.out);
                        outline.push_back ({ side * (caseSideX + p.dx), w.y, w.z });
                    }
                Vec3 centre {};
                for (auto& v : outline) centre = centre + v;
                centre = centre * (1.0f / (float) outline.size());
                const auto mid = mesh.addVertex (centre, normal, s, 0.0f);
                const auto first = (juce::uint32) mesh.vertices.size();
                for (auto& v : outline)
                    mesh.addVertex (v, normal, s, 0.0f);
                for (juce::uint32 k = 0; k < (juce::uint32) outline.size(); ++k)
                    mesh.addTriangle (mid, first + k, first + (k + 1) % (juce::uint32) outline.size());
            }
        }
        return mesh;
    }

    /** Crown on top and plinth underneath: walnut boards across both cheeks, edges chamfered, the crown
        standing a little proud at the front. */
    MeshData caseBoards()
    {
        MeshData mesh;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;
        const float centreOut = 0.5f * (caseFront - caseDepth);
        const float hw = caseSideX + caseCheekW + 0.03f, hd = 0.5f * (caseFront + caseDepth) + 0.03f;
        constexpr float c = 0.014f;
        // Crown: from the top of the cheeks up
        mesh.append (sweptRoundedRect (hw - 0.03f, hd - 0.03f, 0.03f, 3, { { 0.0f, 0.0f }, { 0.0f, caseBoardT - c }, { -c, caseBoardT } }, true),
                     caseFrame (s1, centreOut + 0.015f));
        // Plinth: under the cheeks, a touch wider and deeper
        mesh.append (sweptRoundedRect (hw + 0.02f - 0.03f, hd + 0.02f - 0.03f, 0.03f, 3, { { 0.0f, 0.0f }, { 0.0f, caseBoardT - c }, { -c, caseBoardT } }, true),
                     caseFrame (s0 - caseBoardT, centreOut + 0.01f));
        return mesh;
    }

    /** Four feet under the plinth: short turned pucks. */
    MeshData caseFeet()
    {
        MeshData mesh;
        const float s0 = -caseOverhang;
        const float hw = caseSideX + caseCheekW - 0.02f;
        const auto foot = lathe (0.055f, { { 0.0f, 0.0f }, { 0.0f, 0.035f }, { -0.008f, 0.045f }, { -0.02f, 0.048f } }, 48, true);
        for (float x : { -hw, hw })
            for (float out : { caseFront - 0.10f, -caseDepth + 0.10f })
                mesh.append (foot, caseFrame (s0 - caseBoardT, out) * Mat4::translation ({ x, 0.0f, 0.0f }) * Mat4::rotationX (pi));
        return mesh;
    }

    /** Brass corner protectors on the front of each cheek, top and bottom, each held by two screws. */
    MeshData caseBrass()
    {
        MeshData mesh;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;
        constexpr float len = 0.36f, th = 0.005f;
        for (float side : { -1.0f, 1.0f })
        {
            const float x0 = side * (caseSideX - 0.002f), x1 = side * (caseSideX + caseCheekW + 0.002f);
            for (float s : { s0, s1 })
            {
                const float dir = s > s0 ? -1.0f : 1.0f;   // the plate runs from the end back along the cheek
                const auto frame = caseFrame (s, caseFront);
                const float y0 = std::min (0.0f, dir * len), y1 = std::max (0.0f, dir * len);
                mesh.append (box ({ std::min (x0, x1), y0, 0.0f }, { std::max (x0, x1), y1, th }), frame);
                // and round onto the cheek's outer side
                const float xo = side * (caseSideX + caseCheekW);
                mesh.append (box ({ std::min (xo, xo + side * th), y0, -0.16f }, { std::max (xo, xo + side * th), y1, 0.0f }), frame);
                const auto screw = lathe (0.011f, { { 0.0f, 0.0f }, { -0.002f, 0.003f }, { -0.007f, 0.0045f } }, 24, true);
                for (float f : { 0.25f, 0.75f })
                    mesh.append (screw, frame * Mat4::translation ({ side * (caseSideX + caseCheekW * 0.5f), dir * len * f, th })
                                          * Mat4::rotationX (0.5f * pi));
            }
        }
        return mesh;
    }

    MeshData caseRails()
    {
        // The back board: seen only through the gaps between units, dark-stained
        MeshData mesh;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;
        for (int i = 0; i < caseArcSteps; ++i)
        {
            const float sA = s0 + (s1 - s0) * (float) i / (float) caseArcSteps;
            const float sB = s0 + (s1 - s0) * (float) (i + 1) / (float) caseArcSteps;
            const auto a = arcPoint (sA, -unitBodyDepth - unitRecess - 0.05f), b = arcPoint (sB, -unitBodyDepth - unitRecess - 0.05f);
            mesh.append (quad ({ -caseSideX, a.y, a.z }, { -caseSideX, b.y, b.z },
                               { caseSideX, b.y, b.z }, { caseSideX, a.y, a.z }));
        }
        return mesh;
    }

    namespace
    {
        /** Where a unit's rail segment starts and ends (panel-local z, down +): the middle of the gap
            on each side, or the case overhang at the bottom and top of the stack. */
        std::pair<float, float> railSpan (int unit)
        {
            const float h = unitHalfH (unit);
            const bool bottom = unit == bottomUnit(), top = unit == topUnit();
            return { -h - (top ? caseOverhang : 0.5f * rackGap), h + (bottom ? caseOverhang : 0.5f * rackGap) };
        }

        /** The unit's ear-screw heights (panel-local z). */
        std::vector<float> earScrewZ (int unit)
        {
            if (unit == enhUnit)  return { earSlots[0].cz, earSlots[1].cz };
            if (unit == tubeUnit) return { tubeEarSlots[0].cz, tubeEarSlots[1].cz };
            std::vector<float> zs;
            for (auto& slot : outboardEarSlots (unit))
                if (slot.cx < 0.0f)
                    zs.push_back (slot.cz);
            return zs;
        }
    }

    MeshData caseFrontRails()
    {
        MeshData mesh;
        for (int unit : rackOrder)
        {
            if (! isShown (unit))
                continue;
            const auto [z0, z1] = railSpan (unit);
            const Mat4 toWorld = panelToWorld (unit);
            for (float side : { -1.0f, 1.0f })
            {
                const float xIn = side * railInnerX, xOut = side * caseSideX;
                const float yF = -railFront, yB = -railFront - railThick;
                MeshData seg;
                seg.append (quad ({ xIn, yF, z0 }, { xIn, yF, z1 }, { xOut, yF, z1 }, { xOut, yF, z0 }));   // front
                seg.append (quad ({ xIn, yB, z0 }, { xIn, yB, z1 }, { xIn, yF, z1 }, { xIn, yF, z0 }));     // inner edge
                seg.append (quad ({ xOut, yB, z0 }, { xOut, yB, z1 }, { xIn, yB, z1 }, { xIn, yB, z0 }));   // back
                mesh.append (seg, toWorld);
            }
        }
        return mesh;
    }

    MeshData caseRailHoles()
    {
        MeshData mesh;
        for (int unit : rackOrder)
        {
            if (! isShown (unit))
                continue;
            const auto [z0, z1] = railSpan (unit);
            const auto ears = earScrewZ (unit);
            const Mat4 toWorld = panelToWorld (unit);

            // A hole under every ear screw, then the regular pitch outward from them, skipping any that
            // would crowd a hole already there
            std::vector<float> zs (ears.begin(), ears.end());
            auto add = [&] (float z)
            {
                if (z < z0 + railHoleHalf || z > z1 - railHoleHalf)
                    return false;
                for (float other : zs)
                    if (std::abs (other - z) < 0.6f * railHolePitch)
                        return true;   // crowded: skip it, keep walking
                zs.push_back (z);
                return true;
            };
            for (float e : ears)
            {
                for (float z = e - railHolePitch; add (z); z -= railHolePitch) {}
                for (float z = e + railHolePitch; add (z); z += railHolePitch) {}
            }

            for (float side : { -1.0f, 1.0f })
                for (float z : zs)
                {
                    const float x0 = side * (railHoleX - railHoleHalf), x1 = side * (railHoleX + railHoleHalf), y = -railFront + 0.0008f;
                    mesh.append (quad ({ x0, y, z - railHoleHalf }, { x0, y, z + railHoleHalf }, { x1, y, z + railHoleHalf }, { x1, y, z - railHoleHalf }), toWorld);
                }
        }
        return mesh;
    }

    MeshData caseEdges()
    {
        MeshData mesh;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;

        for (float side : { -1.0f, 1.0f })
        {
            const float xIn = side * (caseSideX + caseCheekW * 0.18f);
            const float xOut = side * (caseSideX + caseCheekW);

            for (int i = 0; i < caseArcSteps; ++i)
            {
                const float sA = s0 + (s1 - s0) * (float) i / (float) caseArcSteps;
                const float sB = s0 + (s1 - s0) * (float) (i + 1) / (float) caseArcSteps;
                const auto a = arcPoint (sA, 0.035f), b = arcPoint (sB, 0.035f);
                const auto a2 = arcPoint (sA, 0.02f), b2 = arcPoint (sB, 0.02f);
                mesh.append (quad ({ xIn, a.y, a.z }, { xIn, b.y, b.z }, { xOut, b2.y, b2.z }, { xOut, a2.y, a2.z }));
            }
        }

        return mesh;
    }

    float floorHeight()
    {
        return arcPoint (-caseOverhang, 0.0f).y - caseBoardT - 0.05f;   // under the plinth's feet
    }

    MeshData caseFloor()
    {
        const auto bottom = arcPoint (-caseOverhang, 0.0f);
        return horizontalQuad ({ 0.0f, bottom.z - 6.0f, 26.0f, 26.0f }, floorHeight());
    }

    MeshData backWall()
    {
        // Two metres behind the case, from the floor up past anything the camera can see
        const float z = arcCentreZ - arcRadius - 2.2f, y0 = floorHeight(), y1 = y0 + 24.0f;
        return quad ({ -30.0f, y0, z }, { 30.0f, y0, z }, { 30.0f, y1, z }, { -30.0f, y1, z });
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
        const auto head = rackScrewHead();
        for (auto& slot : earSlots)
            mesh.append (head, Mat4::translation ({ slot.cx + (slot.cx > 0 ? -0.02f : 0.02f), 0.0f, slot.cz }));
        return mesh;
    }

    MeshData screwSlots()
    {
        MeshData mesh;
        for (auto& slot : earSlots)
        {
            const float x = slot.cx + (slot.cx > 0 ? -0.02f : 0.02f);
            mesh.append (box ({ x - 0.021f, 0.0315f, slot.cz - 0.0042f }, { x + 0.021f, 0.0345f, slot.cz + 0.0042f }));
            mesh.append (box ({ x - 0.0042f, 0.0315f, slot.cz - 0.021f }, { x + 0.0042f, 0.0345f, slot.cz + 0.021f }));
        }
        return mesh;
    }

    //==============================================================================
    // Knob (local to knob centre)


    MeshData knobScaleRing()
    {
        return flatAnnulus (scaleInner, scaleOuter, 72);
    }

    //==============================================================================
    // SERAPH tube unit
    MeshData tubeFaceTop()
    {
        std::vector<Rect> holes { seraphDisplayRect };
        holes.insert (holes.end(), tubeEarSlots.begin(), tubeEarSlots.end());
        return plateWithHoles ({ 0.0f, 0.0f, faceHalfW - 0.016f, tubeHalfH - 0.016f }, 0.0f, holes);
    }

    MeshData tubeFaceEdges()
    {
        constexpr float r = 0.022f;
        return sweptRoundedRect (faceHalfW - r, tubeHalfH - r, r, 2,
                                 { { 0.0f, -faceThick }, { 0.0f, -0.016f }, { -0.016f, 0.0f } }, false);
    }

    MeshData tubeEarSlotWalls()
    {
        MeshData mesh;
        for (auto& slot : tubeEarSlots)
            mesh.append (wellWalls (slot, 0.0f, faceThick));
        return mesh;
    }

    MeshData tubeEarSlotFloors()
    {
        MeshData mesh;
        for (auto& slot : tubeEarSlots)
            mesh.append (horizontalQuad (slot, -faceThick));
        return mesh;
    }

    MeshData tubeScrewHeads()
    {
        MeshData mesh;
        const auto head = rackScrewHead();
        for (auto& slot : tubeEarSlots)
            mesh.append (head, Mat4::translation ({ slot.cx + (slot.cx > 0 ? -0.02f : 0.02f), 0.0f, slot.cz }));
        return mesh;
    }

    MeshData tubeScrewSlots()
    {
        MeshData mesh;
        for (auto& slot : tubeEarSlots)
        {
            const float x = slot.cx + (slot.cx > 0 ? -0.02f : 0.02f);
            mesh.append (box ({ x - 0.021f, 0.0315f, slot.cz - 0.0042f }, { x + 0.021f, 0.0345f, slot.cz + 0.0042f }));
        }
        return mesh;
    }

    MeshData seraphDisplayWalls()
    {
        return wellWalls (seraphDisplayRect, 0.0f, seraphDisplayDepth);
    }

    MeshData seraphDisplayGlass()
    {
        return horizontalQuad (seraphDisplayRect, -seraphDisplayDepth);
    }

    MeshData seraphDisplayBezel()
    {
        constexpr float r = 0.022f;
        const auto& v = seraphDisplayRect;
        MeshData mesh;
        mesh.append (sweptRoundedRect (v.hw - r, v.hd - r, r, 4,
                                       { { 0.032f, 0.0f }, { 0.032f, 0.008f }, { 0.020f, 0.017f }, { 0.004f, 0.017f }, { 0.0f, 0.006f }, { 0.0f, 0.0f } }, false),
                     Mat4::translation ({ v.cx, 0.0f, v.cz }));
        return mesh;
    }







    //==============================================================================
    // The outboard units (the three 1U units, LEVEL & LOUDNESS, MIX BALANCER): same build, their own
    // height, meters, windows and print
    MeshData oneUFaceTop (int unit)
    {
        std::vector<Rect> holes;
        for (int i = 0; i < numVus (unit); ++i)
            holes.push_back ({ vuX (unit, i), vuZ (unit, i), vuHalfW (unit), vuHalfH });
        for (auto& w : outboardWindows (unit))
            holes.push_back (w);
        for (auto& slot : outboardEarSlots (unit))
            holes.push_back (slot);
        return plateWithHoles ({ 0.0f, 0.0f, unitHalfW (unit) - 0.016f, unitHalfH (unit) - 0.016f }, 0.0f, holes);
    }

    MeshData oneUFaceEdges (int unit)
    {
        constexpr float r = 0.020f;
        return sweptRoundedRect (unitHalfW (unit) - r, unitHalfH (unit) - r, r, 2,
                                 { { 0.0f, -faceThick }, { 0.0f, -0.014f }, { -0.014f, 0.0f } }, false);
    }

    MeshData oneUEarWalls (int unit)
    {
        MeshData mesh;
        for (auto& slot : outboardEarSlots (unit))
            mesh.append (wellWalls (slot, 0.0f, faceThick));
        return mesh;
    }

    MeshData oneUEarFloors (int unit)
    {
        MeshData mesh;
        for (auto& slot : outboardEarSlots (unit))
            mesh.append (horizontalQuad (slot, -faceThick));
        return mesh;
    }

    MeshData oneUScrewHeads (int unit)
    {
        MeshData mesh;
        const auto head = rackScrewHead();
        for (auto& slot : outboardEarSlots (unit))
            mesh.append (head, Mat4::translation ({ slot.cx + (slot.cx > 0 ? -0.02f : 0.02f), 0.0f, slot.cz }));
        return mesh;
    }

    MeshData oneUScrewSlots (int unit)
    {
        MeshData mesh;
        for (auto& slot : outboardEarSlots (unit))
        {
            const float x = slot.cx + (slot.cx > 0 ? -0.02f : 0.02f);
            mesh.append (box ({ x - 0.021f, 0.0315f, slot.cz - 0.0042f }, { x + 0.021f, 0.0345f, slot.cz + 0.0042f }));
            mesh.append (box ({ x - 0.0042f, 0.0315f, slot.cz - 0.021f }, { x + 0.0042f, 0.0345f, slot.cz + 0.021f }));
        }
        return mesh;
    }

    // A display window: its well, the glass at the bottom of it (uv 0..1 across), and a bezel round it
    MeshData windowWalls (const Rect& r)  { return wellWalls (r, 0.0f, windowDepth); }
    MeshData windowGlass (const Rect& r)  { return horizontalQuad (r, -windowDepth); }

    MeshData windowBezel (const Rect& v)
    {
        constexpr float r = 0.022f;
        MeshData mesh;
        mesh.append (sweptRoundedRect (v.hw - r, v.hd - r, r, 4,
                                       { { 0.034f, 0.0f }, { 0.034f, 0.009f }, { 0.021f, 0.018f }, { 0.004f, 0.018f }, { 0.0f, 0.006f }, { 0.0f, 0.0f } }, false),
                     Mat4::translation ({ v.cx, 0.0f, v.cz }));
        return mesh;
    }

    //==============================================================================
    // The LUNCHBOX: six slots in a black frame; the modules' plates stand proud of it
    namespace
    {
        Rect modulePlateRect (int m)
        {
            const auto& mod = lbModules[(size_t) m];
            return { lbModuleX (m), 0.0f, 0.5f * lbSlotW * (float) mod.width - 0.010f, lbModuleHalfH - 0.010f };
        }
    }

    MeshData lunchboxModulePlate (int m)
    {
        const auto r = modulePlateRect (m);
        std::vector<Rect> holes;
        if (m == 3)
            holes.push_back ({ vuX (lunchboxUnit, 0), vuZ (lunchboxUnit, 0), vuHalfW (lunchboxUnit), vuHalfH });
        return plateWithHoles (r, 0.012f, holes);
    }

    MeshData lunchboxModuleEdges (int m)
    {
        const auto r = modulePlateRect (m);
        constexpr float rr = 0.012f;
        MeshData mesh;
        mesh.append (sweptRoundedRect (r.hw - rr, r.hd - rr, rr, 2, { { 0.0f, 0.0f }, { 0.0f, 0.008f }, { -0.006f, 0.012f } }, false),
                     Mat4::translation ({ r.cx, 0.0f, 0.0f }));
        return mesh;
    }

    MeshData lunchboxModuleScrews (int m)
    {
        const auto r = modulePlateRect (m);
        MeshData mesh;
        const auto head = lathe (0.026f, { { 0.0f, 0.0f }, { 0.0f, 0.010f }, { -0.006f, 0.016f }, { -0.026f, 0.018f } }, 16, true);
        for (float z : { r.minZ() + 0.045f, r.maxZ() - 0.045f })
            mesh.append (head, Mat4::translation ({ r.cx, 0.012f, z }));
        return mesh;
    }

    MeshData lunchboxSlotWell()
    {
        const Rect hole { slotX (lbEmptySlot), 0.0f, 0.5f * lbSlotW - 0.012f, lbModuleHalfH - 0.012f };
        MeshData mesh = wellWalls (hole, 0.0f, 0.55f);
        mesh.append (horizontalQuad (hole, -0.55f));
        return mesh;
    }

    MeshData lunchboxSlotRails()
    {
        const float x = slotX (lbEmptySlot), hd = lbModuleHalfH - 0.012f;
        MeshData mesh;
        for (float z : { -hd + 0.035f, hd - 0.035f })   // a guide top and bottom, a groove down the middle
        {
            mesh.append (box ({ x - 0.060f, -0.50f, z - 0.018f }, { x - 0.008f, -0.02f, z + 0.018f }));
            mesh.append (box ({ x + 0.008f, -0.50f, z - 0.018f }, { x + 0.060f, -0.02f, z + 0.018f }));
        }
        return mesh;
    }

    MeshData lunchboxConnector()
    {
        const float x = slotX (lbEmptySlot);
        return box ({ x - 0.050f, -0.548f, -0.30f }, { x + 0.050f, -0.46f, 0.30f });
    }

    MeshData lunchboxConnectorPins()
    {
        const float x = slotX (lbEmptySlot);
        MeshData mesh;
        for (int i = 0; i < 15; ++i)   // 15 contacts, like the real edge connector
        {
            const float z = -0.26f + 0.52f * (float) i / 14.0f;
            mesh.append (box ({ x - 0.012f, -0.462f, z - 0.010f }, { x + 0.012f, -0.455f, z + 0.010f }));
        }
        return mesh;
    }

    MeshData lunchboxStand()
    {
        /*  A small studio side table in walnut: a thick top with a rounded-over edge, an apron under it,
            four legs tapering toward the floor on their inside faces, and a slatted shelf between them. */
        const auto o = unitOrigin (lunchboxUnit);
        const float y0 = floorHeight(), top = o.y - lbHalfH;
        const float hw = lbHalfW + 0.14f, cz = o.z - 0.42f, hd = 0.58f;
        MeshData mesh;
        // the top: 7 cm thick, edge rounded over
        mesh.append (sweptRoundedRect (hw - 0.04f, hd - 0.04f, 0.04f, 6,
                                       { { 0.0f, -0.07f }, { 0.0f, -0.035f }, { -0.008f, -0.012f }, { -0.022f, -0.002f }, { -0.04f, 0.0f } }, true),
                     Mat4::translation ({ o.x, top, cz }));
        // apron: four boards under the top, set in from its edge
        const float ay = top - 0.07f, ah = 0.13f, inset = 0.06f, t = 0.035f;
        mesh.append (box ({ o.x - hw + inset, ay - ah, cz + hd - inset - t }, { o.x + hw - inset, ay, cz + hd - inset }));
        mesh.append (box ({ o.x - hw + inset, ay - ah, cz - hd + inset }, { o.x + hw - inset, ay, cz - hd + inset + t }));
        for (float sx : { -1.0f, 1.0f })
            mesh.append (box ({ o.x + sx * (hw - inset) - (sx > 0 ? t : 0.0f), ay - ah, cz - hd + inset }, { o.x + sx * (hw - inset) + (sx < 0 ? t : 0.0f), ay, cz + hd - inset }));
        // legs: 6 cm square at the top, 4 cm at the floor, a small chamfer on every edge
        for (float sx : { -1.0f, 1.0f })
            for (float sz : { -1.0f, 1.0f })
            {
                const float lx = o.x + sx * (hw - inset - 0.03f), lz = cz + sz * (hd - inset - 0.03f);
                mesh.append (sweptRoundedRect (0.022f, 0.022f, 0.008f, 2, { { 0.0f, 0.0f }, { 0.012f, ay - y0 - 0.005f }, { 0.012f, ay - y0 } }, false),
                             Mat4::translation ({ lx, y0, lz }));
            }
        // the shelf: five slats on two rails, 45 cm off the floor
        const float sy = y0 + 0.45f;
        for (float sz : { -1.0f, 1.0f })
            mesh.append (box ({ o.x - hw + inset, sy - 0.05f, cz + sz * (hd - inset - 0.05f) - 0.02f }, { o.x + hw - inset, sy, cz + sz * (hd - inset - 0.05f) + 0.02f }));
        for (int k = 0; k < 5; ++k)
        {
            const float z = cz - hd + inset + 0.10f + (2.0f * (hd - inset) - 0.20f) * (float) k / 4.0f;
            mesh.append (sweptRoundedRect (hw - inset - 0.04f - 0.01f, 0.04f - 0.01f, 0.01f, 2, { { 0.0f, 0.0f }, { 0.0f, 0.022f } }, true),
                         Mat4::translation ({ o.x, sy, z }));
        }
        return mesh;
    }

    MeshData lunchboxFrameHardware()
    {
        // Panel-local to the LUNCHBOX (x across, y out, z down): a tubular carrying handle on top, rubber
        // feet under it, and the rails the modules screw into (a threaded hole per slot, top and bottom)
        MeshData mesh;
        const float topZ = -lbHalfH, botZ = lbHalfH;
        for (float sx : { -1.0f, 1.0f })
            mesh.append (tubeAlong ({ { sx * 0.95f, -0.22f, topZ + 0.01f }, { sx * 0.95f, -0.22f, topZ - 0.10f }, { sx * 0.85f, -0.22f, topZ - 0.17f },
                                      { 0.0f, -0.22f, topZ - 0.18f } }, 0.022f, 12));
        for (float sx : { -1.0f, 1.0f })   // its mounting blocks
            mesh.append (box ({ sx * 0.95f - 0.045f, -0.27f, topZ - 0.03f }, { sx * 0.95f + 0.045f, -0.17f, topZ + 0.005f }));
        for (float sx : { -1.0f, 1.0f })
            for (float y : { -0.08f, -0.52f })
                mesh.append (lathe (0.05f, { { 0.0f, 0.0f }, { 0.0f, 0.035f }, { -0.012f, 0.045f } }, 16, true),
                             Mat4::translation ({ sx * (lbHalfW - 0.16f), y, botZ }) * Mat4::rotationX (0.5f * 3.14159265f));
        return mesh;
    }

    MeshData lunchboxRailHoles()
    {
        MeshData mesh;
        for (int k = 0; k < lbSlots; ++k)
            for (float sz : { -1.0f, 1.0f })
                mesh.append (flatAnnulus (0.0f, 0.016f, 12), Mat4::translation ({ slotX (k), 0.002f, sz * (lbModuleHalfH + 0.05f) }));
        return mesh;
    }

    //==============================================================================
    MeshData tubeAlong (const std::vector<Vec3>& pts, float r, int sides)
    {
        MeshData mesh;
        if (pts.size() < 2)
            return mesh;
        // A smooth curve through the points (Catmull-Rom), then rings round it with parallel-transported frames
        std::vector<Vec3> c;
        auto at = [&] (int i) { return pts[(size_t) std::clamp (i, 0, (int) pts.size() - 1)]; };
        for (int i = 0; i + 1 < (int) pts.size(); ++i)
        {
            const Vec3 p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
            const int steps = std::max (2, (int) std::ceil (length (p2 - p1) / 0.06f));
            for (int k = 0; k < steps; ++k)
            {
                const float t = (float) k / (float) steps, t2 = t * t, t3 = t2 * t;
                c.push_back ((p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f);
            }
        }
        c.push_back (pts.back());
        Vec3 prevT = normalise (c[1] - c[0]);
        Vec3 n = std::abs (prevT.y) < 0.9f ? normalise (cross (prevT, { 0.0f, 1.0f, 0.0f })) : normalise (cross (prevT, { 1.0f, 0.0f, 0.0f }));
        for (size_t i = 0; i < c.size(); ++i)
        {
            const Vec3 t = normalise (i + 1 < c.size() ? c[i + 1] - c[i] : c[i] - c[i - 1]);
            // carry the frame along: take out the part of n along the new tangent
            n = normalise (n - t * dot (n, t));
            const Vec3 b = cross (t, n);
            for (int s = 0; s <= sides; ++s)
            {
                const float a = 2.0f * 3.14159265f * (float) s / (float) sides;
                const Vec3 d = n * std::cos (a) + b * std::sin (a);
                mesh.addVertex (c[i] + d * r, d, (float) s / (float) sides, (float) i * 0.1f);
            }
            prevT = t;
        }
        const int ring = sides + 1;
        for (int i = 0; i + 1 < (int) c.size(); ++i)
            for (int s = 0; s < sides; ++s)
            {
                const auto a = (juce::uint32) (i * ring + s), b = (juce::uint32) (i * ring + s + 1);
                const auto d = (juce::uint32) ((i + 1) * ring + s), e = (juce::uint32) ((i + 1) * ring + s + 1);
                mesh.addQuad (a, d, e, b);
            }
        return mesh;
    }

    //==============================================================================
    // Cables: out of sight behind the case, they come out past the back of its right cheek at floor level
    // and run across the floor side by side, a flat ribbon, to the LUNCHBOX's stand and up its back
    namespace
    {
        /** The rack units that carry audio (all but the POWER strip), bottom to top, as shown. */
        std::vector<int> audioUnits()
        {
            std::vector<int> u;
            for (int unit : rackOrder)
                if (unit != powerUnit && isShown (unit))
                    u.push_back (unit);
            return u;
        }

        constexpr float cableR = 0.042f;   // thick XLR cable
        constexpr float inch = 0.263f;     // the rack's 19 inches are 5.0 units
        constexpr float stripScale = 0.72f;   // the strip's outlets: packed on its 1U front

        /** Where cable i sits in the bundle's cross-section (across, up): a loose heap, not a ribbon. */
        std::pair<float, float> bundleSlot (int i)
        {
            static constexpr std::array<std::pair<float, float>, 12> slots {{
                { 0.00f, 0.0f }, { 0.09f, 0.0f }, { -0.09f, 0.0f }, { 0.045f, 0.075f }, { -0.045f, 0.075f }, { 0.18f, 0.0f },
                { -0.18f, 0.0f }, { 0.135f, 0.075f }, { -0.135f, 0.075f }, { 0.0f, 0.15f }, { 0.27f, 0.0f }, { -0.27f, 0.0f } }};
            return slots[(size_t) i % slots.size()];
        }
        float wander (int i, float t) { return 0.035f * std::sin (2.3f * (float) i + 7.0f * t) + 0.02f * std::sin (1.1f * (float) i + 13.0f * t); }

        /** Cable i: from behind the case, out past its right cheek, across the floor to the LUNCHBOX's stand and
            up its back, where an XLR plugs it in. */
        std::vector<Vec3> floorRun (int i, int n, float floorY)
        {
            juce::ignoreUnused (n);
            const auto bottom = unitOrigin (rackOrder.front());
            const auto [dz, dy] = bundleSlot (i);
            const float y = floorY + cableR + dy;
            const float zBack = bottom.z - 0.70f + dz;
            const float xOut = caseSideX + caseCheekW;
            std::vector<Vec3> pts { { caseSideX - 0.60f, y, zBack }, { xOut + 0.10f, y, zBack + wander (i, 0.1f) } };
            if (isShown (lunchboxUnit))
            {
                const auto lb = unitOrigin (lunchboxUnit);
                const float xl = lb.x - lbHalfW + 0.30f + 0.11f * (float) i;   // each to its own input, left to right
                const float zStand = lb.z - 1.02f;
                pts.push_back ({ xOut + 0.60f, y, zBack + 0.10f + wander (i, 0.4f) });
                pts.push_back ({ 0.5f * (xOut + xl), y, 0.5f * (zBack + zStand) + wander (i, 0.7f) });
                pts.push_back ({ xl, floorY + cableR, zStand - 0.10f });
                pts.push_back ({ xl, floorY + 0.30f, zStand + 0.02f });
                pts.push_back ({ xl, lb.y - lbHalfH - 0.10f, lb.z - 0.86f });
                pts.push_back ({ xl, lb.y - lbHalfH + 0.12f, lb.z - 0.80f });   // up to the XLR on the frame's back
            }
            else
                pts.push_back ({ xOut + 2.5f, y, zBack - 0.4f });
            return pts;
        }
    }

    int numAudioCables() { return (int) audioUnits().size(); }

    MeshData audioCable (int index)
    {
        const int n = numAudioCables();
        if (index < 0 || index >= n)
            return {};
        return tubeAlong (floorRun (index, n, floorHeight()), cableR, 12);
    }

    MeshData audioCables (std::vector<int>& colours)
    {
        MeshData mesh;
        colours.clear();
        for (int i = 0; i < numAudioCables(); ++i)
        {
            mesh.append (audioCable (i));
            colours.push_back (i);
        }
        return mesh;
    }

    MeshData xlrConnectors()
    {
        MeshData m;
        if (! isShown (lunchboxUnit))
            return m;
        const auto lb = unitOrigin (lunchboxUnit);
        // A female XLR barrel on each cable's end, pointing up into the frame's back panel
        const auto barrel = lathe (0.062f, { { -0.022f, 0.0f }, { 0.0f, 0.02f }, { 0.0f, 0.20f }, { -0.006f, 0.21f }, { -0.006f, 0.25f }, { -0.016f, 0.26f } }, 20, true);
        for (int i = 0; i < numAudioCables(); ++i)
        {
            const float xl = lb.x - lbHalfW + 0.30f + 0.11f * (float) i;
            m.append (barrel, Mat4::translation ({ xl, lb.y - lbHalfH + 0.10f, lb.z - 0.80f }));
        }
        return m;
    }

    MeshData xlrLatches()
    {
        MeshData m;
        if (! isShown (lunchboxUnit))
            return m;
        const auto lb = unitOrigin (lunchboxUnit);
        const auto ring = lathe (0.064f, { { 0.0f, 0.16f }, { 0.0f, 0.18f } }, 20, false);
        for (int i = 0; i < numAudioCables(); ++i)
        {
            const float xl = lb.x - lbHalfW + 0.30f + 0.11f * (float) i;
            const Vec3 at { xl, lb.y - lbHalfH + 0.10f, lb.z - 0.80f };
            m.append (ring, Mat4::translation (at));
            m.append (box ({ -0.012f, 0.13f, 0.055f }, { 0.012f, 0.24f, 0.068f }), Mat4::translation (at));   // the latch tab
        }
        return m;
    }

    MeshData powerCables()
    {
        MeshData mesh;
        const float floorY = floorHeight();
        const auto stripPanel = panelToWorld (powerUnit);
        const auto strip = unitOrigin (powerUnit);
        const float xOut = caseSideX + caseCheekW;
        const float plugTip = (0.016f + 0.30f * inch + 0.12f + 0.08f) * stripScale;   // where the relief ends
        constexpr float r = 0.030f;
        // Out of each plug on the strip's front, drooping to the floor in front of the plinth, along it to the
        // right and round the cheek to the back (the rack's units are fed from behind)
        int k = 0;
        for (int i = 0; i < stripOutlets; ++i)
        {
            if (! stripPlugged[(size_t) i])
                continue;
            const float x = stripOutletX (i);
            // straight out of the plug toward the room, then a soft droop down in front of the plinth
            const Vec3 a = stripPanel.transformPoint ({ x, plugTip, stripOutletZ });
            Vec3 out = stripPanel.transformPoint ({ x, plugTip + 1.0f, stripOutletZ }) - a;
            out.y = 0.0f;   // the strip's face tilts up on the arc: the cord leaves level, then droops
            out = normalise (out);
            const Vec3 b = a + out * 0.08f + Vec3 { 0.0f, -0.02f, 0.0f };
            // (clear of the plinth: out past its front board before dropping)
            const Vec3 c = a + out * 0.26f + Vec3 { 0.0f, -0.14f, 0.0f };
            const Vec3 d = a + out * 0.42f + Vec3 { 0.0f, -0.55f * (a.y - floorY), 0.0f };
            const float zf = a.z + 0.62f + 0.06f * (float) k;
            const bool toLunchbox = i == 6 && isShown (lunchboxUnit);
            std::vector<Vec3> pts { a, b, c, d, { x + 0.03f, floorY + r, zf - 0.04f }, { x + 0.28f, floorY + r, zf } };
            if (toLunchbox)
            {
                const auto lb = unitOrigin (lunchboxUnit);
                pts.push_back ({ xOut + 0.30f, floorY + r, zf + 0.05f });
                pts.push_back ({ lb.x + lbHalfW - 0.10f, floorY + r, lb.z - 0.30f });
                pts.push_back ({ lb.x + lbHalfW - 0.30f, floorY + r, lb.z - 1.00f });
                pts.push_back ({ lb.x + lbHalfW - 0.30f, lb.y - lbHalfH - 0.25f, lb.z - 1.00f });
                pts.push_back ({ lb.x + lbHalfW - 0.30f, lb.y - 0.20f, lb.z - 0.82f });
            }
            else
            {
                pts.push_back ({ xOut + 0.12f + 0.06f * (float) k, floorY + r, zf - 0.08f });
                pts.push_back ({ xOut + 0.14f + 0.06f * (float) k, floorY + r, strip.z - 0.40f });
                pts.push_back ({ caseSideX - 0.40f, floorY + r + 0.03f * (float) k, strip.z - 0.80f });
            }
            mesh.append (tubeAlong (pts, r, 10));
            ++k;
        }
        // The strip's own cord: from its plug in the wall socket, down the wall, across the floor behind the case
        {
            const float wallZ = arcCentreZ - arcRadius - 2.2f;
            const float wx = -caseSideX - 1.10f, wy = floorY + 0.62f - 1.5f * inch;
            mesh.append (tubeAlong ({ { wx, wy, wallZ + 0.036f + plugTip },
                                      { wx, wy - 0.06f, wallZ + plugTip + 0.10f },
                                      { wx + 0.04f, floorY + 0.034f, wallZ + 0.45f },
                                      { -caseSideX - 0.40f, floorY + 0.034f, strip.z - 1.10f },
                                      { -caseSideX + 0.30f, floorY + 0.034f, strip.z - 0.80f } }, 0.034f, 10));
        }
        return mesh;
    }

    //==============================================================================
    // US mains: NEMA 5-15 (scale: the rack's 19 inches are 5.0 units, so an inch is 0.263)
    namespace
    {
        /** A D-shaped ground hole (a round top, flat bottom) and the two slots, as thin dark prisms. */
        MeshData dHole (float cx, float cz, float r, float y0, float y1)
        {
            MeshData m;
            constexpr int n = 14;
            const auto top = m.addVertex ({ cx, y1, cz }, { 0, 1, 0 }, 0.5f, 0.5f);
            juce::uint32 first = 0, prev = 0;
            for (int k = 0; k <= n; ++k)
            {
                // from the flat bottom's left end, round over the top to its right end
                const float a = 3.14159265f * (1.1f - 1.2f * (float) k / (float) n);
                const Vec3 p { cx + r * std::cos (a), y1, cz - r * std::sin (a) + 0.25f * r };
                const auto v = m.addVertex (p, { 0, 1, 0 }, 0, 0);
                if (k > 0)
                    m.addTriangle (top, prev, v);
                else
                    first = v;
                prev = v;
            }
            m.addTriangle (top, prev, first);
            juce::ignoreUnused (y0);
            return m;
        }
    }

    MeshData receptacleFace (float k)
    {
        // The face: a flat-sided oval standing a little proud, with a softened edge
        const float hw = 0.68f * inch * k, hd = 0.60f * inch * k, rr = 0.30f * inch * k;
        return sweptRoundedRect (hw - rr, hd - rr, rr, 6, { { 0.0f, 0.0f }, { 0.0f, 0.010f * k }, { -0.006f * k, 0.016f * k } }, true);
    }

    MeshData receptacleHoles (float k)
    {
        MeshData m;
        const float y = 0.0165f * k;
        // hot (narrow, right) and neutral (wider, left) slots, then the ground hole below them
        m.append (horizontalQuad ({ 0.25f * inch * k, -0.12f * inch * k, 0.030f * inch * k, 0.125f * inch * k }, y));
        m.append (horizontalQuad ({ -0.25f * inch * k, -0.12f * inch * k, 0.040f * inch * k, 0.155f * inch * k }, y));
        m.append (dHole (0.0f, 0.24f * inch * k, 0.10f * inch * k, 0.0f, y));
        return m;
    }

    MeshData plugBody (float k)
    {
        // A moulded right-body plug: its face against the outlet (y = 0), the body out along +y, a ribbed
        // strain relief tapering into the cable
        MeshData m;
        const float hw = 0.52f * inch * k, hd = 0.62f * inch * k, rr = 0.22f * inch * k;
        m.append (sweptRoundedRect (hw - rr, hd - rr, rr, 6, { { 0.0f, 0.0f }, { 0.0f, 0.08f * k }, { -0.012f * k, (0.30f * inch + 0.10f) * k }, { -0.05f * k, (0.30f * inch + 0.12f) * k } }, true));
        for (int r = 0; r < 4; ++r)   // the relief's ribs
        {
            const float y = (0.30f * inch + 0.12f + 0.022f * (float) r) * k, rad = (0.058f - 0.006f * (float) r) * k;
            m.append (lathe (rad, { { 0.0f, y }, { 0.0f, y + 0.014f * k }, { -0.006f * k, y + 0.020f * k } }, 16, false));
        }
        return m;
    }

    MeshData plugPins()
    {
        MeshData m;
        // two flat blades and the round ground pin, sticking out of the face (-y)
        for (float x : { -0.25f * inch, 0.25f * inch })
            m.append (box ({ x - 0.012f, -0.625f * inch, -0.12f * inch - 0.125f * inch }, { x + 0.012f, 0.0f, -0.12f * inch + 0.125f * inch }));
        m.append (lathe (0.09f * inch, { { 0.0f, -0.73f * inch }, { 0.0f, 0.0f } }, 14, false), Mat4::translation ({ 0.0f, 0.0f, 0.24f * inch }) * Mat4::rotationX (0.0f));
        m.append (flatAnnulus (0.0f, 0.09f * inch, 14), Mat4::translation ({ 0.0f, -0.73f * inch, 0.24f * inch }) * Mat4::rotationX (3.14159265f));
        return m;
    }

    MeshData stripReceptacles()
    {
        MeshData m;
        for (int i = 0; i < stripOutlets; ++i)
            m.append (receptacleFace (stripScale), Mat4::translation ({ stripOutletX (i), 0.0f, stripOutletZ }));
        return m;
    }

    MeshData stripReceptacleHoles()
    {
        MeshData m;
        for (int i = 0; i < stripOutlets; ++i)
            if (! stripPlugged[(size_t) i])
                m.append (receptacleHoles (stripScale), Mat4::translation ({ stripOutletX (i), 0.0f, stripOutletZ }));
        return m;
    }

    MeshData stripPlugs()
    {
        MeshData m;
        const auto body = plugBody (stripScale);
        for (int i = 0; i < stripOutlets; ++i)
            if (stripPlugged[(size_t) i])
                m.append (body, Mat4::translation ({ stripOutletX (i), 0.016f * stripScale, stripOutletZ }));
        return m;
    }

    MeshData stripPlugPins() { return {}; }

    namespace
    {
        Mat4 loosePlugFrame()
        {
            // on the floor in front of the plinth, left of centre, face turned toward the viewer and up a little
            const auto front = unitOrigin (powerUnit);
            return Mat4::translation ({ -1.35f, floorHeight() + 0.62f * inch, front.z + 0.75f }) * Mat4::rotationY (0.35f) * Mat4::rotationX (-1.35f);
        }
    }

    MeshData loosePlug() { MeshData m; m.append (plugBody (0.8f), loosePlugFrame()); return m; }
    MeshData loosePlugPins() { MeshData m; m.append (plugPins(), loosePlugFrame() * Mat4::scale (0.8f, 0.8f, 0.8f)); return m; }

    //==============================================================================
    // The wall socket: a duplex receptacle behind its plate, the strip's plug in the upper one
    namespace
    {
        Mat4 wallFrame()
        {
            // plate on the wall, left of the case, at outlet height (face = +z, toward the room)
            const float wallZ = arcCentreZ - arcRadius - 2.2f;
            return Mat4::translation ({ -caseSideX - 1.10f, floorHeight() + 0.62f, wallZ }) * Mat4::rotationX (0.5f * 3.14159265f);
        }
        constexpr float wallRecGap = 1.5f * inch;   // centre to centre of the two receptacles (3 in apart)
    }

    MeshData wallPlate()
    {
        MeshData m;
        // 2.75 x 4.5 in plate, bevelled; two receptacle openings are covered by the receptacles themselves
        const float hw = 1.375f * inch, hd = 2.25f * inch, rr = 0.06f;
        m.append (sweptRoundedRect (hw - rr, hd - rr, rr, 4, { { 0.0f, 0.0f }, { 0.0f, 0.006f }, { -0.010f, 0.018f }, { -0.022f, 0.022f } }, true), wallFrame());
        // the centre screw, slotted
        m.append (lathe (0.055f, { { 0.0f, 0.022f }, { -0.01f, 0.030f }, { -0.03f, 0.034f }, { -0.055f, 0.034f } }, 16, true), wallFrame());
        return m;
    }

    MeshData wallReceptacles()
    {
        MeshData m;
        for (float z : { -wallRecGap, wallRecGap })
            m.append (receptacleFace (1.0f), wallFrame() * Mat4::translation ({ 0.0f, 0.020f, z }));
        return m;
    }

    MeshData wallReceptacleHoles()
    {
        MeshData m;
        m.append (receptacleHoles (1.0f), wallFrame() * Mat4::translation ({ 0.0f, 0.020f, wallRecGap }));   // the lower one is free
        return m;
    }

    MeshData wallPlug()
    {
        MeshData m;
        m.append (plugBody (1.0f), wallFrame() * Mat4::translation ({ 0.0f, 0.036f, -wallRecGap }));
        return m;
    }

    MeshData wallOutlet() { return {}; }
}
