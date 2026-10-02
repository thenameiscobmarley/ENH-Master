#pragma once

#include "GeometryFactory.h"
#include "CablePhysics.h"
#include <array>

/*  The cables at the back of the rack (seen when it is turned round), in world space as the rack stands before
    it turns (the renderer turns them with it):
    - every unit's XLRs and its IEC mains cord, plugged into its back (BackPanels.h), each cable hanging into a
      loom down one side - audio on the right as you look at the back, mains on the left - and away under the case;
    - the TT patch cords on the bay, out of one unit's OUT jacks into the next one's IN: the rack's real chain,
      from RACK IN through every unit in order to RACK OUT. */
namespace pad::geo::patch
{
    inline constexpr int numCordColours = 5;

    struct Meshes
    {
        MeshData xlrJackets, iecJackets;                     // black rubber jackets
        std::array<MeshData, numCordColours> cordJackets;    // the TT cords, colour-coded
        MeshData nickel;                                     // XLR shells, bantam ferrules: plated metal
        MeshData rubber;                                     // boots, IEC plug bodies, bantam bodies: black moulding
        MeshData latches;                                    // the female XLRs' release tabs (dark metal)
        std::array<MeshData, 2> braided;                     // braided jackets: black, and old-style tweed
        MeshData brass;                                      // ground lugs
        MeshData gaps;                                       // the dark between the units' backs
        MeshData tape;                                       // the tape flags' bands round their cables
        struct Flag { hwk::gfx::Mat4 frame; juce::String text; bool turned = false; };   // turned: its tab goes the other way (the writing turned to suit)   // a flag's tab: x out from the cable, z along it
        std::vector<Flag> flags;
        std::vector<hwk::gfx::Vec3> colliders;               // the settled cables' particles (what the cords can't go through)
        std::vector<float> colliderR;
    };
    inline constexpr float flagLength = 0.34f, flagWidth = 0.11f;   // a tab, past the cable's edge

    /** The units' backs: every XLR and mains cord, into the looms (TT cords: buildCords). */
    Meshes buildBacks();

    /** One TT cord: each end in a jack (col, row - BackPanels.h bay::; `depth` 1 home .. 0 just out) or, col < 0,
        free: its plug at `at` (rack space), cable leaving along `dir` (held in the hand, or lying on the shelf). */
    struct CordEnd { int col = -1, row = 0; float depth = 1.0f; hwk::gfx::Vec3 at {}, dir { 0.0f, 0.0f, 1.0f }; };
    struct CordDraw { CordEnd a, b; int colour = 0; };

    /** The cords as ropes (CablePhysics.h): settled against `colliders` (the backs' cables), each one from `ropes`
        (how it hung before) where it still fits; `ropes` comes back as they hang now, one per cord. */
    void buildCords (const std::vector<CordDraw>& cords, Meshes& into, const std::vector<hwk::gfx::Vec3>& colliders,
                     const std::vector<float>& colliderR, std::vector<rope::Rope>& ropes);
    rope::Rope cordRope (const CordDraw&, int index);
    void pinCord (rope::Rope&, const CordDraw&);
    void cordMeshes (const rope::Rope&, const CordDraw&, Meshes& into);
    /** The cord in the hand (or still swinging after): one frame of it, its plugs where they are now. */
    void stepCord (rope::Rope&, const CordDraw&, float dt, const std::vector<hwk::gfx::Vec3>& colliders, const std::vector<float>& colliderR);
    rope::World world();   // the shelf and the rack's back, for the cables

    /** Where a jack is (rack space) and the way out of it; where a pulled plug lies (on the shelf below it). */
    hwk::gfx::Vec3 jackAt (int col, int row);
    hwk::gfx::Vec3 bayOut();
    hwk::gfx::Vec3 restingBelow (int col, int row, int index);
    /** A frame at `at` whose +y runs along `dir` (for parts set on the bay). */
    hwk::gfx::Mat4 frameAlong (hwk::gfx::Vec3 at, hwk::gfx::Vec3 dir);

    /*  The MASTER switch on top of the rack at the back: ANYTHING INTO ANYTHING. A small plate standing on the crown's
        back edge, facing the back; its frame is like a panel's (y out of its face, z down it, x across). */
    inline constexpr float masterHalfW = 0.62f, masterHalfH = 0.23f, masterLeverX = 0.36f;
    hwk::gfx::Mat4 masterFrame();
    MeshData masterPlate();                 // the plate's body (its print is a quad on its face: masterFace)
    MeshData masterFace();
    MeshData masterNut();
    MeshData masterLever (bool on);         // the bat lever: up for ON

    /** The TT cords' jackets' colours (gamma-free linear RGB, as the renderer's colours). */
    hwk::gfx::Vec3 cordColour (int k);
}
