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

    // --- world space -------------------------------------------------------------
    MeshData chassisBody();
    MeshData lidTop();            // lid-local (surface at y = 0), with vent slots
    MeshData lidVentWalls();
    MeshData lidVentFloors();
    MeshData feet();
    MeshData lidScrews();         // lid-local
    MeshData tablePlane();

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
    MeshData tubeChassisBody();   // world
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
    MeshData oneUChassis (float centreY, float halfH);   // world

    // --- rack case (world space) ---------------------------------------------------------
    MeshData rackRails();          // the two vertical rails, with their rack holes cut out
    MeshData rackHoleWalls();
    MeshData rackHoleFloors();
    MeshData rackShell();          // back wall, floor and top of the case
    MeshData rackEdges();          // the bright front edges of the case, where the light catches

    // (knobs, buttons, toggles and lamps come from hwk::models)
}
