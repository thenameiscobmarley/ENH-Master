#pragma once

#include "../Render/GLResources.h"
#include "DeviceLayout.h"

/*  Procedural low-poly geometry, built once at context creation.
    Front-panel parts are in panel-local space (see DeviceLayout.h).
*/
namespace pad::geo
{
    using gfx::MeshData;

    /** (outset from the base outline, y) */
    struct ProfilePoint { float outset, y; };

    /** Sweeps a profile around a rounded rectangle (halfW/halfD = 0 gives a circle
        of `radius`). Each profile segment gets its own normal → crisp bevels. */
    MeshData sweptRoundedRect (float halfW, float halfD, float radius, int cornerSegments,
                               const std::vector<ProfilePoint>& profile, bool capTop);

    /** Flat rectangle at height y with axis-aligned rectangular holes. */
    MeshData plateWithHoles (const layout::Rect& outer, float y, const std::vector<layout::Rect>& holes);

    /** Inner walls of a rectangular cutout (normals pointing inward). */
    MeshData wellWalls (const layout::Rect& hole, float topY, float depth);

    /** Flat quad with uv 0..1 (u along +x, v along +z). */
    MeshData horizontalQuad (const layout::Rect& r, float y);

    MeshData box (gfx::Vec3 minCorner, gfx::Vec3 maxCorner);
    MeshData flatAnnulus (float innerRadius, float outerRadius, int segments);
    MeshData dome (float radius, float height, int segments, int rings);
    MeshData triangularBlade (float baseHalfWidth, float height, float halfThickness);

    // --- world space -------------------------------------------------------------
    MeshData chassisBody();
    MeshData lidTop();            // lid-local (surface at y = 0), with vent slots
    MeshData lidVentWalls();
    MeshData lidVentFloors();
    MeshData feet();
    MeshData tablePlane();
    MeshData unitQuad();          // [-1, 1] quad, for shadows / decals

    // --- panel-local ---------------------------------------------------------------
    MeshData faceplateEdges();
    MeshData faceplateTop();      // with cutouts for display and ear slots
    MeshData displayWalls();
    MeshData displayGlass();
    MeshData displayBezel();
    MeshData earSlotWalls();
    MeshData earSlotFloors();
    MeshData screwHeads();

    // --- knob (local to knob) --------------------------------------------------------
    MeshData knobBezel();         // static, mounted to the panel
    MeshData knobSkirt();         // rotating, carries the numbered scale
    MeshData knobCap();           // rotating
    MeshData knobCapInsert();     // rotating

    // --- toggle switch (local to switch) ---------------------------------------------
    MeshData switchPlate();       // rectangular chrome plate
    MeshData switchBushing();     // round washer + bushing from the pivot outward
    MeshData switchLever();       // rotating rectangular pole
}
