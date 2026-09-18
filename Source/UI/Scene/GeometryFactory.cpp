#include "GeometryFactory.h"

namespace pad::geo
{
    using gfx::Vec3;
    using gfx::Mat4;
    using namespace layout;

    using namespace hwk::geo;
    using layout::pi;

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

    MeshData caseCheeks()
    {
        MeshData mesh;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;

        for (float side : { -1.0f, 1.0f })
        {
            const float xIn = side * caseSideX, xOut = side * (caseSideX + caseCheekW);

            for (int i = 0; i < caseArcSteps; ++i)
            {
                const float sA = s0 + (s1 - s0) * (float) i / (float) caseArcSteps;
                const float sB = s0 + (s1 - s0) * (float) (i + 1) / (float) caseArcSteps;

                const auto fA = arcPoint (sA, 0.02f), fB = arcPoint (sB, 0.02f);          // front edge
                const auto bA = arcPoint (sA, -caseDepth), bB = arcPoint (sB, -caseDepth); // back edge

                // outer wall
                mesh.append (quad ({ xOut, fA.y, fA.z }, { xOut, fB.y, fB.z }, { xOut, bB.y, bB.z }, { xOut, bA.y, bA.z }));
                // inner wall
                mesh.append (quad ({ xIn, bA.y, bA.z }, { xIn, bB.y, bB.z }, { xIn, fB.y, fB.z }, { xIn, fA.y, fA.z }));
                // top edge of the cheek, facing the viewer
                mesh.append (quad ({ xIn, fA.y, fA.z }, { xIn, fB.y, fB.z }, { xOut, fB.y, fB.z }, { xOut, fA.y, fA.z }));
                mesh.append (quad ({ xOut, bA.y, bA.z }, { xOut, bB.y, bB.z }, { xIn, bB.y, bB.z }, { xIn, bA.y, bA.z }));
            }
        }

        return mesh;
    }

    MeshData caseRails()
    {
        MeshData mesh;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;

        // A rail running the length of the case well behind the units, so it is seen only
        // through the gaps between them - never through a meter window.
        for (int i = 0; i < caseArcSteps; ++i)
        {
            const float sA = s0 + (s1 - s0) * (float) i / (float) caseArcSteps;
            const float sB = s0 + (s1 - s0) * (float) (i + 1) / (float) caseArcSteps;
            const auto a = arcPoint (sA, -unitBodyDepth - 0.05f), b = arcPoint (sB, -unitBodyDepth - 0.05f);
            mesh.append (quad ({ -caseSideX, a.y, a.z }, { -caseSideX, b.y, b.z },
                               { caseSideX, b.y, b.z }, { caseSideX, a.y, a.z }));
        }

        return mesh;
    }

    MeshData caseFrontRails()
    {
        MeshData mesh;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;
        constexpr int steps = caseArcSteps * 3;

        for (float side : { -1.0f, 1.0f })
        {
            const float xIn = side * railInnerX, xOut = side * caseSideX;
            for (int i = 0; i < steps; ++i)
            {
                const float sA = s0 + (s1 - s0) * (float) i / (float) steps;
                const float sB = s0 + (s1 - s0) * (float) (i + 1) / (float) steps;
                const auto fA = arcPoint (sA, -railFront), fB = arcPoint (sB, -railFront);
                const auto bA = arcPoint (sA, -railFront - railThick), bB = arcPoint (sB, -railFront - railThick);

                // front face (toward the viewer), the inner edge, and the back face
                mesh.append (quad ({ xIn, fA.y, fA.z }, { xIn, fB.y, fB.z }, { xOut, fB.y, fB.z }, { xOut, fA.y, fA.z }));
                mesh.append (quad ({ xIn, bA.y, bA.z }, { xIn, bB.y, bB.z }, { xIn, fB.y, fB.z }, { xIn, fA.y, fA.z }));
                mesh.append (quad ({ xOut, bA.y, bA.z }, { xOut, bB.y, bB.z }, { xIn, bB.y, bB.z }, { xIn, bA.y, bA.z }));
            }
        }

        return mesh;
    }

    MeshData caseRailHoles()
    {
        MeshData mesh;
        const float s0 = -caseOverhang + railHolePitch * 0.5f, s1 = totalArcLength() + caseOverhang - railHolePitch * 0.5f;

        // Square holes, as dark quads a hair in front of the rail face
        for (float side : { -1.0f, 1.0f })
            for (float s = s0; s <= s1; s += railHolePitch)
            {
                const auto a = arcPoint (s - railHoleHalf, -railFront + 0.001f), b = arcPoint (s + railHoleHalf, -railFront + 0.001f);
                const float x0 = side * (railHoleX - railHoleHalf), x1 = side * (railHoleX + railHoleHalf);
                mesh.append (quad ({ x0, a.y, a.z }, { x0, b.y, b.z }, { x1, b.y, b.z }, { x1, a.y, a.z }));
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

    /** A small chevron, lying flat in panel space and pointing up the panel (-z). */
    MeshData flowArrow()
    {
        MeshData mesh;
        constexpr float w = 0.055f, h = 0.055f, t = 0.016f;

        // Two strokes meeting at the tip, so it reads as an arrowhead rather than a triangle
        for (float side : { -1.0f, 1.0f })
        {
            const Vec3 a { side * w, 0.0f, h * 0.55f };
            const Vec3 b { side * (w - t * 1.4f), 0.0f, h * 0.55f };
            const Vec3 c { 0.0f, 0.0f, -h * 0.55f + t * 1.1f };
            const Vec3 d { 0.0f, 0.0f, -h * 0.55f };
            mesh.append (quad (a, b, c, d));
            mesh.append (quad (d, c, b, a));
        }

        return mesh;
    }

    MeshData caseFloor()
    {
        const auto bottom = arcPoint (-caseOverhang, 0.0f);
        return horizontalQuad ({ 0.0f, bottom.z - 6.0f, 26.0f, 26.0f }, bottom.y - 0.10f);
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

    MeshData screwSlots()
    {
        MeshData mesh;
        for (auto& slot : earSlots)
        {
            const float x = slot.cx + (slot.cx > 0 ? -0.02f : 0.02f);
            mesh.append (box ({ x - 0.022f, 0.026f, slot.cz - 0.0045f }, { x + 0.022f, 0.0315f, slot.cz + 0.0045f }));
            mesh.append (box ({ x - 0.0045f, 0.026f, slot.cz - 0.022f }, { x + 0.0045f, 0.0315f, slot.cz + 0.022f }));
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
        const auto head = sweptRoundedRect (0, 0, 0.05f, 3, { { 0.0f, 0.0f }, { 0.0f, 0.012f }, { -0.02f, 0.03f } }, true);
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
            mesh.append (box ({ x - 0.022f, 0.026f, slot.cz - 0.0045f }, { x + 0.022f, 0.0315f, slot.cz + 0.0045f }));
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
    // The two 1U units: same shell, different print
    MeshData oneUFaceTop (int unit)
    {
        std::vector<Rect> holes;
        for (int i = 0; i < numVus (unit); ++i)
            holes.push_back ({ vuX (unit, i), vuCentreZ, vuHalfW (unit), vuHalfH });
        holes.insert (holes.end(), oneUEarSlots.begin(), oneUEarSlots.end());
        return plateWithHoles ({ 0.0f, 0.0f, faceHalfW - 0.016f, oneUHalfH - 0.016f }, 0.0f, holes);
    }

    MeshData oneUFaceEdges()
    {
        constexpr float r = 0.020f;
        return sweptRoundedRect (faceHalfW - r, oneUHalfH - r, r, 2,
                                 { { 0.0f, -faceThick }, { 0.0f, -0.014f }, { -0.014f, 0.0f } }, false);
    }

    MeshData oneUEarWalls()
    {
        MeshData mesh;
        for (auto& slot : oneUEarSlots)
            mesh.append (wellWalls (slot, 0.0f, faceThick));
        return mesh;
    }

    MeshData oneUEarFloors()
    {
        MeshData mesh;
        for (auto& slot : oneUEarSlots)
            mesh.append (horizontalQuad (slot, -faceThick));
        return mesh;
    }

    MeshData oneUScrewHeads()
    {
        MeshData mesh;
        const auto head = sweptRoundedRect (0, 0, 0.05f, 3, { { 0.0f, 0.0f }, { 0.0f, 0.012f }, { -0.02f, 0.03f } }, true);
        for (auto& slot : oneUEarSlots)
            mesh.append (head, Mat4::translation ({ slot.cx + (slot.cx > 0 ? -0.02f : 0.02f), 0.0f, slot.cz }));
        return mesh;
    }

    MeshData oneUScrewSlots()
    {
        MeshData mesh;
        for (auto& slot : oneUEarSlots)
        {
            const float x = slot.cx + (slot.cx > 0 ? -0.02f : 0.02f);
            mesh.append (box ({ x - 0.022f, 0.026f, slot.cz - 0.0045f }, { x + 0.022f, 0.0315f, slot.cz + 0.0045f }));
            mesh.append (box ({ x - 0.0045f, 0.026f, slot.cz - 0.022f }, { x + 0.0045f, 0.0315f, slot.cz + 0.022f }));
        }
        return mesh;
    }



}
