#pragma once

#include "../HardwareKit.h"
#include "DeviceLayout.h"
#include <vector>

/*  Procedural low-poly geometry, built once at context creation.
    Front-panel parts are in panel-local space (see DeviceLayout.h).
*/
namespace pad::geo
{
    using hwk::gfx::MeshData;
    using hwk::geo::ProfilePoint;
    using hwk::geo::unitQuad;

    // --- unit body (panel-local: the panel is at y = 0, the body runs back toward -y) ------
    MeshData unitBody (float halfH, float halfW = layout::chassisHalfW);   // rounded box behind the faceplate
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

    // --- the LUNCHBOX (panel-local to its frame), its stand, the POWER strip's lamps, the cables ------
    MeshData lunchboxModulePlate (int module);   // a module's front plate (with its meter's hole)
    MeshData lunchboxModuleEdges (int module);
    MeshData lunchboxModuleScrews (int module);  // its two captive screws, top and bottom
    MeshData lunchboxSlotWell();                 // the empty slot: its walls and back, in the dark
    MeshData lunchboxSlotRails();                // the card guides a module slides along
    MeshData lunchboxConnector();                // the card-edge connector at the back
    MeshData lunchboxConnectorPins();            // its gold contacts
    MeshData lunchboxStand();                    // world space: the walnut stand it sits on
    MeshData lunchboxFrameHardware();            // panel-local: handle, feet
    MeshData lunchboxRailHoles();                // the threaded holes the modules' screws go into
    /** The cables (world space): an audio cable and a power cable out of the side of every unit, down
        beside the case in a loom, across the floor; the LUNCHBOX's to its stand; the POWER strip's to
        the wall. `colours` gets one entry per audio cable (its colour index), same order as the mesh. */
    MeshData audioCables (std::vector<int>& colours);
    MeshData audioCable (int index);             // one of them (drawn one by one, each in its colour)
    int numAudioCables();
    MeshData powerCables();
    MeshData xlrConnectors();                    // the XLR barrels where the audio cables plug into the LUNCHBOX
    MeshData xlrLatches();                       // their metal latches and rings
    // US mains parts (NEMA 5-15). Receptacle: its face (y up out of the panel) and its dark slots and ground
    // hole; plug: a moulded body with strain relief along +y, its blades and ground pin pointing -y
    MeshData receptacleFace (float scale = 1.0f);
    MeshData receptacleHoles (float scale = 1.0f);
    MeshData plugBody (float scale = 1.0f);
    MeshData plugPins();
    MeshData stripReceptacles();                 // panel-local to the POWER strip: all eight faces
    MeshData stripReceptacleHoles();             // the empty ones' slots (the plugged ones are hidden)
    MeshData stripPlugs();                       // the plugs in it
    MeshData stripPlugPins();                    // (not drawn: inside the outlets) - kept for the loose plug
    MeshData loosePlug();                        // world: a spare plug on the floor, pins toward you
    MeshData loosePlugPins();
    MeshData wallPlate();                        // world: the wall socket's plate and its screws
    MeshData wallReceptacles();
    MeshData wallReceptacleHoles();
    MeshData wallPlug();                         // the strip's own plug in the upper socket
    MeshData wallOutlet();                       // where the POWER strip is plugged in
    /** A tube of radius r along a smooth curve through the points (a cable), `sides` round. */
    MeshData tubeAlong (const std::vector<hwk::gfx::Vec3>& points, float r, int sides = 8);

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
