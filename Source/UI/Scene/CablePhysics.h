#pragma once

#include "../HardwareKit.h"
#include <vector>

/*  Cables as ropes (the rack's own, unturned space): a chain of particles with a fixed length between them, a little
    stiffness against bending, gravity, and what they can't go through - the shelf, the rack's back, and each other.
    A plug holds its cable's first particles in a straight line out of it (so a cable leaves its connector straight
    and bends after, never at the plug). Loom cables are held by ties along the loom.

    settle() runs a cable set until it hangs at rest (verlet, position constraints); step() moves one cable a frame
    (the one in the hand), against cables already settled. */
namespace pad::geo::rope
{
    using hwk::gfx::Vec3;

    struct Rope
    {
        std::vector<Vec3> p, prev;
        std::vector<unsigned char> pinned;   // 1: held where it is (a plug's straight run, a cable tie, the far end)
        float r = 0.03f;                     // radius
        float rest = 0.06f;                  // between neighbours
        float natural = 0.06f;               // ... as it was made (a cord pulled taut stretches a little, then gives back)
    };

    /** What the cables lie on and against. */
    struct World
    {
        float shelfY = -1.0e9f;              // the shelf's top
        float backRadius = 0.0f;             // the rack's back: nothing nearer the arc's axis than this ...
        float axisY = 0.0f, axisZ = 0.0f;    // (the axis: a line along x through (axisY, axisZ))
        float halfW = 0.0f;                  // ... across the rack's width
        float yMin = 0.0f, yMax = 0.0f;      // ... over its height
    };

    /** A rope along a path (resampled to particles about `spacing` apart), its length `slack` times the path's. The
        first `pinStart` and last `pinEnd` particles are held. */
    Rope along (const std::vector<Vec3>& path, float r, float spacing, float slack, int pinStart, int pinEnd);

    /** A rope from a to b, `length` long, started on the curve `path` (resampled), its ends pinned (`pinEnds` each). */
    Rope between (const std::vector<Vec3>& path, float r, float length, int pinEnds);

    /** Holds the particle nearest each of `points` (a cable tie). */
    void tie (Rope&, const std::vector<Vec3>& points);

    /** Runs the ropes until they settle (`steps` frames). `colliders` (x, y, z = centre, and their radii): fixed
        things to keep off - cables settled earlier. */
    void settle (std::vector<Rope>& ropes, const World&, int steps, const std::vector<Vec3>& colliders = {}, const std::vector<float>& colliderR = {});

    /** One frame for one rope (dt seconds), against fixed colliders. */
    void step (Rope&, const World&, float dt, const std::vector<Vec3>& colliders, const std::vector<float>& colliderR);
}
