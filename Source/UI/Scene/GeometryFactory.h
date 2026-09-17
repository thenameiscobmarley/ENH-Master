#pragma once

#include "../Render/GLResources.h"
#include "DeviceLayout.h"

/*  Procedural low-poly geometry. Everything is built once at context creation.
    Total scene is a few thousand triangles.
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

    /** Flat horizontal rectangle at height y with axis-aligned rectangular holes. */
    MeshData plateWithHoles (const layout::Rect& outer, float y, const std::vector<layout::Rect>& holes);

    /** Inner walls of a rectangular cutout (normals pointing inward). */
    MeshData wellWalls (const layout::Rect& hole, float topY, float depth);

    /** Horizontal quad with uv 0..1 (u along +x, v along +z). */
    MeshData horizontalQuad (const layout::Rect& r, float y);

    MeshData box (gfx::Vec3 minCorner, gfx::Vec3 maxCorner);
    /** Flat ring in the XZ plane at y = 0, uv = (x, z). */
    MeshData flatAnnulus (float innerRadius, float outerRadius, int segments);

    MeshData dome (float radius, float height, int segments, int rings);

    /** Triangular blade in the YZ plane extruded along X; pivot at origin. */
    MeshData triangularBlade (float baseHalfWidth, float height, float halfThickness);

    // --- composite parts -------------------------------------------------------
    MeshData chassisBody();
    MeshData rackEars();
    MeshData feet();
    MeshData panelEdges();
    MeshData panelTop();                  // with cutouts for scope, vents, switches
    MeshData screws();

    MeshData knobFlange();                // static skirt + dial face
    MeshData knobCapBody();               // rotating
    MeshData knobCapInsert();             // rotating (metal top)
    MeshData knobPointer();               // rotating

    MeshData switchWellWalls();           // both switches
    MeshData switchWellFloors();
    MeshData switchBlade();               // single, at origin
    MeshData switchHub();                 // single, at origin

    MeshData scopeWalls();
    MeshData scopeGlass();

    MeshData ventWalls();
    MeshData ventFloors();

    MeshData tablePlane();
}
