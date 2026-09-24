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
    MeshData unitBody (float halfH)
    {
        constexpr float r = 0.045f;
        const float d = unitBodyDepth;
        MeshData mesh;

        // Swept profile: a small chamfer at the front, a seam a third of the way back, and a
        // chamfer at the rear, so the body reads as folded metal rather than a plain box.
        mesh.append (sweptRoundedRect (chassisHalfW - r, halfH - r, r, 3,
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
            const bool bottom = unit == rackOrder.front(), top = unit == rackOrder.back();
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
        return plateWithHoles ({ 0.0f, 0.0f, faceHalfW - 0.016f, unitHalfH (unit) - 0.016f }, 0.0f, holes);
    }

    MeshData oneUFaceEdges (int unit)
    {
        constexpr float r = 0.020f;
        return sweptRoundedRect (faceHalfW - r, unitHalfH (unit) - r, r, 2,
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
}
