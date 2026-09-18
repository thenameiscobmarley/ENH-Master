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
    MeshData caseCheeks();      // the two side walls, swept along the arc
    MeshData caseRails();       // the rails behind the gaps between units
    MeshData caseFrontRails();  // the front mounting rails the units' ears are screwed to
    MeshData caseRailHoles();   // the square rack holes punched down those rails
    MeshData caseEdges();       // bright chamfer along the front edges of the cheeks
    MeshData caseFloor();       // the surface the case is standing on

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

    // --- the two 1U units (panel-local; both are the same shape) ------------------------
    MeshData oneUFaceTop (int unit);   // holes cut for that unit's VU meters
    MeshData oneUFaceEdges();
    MeshData oneUEarWalls();
    MeshData oneUEarFloors();
    MeshData oneUScrewHeads();
    MeshData oneUScrewSlots();

    // (knobs, buttons, toggles and lamps come from hwk::models)
}
