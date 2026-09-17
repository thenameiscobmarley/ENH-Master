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

    // --- Front panel features (panel-local) ------------------------------------
    inline constexpr Rect  displayRect  { -1.36f, -0.02f, 0.54f, 0.36f };
    inline constexpr float displayDepth = 0.05f;

    inline constexpr Rect  ventBlock     { 1.86f, 0.0f, 0.11f, 0.44f };
    inline constexpr int   numVentSlots  = 8;
    inline constexpr float ventSlotHalfD = 0.022f;
    inline constexpr float ventDepth     = 0.06f;

    inline constexpr Rect ventSlot (int i) noexcept
    {
        const float pitch = (2.0f * ventBlock.hd - 2.0f * ventSlotHalfD) / (float) (numVentSlots - 1);
        return { ventBlock.cx, ventBlock.minZ() + ventSlotHalfD + (float) i * pitch, ventBlock.hw, ventSlotHalfD };
    }

    inline constexpr float powerLampX = 1.86f, powerLampZ = -0.58f;
    inline constexpr float handleX = 2.13f, handleHalfSpan = 0.44f, handleReach = 0.17f;

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
        float x, z;
        const char* paramId;
        const char* label;
        const char* subLabel;
    };

    namespace pid = pad::params::id;

    inline constexpr std::array<ControlDef, 3> controls {{
        { ControlKind::knob,   -0.30f, -0.02f, pid::clarity,      "CLARITY",     "DEPTH  /  DETAIL" },
        { ControlKind::knob,    0.66f, -0.02f, pid::adaptSpeed,   "ADAPT SPEED", "SLOW  /  FAST" },
        { ControlKind::toggle,  1.46f,  0.00f, pid::modeFootstep, "FOOTSTEP",    "PRIORITY" },
    }};

    inline constexpr int numControls = (int) controls.size();

    inline constexpr float glyphX = 1.46f, glyphZ = -0.50f;

    /** Source legend LEDs under the display (panel-local). */
    inline constexpr float legendZ = 0.50f;
    inline constexpr float legendX[3] { -1.84f, -1.44f, -1.02f };
}
