#pragma once

#include "../Render/GLMath.h"
#include "../../Parameters/ParameterSpecs.h"

/*  Layout of the rack unit. Shared by geometry, artwork, rendering and picking.

    World: +Y up, +Z toward the viewer, table at y = 0.

    Front-panel controls are described in PANEL-LOCAL space:
        x = across (right +), z = down the faceplate (down +), y = out of the faceplate.
    panelToWorld() maps that space onto the vertical faceplate.
*/
namespace pad::layout
{
    struct Rect
    {
        float cx, cz, hw, hd;
        constexpr float minX() const noexcept { return cx - hw; }
        constexpr float maxX() const noexcept { return cx + hw; }
        constexpr float minZ() const noexcept { return cz - hd; }
        constexpr float maxZ() const noexcept { return cz + hd; }
        constexpr bool contains (float x, float z) const noexcept { return x >= minX() && x <= maxX() && z >= minZ() && z <= maxZ(); }
    };

    inline constexpr float pi = 3.14159265f;

    // --- Faceplate (19" style, with rack ears) --------------------------------
    inline constexpr float faceHalfW   = 2.50f;
    inline constexpr float faceHalfH   = 0.70f;
    inline constexpr float faceThick   = 0.07f;
    inline constexpr float faceCenterY = 0.79f;
    inline constexpr float frontZ      = 1.00f;

    inline gfx::Mat4 panelToWorld() noexcept
    {
        return gfx::Mat4::translation ({ 0.0f, faceCenterY, frontZ }) * gfx::Mat4::rotationX (0.5f * pi);
    }

    // --- Chassis behind the faceplate -----------------------------------------
    inline constexpr float chassisHalfW = 2.20f;
    inline constexpr float chassisBottom = 0.07f;
    inline constexpr float chassisTop   = 1.42f;
    inline constexpr float chassisDepth = 2.50f;
    inline constexpr float chassisFrontZ = frontZ - faceThick;
    inline constexpr float chassisBackZ  = chassisFrontZ - chassisDepth;

    // --- Lid vents (world space, on top of the chassis) ---------------------------
    inline constexpr int   numLidVents   = 6;
    inline constexpr float lidVentHalfW  = 0.62f;
    inline constexpr float lidVentHalfD  = 0.028f;
    inline constexpr float lidVentDepth  = 0.05f;

    inline constexpr Rect lidVent (int i) noexcept
    {
        const float side = (i % 2 == 0) ? -1.0f : 1.0f;
        const int row = i / 2;
        return { side * 1.15f, chassisFrontZ - 0.30f - (float) row * 0.14f, lidVentHalfW, lidVentHalfD };
    }

    // --- Front panel features (panel-local) ------------------------------------
    inline constexpr Rect  displayRect  { -1.52f, -0.02f, 0.48f, 0.34f };
    inline constexpr float displayDepth = 0.05f;

    inline constexpr std::array<Rect, 4> earSlots {{
        { -2.36f, -0.47f, 0.075f, 0.036f }, { -2.36f, 0.47f, 0.075f, 0.036f },
        {  2.36f, -0.47f, 0.075f, 0.036f }, {  2.36f, 0.47f, 0.075f, 0.036f },
    }};

    // --- Knobs (unscaled, local to knob; y = out of panel) ----------------------
    inline constexpr float bezelRadius   = 0.37f;
    inline constexpr float skirtRadius   = 0.34f;
    inline constexpr float dialRadius    = 0.31f;   // flat numbered face of the skirt
    inline constexpr float dialTop       = 0.08f;
    inline constexpr float capRadius     = 0.17f;
    inline constexpr float capTop        = 0.272f;
    inline constexpr float knobSweep     = 1.5f * pi;  // 270 degrees

    /** Knob rotation about its axis (clockwise as seen by the viewer).
        The numbered skirt turns with the knob; the value under the fixed
        indicator at 12 o'clock is the current value. */
    inline float knobAngleForValue (float normalised) noexcept { return -0.5f * knobSweep + knobSweep * normalised; }

    /** Angle, in the knob's own frame, at which the scale mark for `normalised` is printed. */
    inline float scaleAngleForValue (float normalised) noexcept { return -knobAngleForValue (normalised); }

    inline constexpr float indicatorNear = 0.405f, indicatorFar = 0.50f; // distance above knob centre

    // --- Toggle switch (local to switch) ---------------------------------------
    inline constexpr float switchPlateHalfW = 0.13f;
    inline constexpr float switchPlateHalfD = 0.21f;
    inline constexpr float switchPivotY     = 0.09f;
    inline constexpr float switchAngle      = 35.0f * pi / 180.0f;

    // --- Controls ---------------------------------------------------------------
    enum class ControlKind { knob, toggle };

    struct ControlDef
    {
        ControlKind kind;
        float x, z, scale;
        const char* paramId;
        const char* label;
    };

    namespace pid = pad::params::id;

    inline constexpr float knobScale = 0.9f;
    inline constexpr float knobZ = -0.04f;
    inline constexpr float switchZ = 0.02f;

    inline constexpr std::array<ControlDef, 5> controls {{
        { ControlKind::knob,   -0.58f, knobZ,   knobScale, pid::clarity,    "CLARITY" },
        { ControlKind::knob,    0.18f, knobZ,   knobScale, pid::adaptSpeed, "ADAPT" },
        { ControlKind::knob,    0.94f, knobZ,   knobScale, pid::sub,        "SUB" },
        { ControlKind::toggle,  1.50f, switchZ, 1.0f,      pid::subBoost,   "+BOOST" },
        { ControlKind::toggle,  1.98f, switchZ, 1.0f,      pid::footstep,   "FOOTSTEP" },
    }};

    inline constexpr int numControls = (int) controls.size();

    inline constexpr float labelOffset = 0.43f;     // label centre below a control
    inline constexpr float switchLedOffset = -0.31f; // status LED above a switch
    inline constexpr float glyphOffset = -0.50f;     // footprint glyph above the footstep switch
}
