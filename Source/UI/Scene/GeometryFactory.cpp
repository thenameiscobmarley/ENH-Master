#include "GeometryFactory.h"

namespace pad::geo
{
    using gfx::Vec3;
    using gfx::Mat4;
    using namespace layout;

    using namespace hwk::geo;
    using layout::pi;

    //==============================================================================
    // World-space parts
    MeshData chassisBody()
    {
        constexpr float r = 0.05f;
        const float halfD = 0.5f * chassisDepth;
        MeshData mesh;
        // A shallow seam groove where the lid wraps over the body
        const float seam = chassisTop - 0.16f;
        mesh.append (sweptRoundedRect (chassisHalfW - r, halfD - r, r, 3,
                                       { { 0.0f, chassisBottom }, { 0.0f, seam - 0.012f }, { -0.010f, seam - 0.004f },
                                         { -0.010f, seam + 0.004f }, { 0.0f, seam + 0.012f },
                                         { 0.0f, chassisTop - 0.03f }, { -0.03f, chassisTop } }, false),
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

    MeshData lidScrews()
    {
        MeshData mesh;
        const auto head = sweptRoundedRect (0, 0, 0.032f, 3, { { 0.0f, -0.002f }, { 0.0f, 0.004f }, { -0.012f, 0.012f } }, true);
        for (auto& s : layout::lidScrews)
            mesh.append (head, Mat4::translation ({ s[0], 0.0f, s[1] }));
        return mesh;
    }

    MeshData tablePlane()
    {
        return horizontalQuad ({ 0.0f, 0.0f, 24.0f, 24.0f }, 0.0f);
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
    MeshData tubeChassisBody()
    {
        constexpr float r = 0.05f;
        const float halfD = 0.5f * chassisDepth;
        MeshData mesh;
        mesh.append (sweptRoundedRect (chassisHalfW - r, halfD - r, r, 3,
                                       { { 0.0f, tubeChassisBottom }, { 0.0f, tubeChassisTop - 0.03f }, { -0.03f, tubeChassisTop } }, false),
                     Mat4::translation ({ 0.0f, 0.0f, chassisFrontZ - halfD }));
        return mesh;
    }

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



    MeshData oneUChassis (float centreY, float halfH)
    {
        constexpr float r = 0.04f;
        const float halfD = 0.5f * chassisDepth;
        MeshData mesh;
        mesh.append (sweptRoundedRect (chassisHalfW - r, halfD - r, r, 3,
                                       { { 0.0f, centreY - halfH + 0.01f }, { 0.0f, centreY + halfH - 0.05f }, { -0.03f, centreY + halfH - 0.01f } }, false),
                     Mat4::translation ({ 0.0f, 0.0f, chassisFrontZ - halfD }));
        return mesh;
    }

    //==============================================================================
    // Rack case: two vertical rails with rack holes, a back wall, a floor and a top
    static std::vector<Rect> rackHoles (float railCx)
    {
        std::vector<Rect> holes;
        for (float y = rackFloorY + 0.09f; y < rackTopY - 0.06f; y += rackHoleStep)
            holes.push_back ({ railCx, y, 0.026f, 0.020f });
        return holes;
    }

    MeshData rackRails()
    {
        MeshData mesh;
        // Built in the xy plane at the front of the case, so "z" of the Rect is world y
        for (float side : { -1.0f, 1.0f })
        {
            const float cx = side * rackRailX;
            auto plate = plateWithHoles ({ cx, 0.5f * (rackFloorY + rackTopY), rackRailHalfW, 0.5f * (rackTopY - rackFloorY) },
                                         0.0f, rackHoles (cx));
            mesh.append (plate, Mat4::translation ({ 0.0f, 0.0f, frontZ }) * Mat4::rotationX (0.5f * pi));
        }
        return mesh;
    }

    MeshData rackHoleWalls()
    {
        MeshData mesh;
        for (float side : { -1.0f, 1.0f })
            for (auto& h : rackHoles (side * rackRailX))
                mesh.append (wellWalls (h, 0.0f, 0.03f), Mat4::translation ({ 0.0f, 0.0f, frontZ }) * Mat4::rotationX (0.5f * pi));
        return mesh;
    }

    MeshData rackHoleFloors()
    {
        MeshData mesh;
        for (float side : { -1.0f, 1.0f })
            for (auto& h : rackHoles (side * rackRailX))
                mesh.append (horizontalQuad (h, -0.03f), Mat4::translation ({ 0.0f, 0.0f, frontZ }) * Mat4::rotationX (0.5f * pi));
        return mesh;
    }

    MeshData rackShell()
    {
        MeshData mesh;
        const float outer = rackRailX + rackRailHalfW;
        const float depth = frontZ - rackBackZ;

        // Back wall (seen through the gaps between units), floor, top and two side walls
        mesh.append (box ({ -outer, rackFloorY, rackBackZ }, { outer, rackTopY, rackBackZ + 0.05f }));
        mesh.append (box ({ -outer, rackFloorY - 0.09f, rackBackZ }, { outer, rackFloorY, frontZ + 0.02f }));
        mesh.append (box ({ -outer, rackTopY, rackBackZ }, { outer, rackTopY + 0.09f, frontZ + 0.02f }));
        mesh.append (box ({ -outer - 0.06f, rackFloorY - 0.09f, rackBackZ }, { -outer, rackTopY + 0.09f, frontZ + 0.02f }));
        mesh.append (box ({ outer, rackFloorY - 0.09f, rackBackZ }, { outer + 0.06f, rackTopY + 0.09f, frontZ + 0.02f }));
        (void) depth;
        return mesh;
    }

    MeshData rackEdges()
    {
        MeshData mesh;
        const float outer = rackRailX + rackRailHalfW + 0.06f;
        // A bright chamfer along the front edges of the case, where the window light catches it
        for (float side : { -1.0f, 1.0f })
            mesh.append (box ({ side * outer - 0.012f, rackFloorY - 0.09f, frontZ + 0.02f },
                              { side * outer + 0.012f, rackTopY + 0.09f, frontZ + 0.035f }));
        mesh.append (box ({ -outer, rackTopY + 0.075f, frontZ + 0.02f }, { outer, rackTopY + 0.09f, frontZ + 0.035f }));
        mesh.append (box ({ -outer, rackFloorY - 0.09f, frontZ + 0.02f }, { outer, rackFloorY - 0.075f, frontZ + 0.035f }));
        return mesh;
    }
}
