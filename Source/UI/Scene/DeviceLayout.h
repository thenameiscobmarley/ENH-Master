#pragma once

#include <string_view>
#include <algorithm>
#include <cmath>
#include "../HardwareKit.h"
#include "../../Parameters/ParameterSpecs.h"

/*  Layout of the rack unit. Shared by geometry, artwork, rendering and picking.

    World: +Y up, +Z toward the viewer, table at y = 0.

    Front-panel controls are described in PANEL-LOCAL space:
        x = across (right +), z = down the faceplate (down +), y = out of the faceplate.
    panelToWorld() maps that space onto the vertical faceplate.
*/
namespace pad::layout
{
    using Rect = hwk::geo::Rect;

    inline constexpr float pi = 3.14159265f;

    // --- Faceplate (19" style, with rack ears) --------------------------------
    inline constexpr float faceHalfW   = 2.50f;
    inline constexpr float faceHalfH   = 0.92f;
    inline constexpr float faceThick   = 0.07f;
    inline constexpr float faceCenterY = 0.79f;
    inline constexpr float frontZ      = 1.00f;

    // --- Unit body, behind the faceplate (panel-local: -y runs back into the case) --------
    inline constexpr float chassisHalfW = 2.34f;
    inline constexpr float chassisDepth = 0.62f;

    // Ventilation slots in the body's top face, where warm units breathe
    inline constexpr int   lidVentCols   = 9;
    inline constexpr int   lidVentRows   = 3;
    inline constexpr int   numLidVents   = 2 * lidVentCols * lidVentRows;
    inline constexpr float lidVentHalfW  = 0.050f;
    inline constexpr float lidVentHalfD  = 0.013f;
    inline constexpr float lidVentDepth  = 0.035f;

    /** Vent slot i, in the body's top-face coordinates (x across, z back from the panel). */
    inline constexpr Rect lidVent (int i) noexcept
    {
        const int perSide = lidVentCols * lidVentRows;
        const float side = i < perSide ? -1.0f : 1.0f;
        const int k = i % perSide, col = k % lidVentCols, row = k / lidVentCols;
        const float x = side * (1.15f + ((float) col - 0.5f * (float) (lidVentCols - 1)) * 0.135f);
        return { x, -0.16f - (float) row * 0.085f, lidVentHalfW, lidVentHalfD };
    }

    // --- Front panel (panel-local) -----------------------------------------------
    // Pro-XL style: dark faceplate, outlined sections with titles, rows of small knobs over
    // fixed printed scales, square push buttons with an LED above, 12-segment LED ladders.
    /** The analyser window: wide and shallow, across the top of the panel. */
    inline constexpr Rect  displayRect  { -0.26f, -0.475f, 1.98f, 0.345f };
    inline constexpr float displayDepth = 0.05f;

    inline constexpr std::array<Rect, 4> earSlots {{
        { -2.36f, -0.66f, 0.075f, 0.036f }, { -2.36f, 0.66f, 0.075f, 0.036f },
        {  2.36f, -0.66f, 0.075f, 0.036f }, {  2.36f, 0.66f, 0.075f, 0.036f },
    }};

    struct Section { Rect box; const char* title; };

    inline constexpr float sectionTop = 0.02f, sectionBottom = 0.84f;
    inline constexpr float sectionCz = 0.5f * (sectionTop + sectionBottom), sectionHd = 0.5f * (sectionBottom - sectionTop);

    /*  Five sections with a clear gutter (>= 0.10) between each. FOOTSTEP is wide enough to hold its
        button and the DETECT ladder side by side, well inside its own border, so the ladder no longer
        sits on the FOOTSTEP / METER boundary next to the OUT ladder. */
    inline constexpr std::array<Section, 5> sections {{
        { { -1.94f,  sectionCz, 0.38f,  sectionHd }, "MASTER" },
        { { -0.72f,  sectionCz, 0.74f,  sectionHd }, "CLARITY" },
        { {  0.51f,  sectionCz, 0.39f,  sectionHd }, "SUB" },
        { {  1.33f,  sectionCz, 0.33f,  sectionHd }, "FOOTSTEP" },
        { {  2.06f,  sectionCz, 0.28f,  sectionHd }, "METER" },
    }};

    inline constexpr float sectionTitleZ = sectionTop + 0.065f;   // title printed inside the top of a box

    // --- Knobs (local to knob; y = out of panel) ------------------------------------
    inline constexpr float knobRadius   = 0.114f;   // black body
    inline constexpr float knobFlange   = 0.127f;   // small skirt at the base
    inline constexpr float knobTop      = 0.135f;
    inline constexpr float scaleInner   = 0.145f;   // printed scale ring on the panel
    inline constexpr float scaleOuter   = 0.285f;
    inline constexpr float knobSweep    = 1.5f * pi;  // 270 degrees

    /** Pointer rotation about the knob axis (clockwise as seen by the viewer); 0 = straight up. */
    inline float knobAngleForValue (float normalised) noexcept { return -0.5f * knobSweep + knobSweep * normalised; }

    // --- Push buttons (local to button) -------------------------------------------------
    inline constexpr float buttonHalfW  = 0.060f;
    inline constexpr float buttonHalfD  = 0.042f;
    inline constexpr float buttonTravel = 0.020f;
    inline constexpr float buttonLedDz  = -0.19f;   // LED above the button
    inline constexpr float ledRadius    = 0.020f;

    // --- Controls ---------------------------------------------------------------
    enum class ControlKind { knob, button, toggle, selector };

    /*  Panel names (what each unit does): enhUnit = ADAPTIVE ENHANCER, tubeUnit = TONE & SPACE,
        tideUnit = ADAPTIVE COMPRESSOR, lumenUnit = UPWARD LEVELER, limiterUnit = SPECTRAL LIMITER.
        The identifiers keep the units' earlier names, as the parameter IDs do. */
    enum Unit { enhUnit = 0, tubeUnit = 1, tideUnit = 2, lumenUnit = 3, limiterUnit = 4, numUnits = 5 };

    inline constexpr bool isOneU (int unit) noexcept { return unit == tideUnit || unit == lumenUnit || unit == limiterUnit; }

    struct ControlDef
    {
        ControlKind kind;
        float x, z;
        const char* paramId;
        const char* label;
        const char* altParamId = nullptr;    // knob bound to this parameter while modeParamId is on
        const char* modeParamId = nullptr;
        int unit = enhUnit;
        const char* group = nullptr;         // shown before the label on the display (e.g. "TONE")
        float size = 1.0f;                   // knob size relative to the unit's standard knob
        hwk::models::KnobStyle style = hwk::models::KnobStyle::proXl;
    };

    using hwk::models::KnobStyle;

    namespace pid = pad::params::id;

    // Knobs sit low enough that their top scale numbers clear the section title rule (0.134), and
    // every label - knobs, buttons, the master knobs and the LED ladders - shares one line well above
    // the bottom border (0.84).
    inline constexpr float knobZ = 0.42f;
    inline constexpr float buttonZ = 0.50f;
    inline constexpr float labelZ = 0.76f;      // control labels (all on one line, Pro-XL style)

    // ==============================================================================
    // TONE & SPACE - the purple finishing processor at the top of the case (TONE | SPACE)
    // ==============================================================================
    /*  Eurorack-style curved case. The units sit on an arc centred on the viewer, so however
        many are stacked, every panel faces the camera head on: the higher a unit sits, the
        further it leans back and the more it is rotated toward you. Nothing is foreshortened,
        which is what keeps the print readable as the case grows.

            ADAPTIVE ENHANCER (2U) -> UPWARD LEVELER (1U) -> SPECTRAL LIMITER (1U)
                -> ADAPTIVE COMPRESSOR (1U) -> TONE & SPACE (2U) -> out
    */
    inline constexpr float rackGap    = 0.105f;   // air between panels, measured along the arc
    inline constexpr float oneUHalfH  = 0.295f;

    inline constexpr float lumenHalfH = oneUHalfH;
    inline constexpr float tideHalfH  = oneUHalfH;
    inline constexpr float tubeHalfH  = 0.82f;

    inline constexpr float arcRadius  = 9.60f;    // viewer to panel
    inline constexpr float arcCentreY = 1.62f;    // the viewer's eye height
    inline constexpr float arcCentreZ = 10.05f;   // and where they are standing

    inline constexpr float unitHalfH (int unit) noexcept
    {
        return unit == tubeUnit ? tubeHalfH : isOneU (unit) ? oneUHalfH : faceHalfH;
    }

    /** Units in case order, bottom to top - which is also the order the signal runs. */
    inline constexpr std::array<int, numUnits> rackOrder { enhUnit, lumenUnit, limiterUnit, tideUnit, tubeUnit };

    /** Distance along the arc from the bottom of the stack to the centre of a unit. */
    inline constexpr float unitArcPos (int unit) noexcept
    {
        float pos = 0.0f;
        for (int u : rackOrder)
        {
            pos += unitHalfH (u);
            if (u == unit)
                return pos;
            pos += unitHalfH (u) + rackGap;
        }
        return pos;
    }

    inline constexpr float totalArcLength() noexcept
    {
        float total = 0.0f;
        for (int i = 0; i < numUnits; ++i)
            total += 2.0f * unitHalfH (rackOrder[(size_t) i]) + (i + 1 < numUnits ? rackGap : 0.0f);
        return total;
    }

    /** How far a unit is rotated toward the viewer: 0 at the middle of the case. */
    inline float unitAngle (int unit) noexcept
    {
        return (unitArcPos (unit) - 0.5f * totalArcLength()) / arcRadius;
    }

    /** Centre of a unit's faceplate, in world space. */
    inline gfx::Vec3 unitOrigin (int unit) noexcept
    {
        const float a = unitAngle (unit);
        return { 0.0f, arcCentreY + arcRadius * std::sin (a), arcCentreZ - arcRadius * std::cos (a) };
    }

    /** Panel-local (x across, z down, y out of the panel) to world, on the arc. */
    inline gfx::Mat4 panelToWorld (int unit) noexcept
    {
        return gfx::Mat4::translation (unitOrigin (unit)) * gfx::Mat4::rotationX (0.5f * pi + unitAngle (unit));
    }

    inline gfx::Mat4 panelToWorld() noexcept { return panelToWorld (enhUnit); }

    /** Outward normal of a unit's faceplate (world). */
    inline gfx::Vec3 unitNormal (int unit) noexcept
    {
        const float a = unitAngle (unit);
        return { 0.0f, -std::sin (a), std::cos (a) };
    }

    /** Kept for the few places that still want a height: the world y of a panel's centre. */
    inline float unitCenterY (int unit) noexcept { return unitOrigin (unit).y; }

    /** Where each unit sits in the signal chain, and what it is called on the panel. */
    struct UnitInfo { const char* name; const char* role; int chainPosition; };

    inline constexpr std::array<UnitInfo, numUnits> unitInfo {{
        { "ADAPTIVE ENHANCER",   "ADAPTIVE EQ - HARMONIC EXCITER - SUB - FOOTSTEP PRIORITY", 1 },
        { "TONE & SPACE",        "FINISHING PROCESSOR - LOUDNESS-MATCHED",                   5 },
        { "ADAPTIVE COMPRESSOR", "PROGRAM-DEPENDENT - AUTO THRESHOLD",                       4 },
        { "UPWARD LEVELER",      "3-BAND - LIFTS QUIET DETAIL",                              2 },
        { "SPECTRAL LIMITER",    "ANTI-PUMP DYNAMIC EQ",                                     3 },
    }};

    // --- the case the units are screwed into -------------------------------------------
    inline constexpr float caseSideX     = faceHalfW + 0.115f;   // inner face of each cheek
    inline constexpr float caseCheekW    = 0.135f;
    inline constexpr float caseDepth     = 0.78f;                // how far the case runs back
    inline constexpr float caseOverhang  = 0.30f;                // past the top and bottom unit
    inline constexpr int   caseArcSteps  = 26;                   // segments along the curve

    /*  Front mounting rails, one each side, swept along the same arc just behind the faceplates:
        the ears overlap them and the ear screws go through into them, as in a real rack. They run
        from just inside the ear screws out to the cheeks, and show in the gaps between units. */
    inline constexpr float railInnerX    = 2.28f;                // ear screws sit at +-2.34
    inline constexpr float railFront     = faceThick + 0.004f;   // behind the back of a faceplate
    inline constexpr float railThick     = 0.030f;
    inline constexpr float railHoleX     = 2.34f;                // on the ear screws' line
    inline constexpr float railHoleHalf  = 0.036f;
    inline constexpr float railHolePitch = oneUHalfH * 2.0f / 3.0f;   // three holes per rack unit (1U = 0.59)

    /** Panel depth of a unit's body behind its faceplate. */
    inline constexpr float unitBodyDepth = 0.62f;

    inline constexpr std::array<Rect, 4> tubeEarSlots {{
        { -2.36f, -0.58f, 0.075f, 0.036f }, { -2.36f, 0.58f, 0.075f, 0.036f },
        {  2.36f, -0.58f, 0.075f, 0.036f }, {  2.36f, 0.58f, 0.075f, 0.036f },
    }};

    // One unified front: live L/R display in the middle, one row of knobs along the bottom,
    // the toggles in a grid on the right, lamp + POWER on the left.
    // (Its bezel used to sit over the top scale numbers of WARMTH, BODY and OUTPUT below it.)
    inline constexpr Rect  seraphDisplayRect  { -0.06f, -0.46f, 1.46f, 0.26f };
    inline constexpr float seraphDisplayDepth = 0.045f;

    // Two rows of knobs with room to breathe: SILK along the first, HALO along the second
    inline constexpr float seraphRow1Z = 0.055f, seraphRow2Z = 0.505f;
    inline constexpr float seraphKnobStep = 0.545f, seraphFirstKnobX = -2.12f;
    inline constexpr float seraphKnobZ = seraphRow1Z;    // (kept for the artwork's scale ring)
    inline constexpr float lampX = -2.24f, lampZ = -0.655f;

    /** The live display: resonance-dip curve on the left, L/R activity columns on the right (uv 0..1). */
    inline constexpr float displayDipsU0 = 0.03f, displayDipsU1 = 0.35f;
    inline constexpr float displayColsU0 = 0.40f, displayColsU1 = 0.985f;
    inline constexpr int   displayColumns = 9;   // SMOOTH AIR WARMTH BODY TAPE LEVEL WIDTH SPACE SHIMMER

    // Bat toggles (local to toggle; lever pivots about x)
    inline constexpr float togglePivotY = 0.05f;
    inline constexpr float toggleAngle  = 28.0f * pi / 180.0f;

    /** Stepped selector (OFF / SILK / HEAVEN): pointer angle for a normalised choice value. */
    inline float selectorAngleForValue (float normalised) noexcept { return (normalised - 0.5f) * 2.0f * (55.0f * pi / 180.0f); }

    // Device masters: MULTIPLY (0-3x every knob) and STRENGTH (0-5, how hard it hits) on each unit
    inline constexpr float masterKnobSize = 0.62f;
    // Side by side on the knob row, labelled on the shared label line (stacked, the upper knob's
    // label ran into the lower knob's scale and the lower label sat on the section border)
    inline constexpr std::array<float, 2> masterKnobX2 { -2.12f, -1.76f };
    inline constexpr std::array<float, 2> seraphMasterX { -2.12f, -1.74f };
    inline constexpr float seraphMasterZ = -0.34f;   // clear of the knob row below (its labels used to sit on it)

    /** Knob body radius for a control (unit standard x size); styles add their own skirts / caps. */
    inline constexpr float tubeKnobBodyRadius = 0.105f;
    inline constexpr float tubeKnobScale = tubeKnobBodyRadius / knobRadius;

    // --- the three 1U units ----------------------------------------------------------
    // Built like outboard gear: brushed plate, engraved print, knobs in a bordered section on
    // the left, moving-coil VU meters behind glass on the right. The maker block (the unit's name)
    // sits between the left rack screw and the control section, clear of both.
    inline constexpr float oneUKnobRadius = 0.105f;
    inline constexpr float oneUKnobZ      = 0.010f;
    inline constexpr float oneUKnobX      = -1.37f;     // first (left) knob
    inline constexpr float oneUKnobStep   = 0.52f;
    inline constexpr float oneUButtonX    = -0.47f;     // IN, after two knobs
    inline constexpr float oneUButtonZ    = 0.010f;
    inline constexpr float oneULabelDz    = 0.190f;     // knob and switch labels, below the skirt / lever
    inline constexpr float oneUMakerX0    = -2.25f, oneUMakerX1 = -1.71f;

    /** Section box printed around the controls, like a hardware compressor's front panel. */
    inline constexpr Rect oneUSectionBox (int unit) noexcept
    {
        return unit == limiterUnit ? Rect { -0.73f, 0.0f, 0.92f, 0.245f } : Rect { -0.99f, 0.0f, 0.66f, 0.245f };
    }

    // VU meters: one wide meter for the compressor, three narrow ones for the leveler, two for the limiter
    inline constexpr float vuDepth      = 0.055f;
    inline constexpr float vuCentreZ    = -0.005f;
    inline constexpr float vuHalfH      = 0.190f;
    inline constexpr float tideVuHalfW  = 0.76f;
    inline constexpr float tideVuX      = 1.34f;
    inline constexpr float lumenVuHalfW = 0.335f;
    inline constexpr float lumenVuX     = 0.42f;        // first of three
    inline constexpr float lumenVuStep  = 0.735f;
    inline constexpr float limiterVuHalfW = 0.42f;
    inline constexpr float limiterVuX     = 0.80f;      // SPECTRAL, then BROADBAND
    inline constexpr float limiterVuStep  = 0.96f;

    inline constexpr int numVus (int unit) noexcept { return unit == tideUnit ? 1 : unit == limiterUnit ? 2 : 3; }

    inline constexpr float vuHalfW (int unit) noexcept
    {
        return unit == tideUnit ? tideVuHalfW : unit == limiterUnit ? limiterVuHalfW : lumenVuHalfW;
    }

    inline constexpr float vuX (int unit, int index) noexcept
    {
        return unit == tideUnit ? tideVuX
             : unit == limiterUnit ? limiterVuX + (float) index * limiterVuStep
                                   : lumenVuX + (float) index * lumenVuStep;
    }

    /** First needle of each 1U unit in the renderer's needle array (compressor 1, leveler 3, limiter 2). */
    inline constexpr int firstNeedle (int unit) noexcept { return unit == tideUnit ? 0 : unit == lumenUnit ? 1 : 4; }
    inline constexpr int numNeedles = 6;

    inline constexpr float oneUDisplayDepth = vuDepth;

    inline constexpr std::array<Rect, 2> oneUEarSlots {{
        { -2.36f, 0.0f, 0.075f, 0.036f }, { 2.36f, 0.0f, 0.075f, 0.036f },
    }};

    // PRESET PREV / NEXT, in the enhancer's maker block (right of the analyser window)
    inline constexpr std::array<float, 2> presetButtonX { 1.93f, 2.13f };
    inline constexpr float presetButtonZ = -0.235f;

    // CLARITY is one physical knob with two printed scales: NORM (0-30) and ADD + NORM (0-10).
    // Each mode keeps its own setting; the MODE button swaps which one the knob drives.
    inline constexpr std::array<ControlDef, 41> controls {{
        { ControlKind::button, -1.29f, buttonZ, pid::clarityMode, "MODE" },
        { ControlKind::knob,   -0.86f, knobZ,   pid::clarityNorm, "CLARITY", pid::clarityAdd, pid::clarityMode },
        { ControlKind::knob,   -0.27f, knobZ,   pid::adaptSpeed,  "ADAPT" },
        { ControlKind::knob,    0.42f, knobZ,   pid::sub,         "SUB" },
        { ControlKind::button,  0.79f, buttonZ, pid::subBoost,    "BOOST" },
        { ControlKind::button,  1.18f, buttonZ, pid::footstep,    "PRIORITY" },
        { ControlKind::knob,   masterKnobX2[0], knobZ, pid::enhMultiply, "MULTIPLY", nullptr, nullptr, enhUnit, "ENHANCER", masterKnobSize, KnobStyle::aluminium },
        { ControlKind::knob,   masterKnobX2[1], knobZ, pid::enhStrength, "STRENGTH", nullptr, nullptr, enhUnit, "ENHANCER", masterKnobSize, KnobStyle::aluminium },

        // Rack-wide PRESET buttons in the maker block (momentary; the name shows on the analyser)
        { ControlKind::button, presetButtonX[0], presetButtonZ, pid::presetPrev, "PREV", nullptr, nullptr, enhUnit, "PRESET" },
        { ControlKind::button, presetButtonX[1], presetButtonZ, pid::presetNext, "NEXT", nullptr, nullptr, enhUnit, "PRESET" },

        { ControlKind::knob, seraphFirstKnobX + 0.0f * seraphKnobStep, seraphRow1Z, pid::silkSmooth, "SMOOTH", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::fluted },
        { ControlKind::knob, seraphFirstKnobX + 1.0f * seraphKnobStep, seraphRow1Z, pid::silkAir, "AIR", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::fluted },
        { ControlKind::knob, seraphFirstKnobX + 2.0f * seraphKnobStep, seraphRow1Z, pid::silkWarmth, "WARMTH", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::fluted },
        { ControlKind::knob, seraphFirstKnobX + 3.0f * seraphKnobStep, seraphRow1Z, pid::silkBody, "BODY", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::fluted },
        { ControlKind::knob, seraphFirstKnobX + 4.0f * seraphKnobStep, seraphRow1Z, pid::silkOutput, "OUTPUT", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::fluted },

        { ControlKind::knob, seraphFirstKnobX + 0.0f * seraphKnobStep, seraphRow2Z, pid::haloWidth, "WIDTH", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::fluted },
        { ControlKind::knob, seraphFirstKnobX + 1.0f * seraphKnobStep, seraphRow2Z, pid::haloSpace, "REVERB", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::fluted },
        { ControlKind::knob, seraphFirstKnobX + 2.0f * seraphKnobStep, seraphRow2Z, pid::haloDecay, "DECAY", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::fluted },
        { ControlKind::knob, seraphFirstKnobX + 3.0f * seraphKnobStep, seraphRow2Z, pid::haloShimmer, "SHIMMER", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::fluted },
        { ControlKind::knob, seraphFirstKnobX + 4.0f * seraphKnobStep, seraphRow2Z, pid::haloTone, "TONE", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::fluted },

        // LOUDNESS: one knob, two printed scales (HOLD / LIFT + HOLD), a button to swap between them
        { ControlKind::knob,   0.74f, seraphRow2Z, pid::heavenHold, "LOUDNESS", pid::heavenLift, pid::heavenMode, tubeUnit, "TONE & SPACE", 1.18f, KnobStyle::fluted },
        { ControlKind::button, 1.28f, seraphRow2Z, pid::heavenMode, "LIFT", nullptr, nullptr, tubeUnit, "LOUDNESS" },

        { ControlKind::toggle, 1.78f, seraphRow1Z - 0.10f, pid::silkProtect, "PROTECT", nullptr, nullptr, tubeUnit, "TONE" },
        { ControlKind::toggle, 2.12f, seraphRow1Z - 0.10f, pid::silkTape, "TAPE", nullptr, nullptr, tubeUnit, "TONE" },
        { ControlKind::toggle, 1.44f, seraphRow1Z - 0.10f, pid::silkAuto, "AUTO", nullptr, nullptr, tubeUnit, "TONE" },
        { ControlKind::toggle, 1.78f, seraphRow2Z + 0.02f, pid::haloDuck, "DUCK", nullptr, nullptr, tubeUnit, "SPACE" },
        { ControlKind::toggle, 2.12f, seraphRow2Z + 0.02f, pid::haloBassMono, "BASS MONO", nullptr, nullptr, tubeUnit, "SPACE" },
        { ControlKind::toggle, 1.44f, seraphRow2Z + 0.02f, pid::haloMod, "MOD", nullptr, nullptr, tubeUnit, "SPACE" },

        { ControlKind::selector, 1.80f, -0.44f, pid::seraphMode, "POWER", nullptr, nullptr, tubeUnit, "TONE & SPACE", 1.15f, KnobStyle::chickenHead },
        { ControlKind::knob, seraphMasterX[0], seraphMasterZ, pid::seraphMultiply, "MULTIPLY", nullptr, nullptr, tubeUnit, "TONE & SPACE", masterKnobSize, KnobStyle::softTouch },
        { ControlKind::knob, seraphMasterX[1], seraphMasterZ, pid::seraphStrength, "STRENGTH", nullptr, nullptr, tubeUnit, "TONE & SPACE", masterKnobSize, KnobStyle::softTouch },

        { ControlKind::knob,   oneUKnobX + 0.0f * oneUKnobStep, oneUKnobZ, pid::tideMix, "MIX", nullptr, nullptr, tideUnit, "COMPRESSOR", 1.0f, KnobStyle::skirted },
        { ControlKind::knob,   oneUKnobX + 1.0f * oneUKnobStep, oneUKnobZ, pid::tideResponse, "RESPONSE", nullptr, nullptr, tideUnit, "COMPRESSOR", 1.0f, KnobStyle::skirted },
        { ControlKind::toggle, oneUButtonX, oneUButtonZ, pid::tideActive, "IN", nullptr, nullptr, tideUnit, "COMPRESSOR" },

        { ControlKind::knob,   oneUKnobX + 0.0f * oneUKnobStep, oneUKnobZ, pid::lumenTarget, "TARGET", nullptr, nullptr, lumenUnit, "LEVELER", 1.0f, KnobStyle::skirted },
        { ControlKind::knob,   oneUKnobX + 1.0f * oneUKnobStep, oneUKnobZ, pid::lumenResponse, "RESPONSE", nullptr, nullptr, lumenUnit, "LEVELER", 1.0f, KnobStyle::skirted },
        { ControlKind::toggle, oneUButtonX, oneUButtonZ, pid::lumenActive, "IN", nullptr, nullptr, lumenUnit, "LEVELER" },

        // SPECTRAL LIMITER: three knobs, then IN
        { ControlKind::knob,   oneUKnobX + 0.0f * oneUKnobStep, oneUKnobZ, pid::spectralRange, "RANGE", nullptr, nullptr, limiterUnit, "SPECTRAL LIMITER", 1.0f, KnobStyle::skirted },
        { ControlKind::knob,   oneUKnobX + 1.0f * oneUKnobStep, oneUKnobZ, pid::spectralRelease, "RELEASE", nullptr, nullptr, limiterUnit, "SPECTRAL LIMITER", 1.0f, KnobStyle::skirted },
        { ControlKind::knob,   oneUKnobX + 2.0f * oneUKnobStep, oneUKnobZ, pid::spectralCeiling, "CEILING", nullptr, nullptr, limiterUnit, "SPECTRAL LIMITER", 1.0f, KnobStyle::skirted },
        { ControlKind::toggle, oneUKnobX + 2.0f * oneUKnobStep + 0.38f, oneUButtonZ, pid::spectralActive, "IN", nullptr, nullptr, limiterUnit, "SPECTRAL LIMITER" },
    }};

    inline constexpr int numControls = (int) controls.size();

    /** Momentary buttons (PRESET PREV / NEXT) have no LED: there is no state to show. */
    inline bool hasLed (const ControlDef& c) noexcept
    {
        return std::string_view (c.paramId) != pad::params::id::presetPrev && std::string_view (c.paramId) != pad::params::id::presetNext;
    }

    inline int controlIndex (const char* paramId) noexcept
    {
        for (int i = 0; i < numControls; ++i)
            if (std::string_view (controls[(size_t) i].paramId) == paramId)
                return i;
        return -1;
    }

    // MODE has two LEDs (NORM, ADD) side by side above its button
    inline constexpr float modeLedDx = 0.065f;

    // --- LED ladders (bottom to top) -----------------------------------------------------
    inline constexpr int   ladderSegments = 12;
    inline constexpr float ladderBottomZ  = 0.685f;   // bottom LED clear of the label line (0.76)
    inline constexpr float ladderStep     = 0.046f;   // top LED clear of the section title rule

    struct Ladder { float x; int segments; const char* label; };

    inline constexpr Ladder detectLadder { 1.50f, 6, "DETECT" };   // inside FOOTSTEP, beside PRIORITY
    inline constexpr Ladder outLadder    { 1.93f, 12, "OUT" };
    inline constexpr Ladder enhLadder    { 2.14f, 12, "ENH" };

    inline constexpr float ladderLedZ (int segment) noexcept { return ladderBottomZ - (float) segment * ladderStep; }

    /** dB printed beside the OUT ladder, one per segment (bottom to top). */
    inline constexpr std::array<int, ladderSegments> outLadderDb {{ -30, -24, -18, -15, -12, -9, -6, -4, -3, -2, -1, 0 }};

    // Maker block to the right of the analyser window (the name used to be printed under the
    // window's cut-out), with the power LED above it rather than on the analyser glass
    inline constexpr float makerX = 2.03f, makerHalfW = 0.21f;
    inline constexpr float powerLedX = makerX, powerLedZ = -0.765f;

    /** Body radius of a knob control as drawn. */
    inline float knobBodyRadius (const ControlDef& c) noexcept
    {
        if (c.unit == tubeUnit)
            return (c.kind == ControlKind::selector ? 0.10f : tubeKnobBodyRadius) * c.size;
        if (isOneU (c.unit))
            return oneUKnobRadius * c.size;
        return knobRadius * c.size;
    }

    /** The display window on a unit's panel (the 1U units carry VU meters instead). */
    inline constexpr Rect unitDisplayRect (int unit) noexcept
    {
        return unit == tubeUnit ? seraphDisplayRect : displayRect;
    }
}
