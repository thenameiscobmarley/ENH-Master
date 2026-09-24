#pragma once

#include "../HardwareKit.h"
#include "DeviceLayout.h"

/*  Procedural low-poly geometry, built once at context creation.
    Front-panel parts are in panel-local space (see DeviceLayout.h).
*/
namespace pad::geo
{
    using hwk::gfx::MeshData;
    using hwk::geo::ProfilePoint;
    using hwk::geo::unitQuad;

    // --- unit body (panel-local: the panel is at y = 0, the body runs back toward -y) ------
    MeshData unitBody (float halfH);          // rounded box behind the faceplate
    MeshData unitVents (float halfH);         // slots in its top face
    MeshData unitVentWalls (float halfH);
    MeshData unitVentFloors (float halfH);
    MeshData unitBodyScrews (float halfH);    // screws along the body seam

    // --- the curved case (world space) -----------------------------------------------------
    MeshData caseCheeks();      // the two walnut side cheeks, swept along the arc, with their end grain
    MeshData caseBoards();      // walnut crown on top, plinth underneath
    MeshData caseFeet();        // four feet under the plinth
    MeshData caseBrass();       // brass corner protectors and their screws
    MeshData caseRails();       // the back board, seen through the gaps between units
    MeshData caseFrontRails();  // the front mounting rails the units' ears are screwed to
    MeshData caseRailHoles();   // the square rack holes punched down those rails
    MeshData caseEdges();       // bright chamfer along the front edges of the cheeks
    MeshData caseFloor();       // the surface the case is standing on
    MeshData backWall();        // the studio wall behind it (walnut slats, see studioWall)
    float floorHeight();        // where caseFloor lies

    // --- panel-local ---------------------------------------------------------------
    MeshData faceplateEdges();
    MeshData faceplateTop();      // with cutouts for display and ear slots
    MeshData displayWalls();
    MeshData displayGlass();
    MeshData displayBezel();
    MeshData earSlotWalls();
    MeshData earSlotFloors();
    MeshData screwHeads();
    MeshData screwSlots();        // cross recess on the rack-ear screws

    // --- knob (local to knob) ---------------------------------------------------------
    MeshData knobScaleRing();     // fixed flat annulus printed with the scale (uv = local xz)

    // --- SERAPH tube unit (panel-local unless noted) -----------------------------------------
    MeshData tubeFaceTop();       // with the live display window and ear slots cut out
    MeshData tubeFaceEdges();
    MeshData tubeEarSlotWalls();
    MeshData tubeEarSlotFloors();
    MeshData tubeScrewHeads();
    MeshData tubeScrewSlots();
    MeshData seraphDisplayWalls();
    MeshData seraphDisplayGlass();   // uv 0..1 across the window
    MeshData seraphDisplayBezel();

    // --- the outboard units (panel-local): the three 1U units, LEVEL & LOUDNESS, MIX BALANCER ---
    MeshData oneUFaceTop (int unit);     // holes cut for that unit's meters, windows and ear slots
    MeshData oneUFaceEdges (int unit);
    MeshData oneUEarWalls (int unit);
    MeshData oneUEarFloors (int unit);
    MeshData oneUScrewHeads (int unit);
    MeshData oneUScrewSlots (int unit);

    // --- a display window (panel-local): well walls, glass (uv 0..1 across), bezel ------------
    MeshData windowWalls (const layout::Rect&);
    MeshData windowGlass (const layout::Rect&);
    MeshData windowBezel (const layout::Rect&);

    // (knobs, buttons, toggles and lamps come from hwk::models)
}
