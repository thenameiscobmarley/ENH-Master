#pragma once

#include "../Render/GLMath.h"
#include "../../Parameters/ParameterSpecs.h"

/*  World-space layout of the hardware unit. Shared by geometry building,
    panel artwork, rendering and mouse picking so they can never disagree.

    Units are arbitrary ("decimetres"). +Y up, +Z toward the viewer.
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

    // Chassis
    inline constexpr float bodyHalfW  = 2.50f;
    inline constexpr float bodyHalfD  = 1.35f;
    inline constexpr float bodyBottom = 0.05f;
    inline constexpr float bodyTop    = 0.62f;
    inline constexpr float earOuterX  = 2.80f;
    inline constexpr float earThick   = 0.05f;

    // Pearlescent top panel
    inline constexpr float panelHalfW = 2.36f;
    inline constexpr float panelHalfD = 1.21f;
    inline constexpr float panelTop   = 0.66f;

    // Oscilloscope window
    inline constexpr Rect  scopeRect  { -1.62f, -0.27f, 0.58f, 0.38f };
    inline constexpr float scopeDepth = 0.035f;

    // Vents
    inline constexpr Rect  ventBlock      { 0.50f, -0.93f, 1.25f, 0.13f };
    inline constexpr int   numVentSlots   = 22;
    inline constexpr float ventSlotHalfW  = 0.026f;
    inline constexpr float ventDepth      = 0.10f;

    inline constexpr Rect ventSlot (int i) noexcept
    {
        const float pitch = (2.0f * ventBlock.hw - 2.0f * ventSlotHalfW) / (float) (numVentSlots - 1);
        return { ventBlock.minX() + ventSlotHalfW + (float) i * pitch, ventBlock.cz, ventSlotHalfW, ventBlock.hd };
    }

    inline constexpr float glyphX = 2.08f, glyphZ = -0.93f;

    // Knob geometry (unscaled, local)
    inline constexpr float flangeRadius = 0.30f;
    inline constexpr float flangeTop    = 0.05f;
    inline constexpr float capRadius    = 0.175f;
    inline constexpr float capTop       = 0.19f;
    inline constexpr float knobSweep    = 270.0f * 3.14159265f / 180.0f;

    /** Knob angle about +Y. 0 = pointing away from viewer, positive = clockwise from above. */
    inline float knobAngleForValue (float normalised) noexcept { return -0.5f * knobSweep + knobSweep * normalised; }

    // Switch well
    inline constexpr float switchWellHalfW = 0.10f;
    inline constexpr float switchWellHalfD = 0.19f;
    inline constexpr float switchWellDepth = 0.075f;
    inline constexpr float switchPivotY    = panelTop - 0.035f;
    inline constexpr float switchAngle     = 38.0f * 3.14159265f / 180.0f;

    enum class ControlKind { knob, auxKnob, toggle };
    enum class Binding     { fixed, pdKp, pdKd };

    struct ControlDef
    {
        ControlKind kind;
        float x, z, scale;
        const char* paramId;     // for fixed bindings
        Binding binding;
        int pdTarget;            // main knobs: index into params::pdTargetIds(), else -1
        const char* label;
    };

    inline constexpr float knobColX[4] { -0.52f, 0.16f, 0.84f, 1.52f };
    inline constexpr float knobRowZ[2] { -0.30f, 0.55f };
    inline constexpr float auxRowZ     = 0.60f;
    inline constexpr float auxScale    = 0.62f;
    inline constexpr float switchX     = 2.08f;

    namespace pid = pad::params::id;

    inline constexpr std::array<ControlDef, 13> controls {{
        { ControlKind::knob,    knobColX[0], knobRowZ[0], 1.0f, pid::adaptDepth,     Binding::fixed, 0, "DEPTH" },
        { ControlKind::knob,    knobColX[1], knobRowZ[0], 1.0f, pid::response,       Binding::fixed, 1, "RESPONSE" },
        { ControlKind::knob,    knobColX[2], knobRowZ[0], 1.0f, pid::bandLeveling,   Binding::fixed, 2, "LEVELING" },
        { ControlKind::knob,    knobColX[3], knobRowZ[0], 1.0f, pid::maskDucking,    Binding::fixed, 3, "MASK DUCK" },
        { ControlKind::knob,    knobColX[0], knobRowZ[1], 1.0f, pid::exciterDrive,   Binding::fixed, 4, "EXCITE" },
        { ControlKind::knob,    knobColX[1], knobRowZ[1], 1.0f, pid::transientFocus, Binding::fixed, 5, "TRANSIENT" },
        { ControlKind::knob,    knobColX[2], knobRowZ[1], 1.0f, pid::stepFocus,      Binding::fixed, 6, "STEP FOCUS" },
        { ControlKind::knob,    knobColX[3], knobRowZ[1], 1.0f, pid::outputGain,     Binding::fixed, 7, "OUTPUT" },

        { ControlKind::auxKnob, -2.00f, auxRowZ, auxScale, nullptr,                  Binding::pdKp,  -1, "Kp" },
        { ControlKind::auxKnob, -1.62f, auxRowZ, auxScale, nullptr,                  Binding::pdKd,  -1, "Kd" },
        { ControlKind::auxKnob, -1.24f, auxRowZ, auxScale, pid::reactionComp,        Binding::fixed, -1, "REACT COMP" },

        { ControlKind::toggle,  switchX, knobRowZ[0], 1.0f, pid::modeMasking,        Binding::fixed, -1, "MASKING" },
        { ControlKind::toggle,  switchX, knobRowZ[1], 1.0f, pid::modeFootstep,       Binding::fixed, -1, "FOOTSTEP" },
    }};

    inline constexpr int numControls = (int) controls.size();

    inline constexpr Rect switchWell (const ControlDef& c) noexcept { return { c.x, c.z, switchWellHalfW, switchWellHalfD }; }

    /** Legend LEDs (user / automation / self-tune). */
    inline constexpr float legendZ = 1.06f;
    inline constexpr float legendX[3] { -2.18f, -1.66f, -1.10f };
}
