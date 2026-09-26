#pragma once

#include <array>
#include <atomic>
#include <string_view>
#include <vector>
#include <utility>
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

    /** A switch or button's outline on the panel (half sizes), per style, measured once from the models. */
    struct Outline { float halfW = 0.0f, halfD = 0.0f; };

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

    /*  Four sections with a clear gutter (>= 0.10) between each. Footsteps have a unit of their own now
        (the FOOTSTEP RADAR); the METER section took their space, with a STEPS ladder that lights when the
        radar finds one. */
    inline constexpr std::array<Section, 4> sections {{
        { { -1.94f,  sectionCz, 0.38f,  sectionHd }, "MASTER" },
        { { -0.72f,  sectionCz, 0.74f,  sectionHd }, "CLARITY" },
        { {  0.51f,  sectionCz, 0.39f,  sectionHd }, "SUB" },
        { {  1.69f,  sectionCz, 0.65f,  sectionHd }, "METER" },
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

    inline Outline buttonOutline (hwk::models::ButtonStyle s)
    {
        static const auto table = []
        {
            std::array<Outline, hwk::models::numButtonStyles> t {};
            for (int i = 0; i < hwk::models::numButtonStyles; ++i)
            {
                const auto m = hwk::models::pushButton ((hwk::models::ButtonStyle) i, buttonHalfW, buttonHalfD, 0);
                t[(size_t) i] = { m.halfW, m.halfD };
            }
            return t;
        }();
        return table[(size_t) s];
    }
    inline constexpr float buttonTravel = 0.020f;
    inline constexpr float buttonLedDz  = -0.19f;   // LED above the button
    inline constexpr float ledRadius    = 0.020f;

    // --- Controls ---------------------------------------------------------------
    enum class ControlKind { knob, button, toggle, selector };

    /*  Panel names (what each unit does): enhUnit = ADAPTIVE ENHANCER, tubeUnit = TONE & SPACE,
        tideUnit = ADAPTIVE COMPRESSOR, lumenUnit = UPWARD LEVELER, limiterUnit = SPECTRAL LIMITER,
        levelUnit = LEVEL (1U, first), balancerUnit = MIX BALANCER (3U), monitorUnit = MONITOR (3U, on top:
        the rack's input against its output, and its loudness), deepUnit = DEEP SUB (1U: sub-harmonic synth
        and resonant hull).
        The identifiers keep the units' earlier names, as the parameter IDs do. */
    enum Unit { enhUnit = 0, tubeUnit = 1, tideUnit = 2, lumenUnit = 3, limiterUnit = 4, levelUnit = 5, balancerUnit = 6,
                monitorUnit = 7, deepUnit = 8, characterUnit = 9, radarUnit = 10, powerUnit = 11, lunchboxUnit = 12, numUnits = 13 };

    /** The rack's own units (in the case, on its arc); the LUNCHBOX stands beside it. */
    inline constexpr int numRackUnits = numUnits - 1;

    inline constexpr bool isOneU (int unit) noexcept { return unit == tideUnit || unit == lumenUnit || unit == limiterUnit || unit == deepUnit || unit == powerUnit; }

    /** The outboard family: brushed plate, engraved print, knobs in a bordered section, meters or a
        display behind glass. The four 1U units, LEVEL (1U), MIX BALANCER (3U) and the MONITOR (3U). */
    inline constexpr bool isOutboard (int unit) noexcept { return isOneU (unit) || unit == levelUnit || unit == balancerUnit || unit == monitorUnit || unit == characterUnit
                                                                  || unit == radarUnit
                                                                  || unit == powerUnit || unit == lunchboxUnit; }

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
        hwk::models::SwitchStyle switchStyle = hwk::models::SwitchStyle::rocker;   // toggles
        hwk::models::ButtonStyle buttonStyle = hwk::models::ButtonStyle::square;   // buttons
    };

    using hwk::models::KnobStyle;
    using hwk::models::SwitchStyle;
    using hwk::models::ButtonStyle;

    inline Outline switchOutline (SwitchStyle s)
    {
        static const auto table = []
        {
            std::array<Outline, hwk::models::numSwitchStyles> t {};
            for (int i = 0; i < hwk::models::numSwitchStyles; ++i)
            {
                const auto m = hwk::models::toggleSwitch ((SwitchStyle) i, 0);
                t[(size_t) i] = { m.halfW, m.halfD };
            }
            return t;
        }();
        return table[(size_t) s];
    }

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

            LEVEL (1U) -> ADAPTIVE ENHANCER (3U) -> UPWARD LEVELER (1U) -> SPECTRAL LIMITER (1U)
                -> MIX BALANCER (4U) -> ADAPTIVE COMPRESSOR (1U) -> TONE & SPACE (3U) -> MONITOR (3U) -> out
    */
    inline constexpr float rackGap    = 0.060f;   // air between panels, measured along the arc (kept tight: nine units)
    inline constexpr float oneUHalfH  = 0.295f;

    inline constexpr float lumenHalfH = oneUHalfH;
    inline constexpr float tideHalfH  = oneUHalfH;
    inline constexpr float tubeHalfH  = 0.82f;
    inline constexpr float levelHalfH    = oneUHalfH;          // 1U
    inline constexpr float monitorHalfH  = 3.0f * oneUHalfH;   // 3U
    inline constexpr float balancerHalfH = 3.0f * oneUHalfH;   // 3U (was 4U: the rack was getting tall)
    inline constexpr float characterHalfH = 2.0f * oneUHalfH;  // 2U: room to engrave nine model names round each selector
    inline constexpr float radarHalfH = 2.0f * oneUHalfH;      // 2U: CHARACTER's sister - the same plate, its own finish

    /*  The LUNCHBOX: a six-slot 500-series frame on a walnut stand to the right of the case. A module is
        1.5 x 5.25 inches; at this scale (the rack's 19 inches are 5.0) a slot is 0.40 wide, 1.38 tall. */
    inline constexpr float lbSlotW   = 0.50f;   // (a touch wider and taller than a real card, to read at a distance)
    inline constexpr int   lbSlots   = 6;
    inline constexpr float lbFrame   = 0.10f;                          // the frame round the slots
    inline constexpr float lbHalfW   = 0.5f * lbSlotW * (float) lbSlots + lbFrame;
    inline constexpr float lbModuleHalfH = 0.96f;
    inline constexpr float lbTitleZ = -0.80f;          // each module's name, under its top screw
    inline constexpr float lbCutLedDx = 0.14f, lbCutLedZ = -0.62f;   // DE-HARSH's CUT lamp, beside its IN
    inline constexpr float lbHalfH   = lbModuleHalfH + lbFrame;
    inline constexpr float lbStandH  = 2.30f;                          // floor to the frame's underside
    inline constexpr float slotX (int slot) noexcept { return -0.5f * lbSlotW * (float) (lbSlots - 1) + lbSlotW * (float) slot; }
    /** The modules: first slot, how many slots wide, and which is which. The last slot is left empty. */
    struct LbModule { int slot, width; const char* name; };
    inline constexpr std::array<LbModule, 4> lbModules {{ { 0, 2, "CLASS-A EQ" }, { 2, 1, "DE-HARSH" }, { 3, 1, "CROSSFEED" }, { 4, 1, "OUTPUT" } }};
    inline constexpr int lbEmptySlot = 5;
    inline constexpr float slotCentre (int first, int width) noexcept { return -0.5f * lbSlotW * (float) (lbSlots - 1) + lbSlotW * ((float) first + 0.5f * (float) (width - 1)); }
    inline constexpr float lbEqX = slotCentre (0, 2), lbHarshX = slotCentre (2, 1), lbFeedX = slotCentre (3, 1);
    inline constexpr float lbModuleX (int m) noexcept
    {
        return slotX (lbModules[(size_t) m].slot) + 0.5f * lbSlotW * (float) (lbModules[(size_t) m].width - 1);
    }

    inline constexpr float arcRadius  = 9.60f;    // viewer to panel
    inline constexpr float arcCentreY = 1.62f;    // the viewer's eye height
    inline constexpr float arcCentreZ = 10.05f;   // and where they are standing
    inline constexpr float unitRecess = 0.03f;    // faceplates sit this far inside the cheeks' arc, on the rails

    inline constexpr float unitHalfH (int unit) noexcept
    {
        return unit == tubeUnit ? tubeHalfH : isOneU (unit) ? oneUHalfH : unit == levelUnit ? levelHalfH
             : unit == balancerUnit ? balancerHalfH : unit == monitorUnit ? monitorHalfH
             : unit == characterUnit ? characterHalfH : unit == radarUnit ? radarHalfH : unit == lunchboxUnit ? lbHalfH : faceHalfH;
    }

    /** Half the width of a unit's faceplate: 19 inches for the rack's, the LUNCHBOX's own frame. */
    inline constexpr float unitHalfW (int unit) noexcept
    {
        return unit == lunchboxUnit ? lbHalfW : faceHalfW;
    }

    /** Units in case order, bottom to top - which is also the order the signal runs. */
    inline constexpr std::array<int, numRackUnits> rackOrder { powerUnit, levelUnit, enhUnit, lumenUnit, deepUnit, limiterUnit, balancerUnit, tideUnit, radarUnit,
                                                           tubeUnit, characterUnit, monitorUnit };

    /** SIMPLE view: the units that work by themselves are taken out of the case and the rack closes up
        around the rest (they still run, as the preset set them). A bit per unit; both threads read it. */
    inline constexpr float hiddenY = -1000.0f;
    inline std::atomic<unsigned> hiddenUnits { 0u };
    inline constexpr unsigned simpleViewHidden = (1u << levelUnit) | (1u << lumenUnit) | (1u << deepUnit) | (1u << limiterUnit)
                                               | (1u << balancerUnit) | (1u << tideUnit)
                                               | (1u << lunchboxUnit);
    inline bool isShown (int unit) noexcept { return ((hiddenUnits.load (std::memory_order_relaxed) >> unit) & 1u) == 0u; }

    inline int bottomUnit() noexcept
    {
        for (int u : rackOrder)
            if (isShown (u))
                return u;
        return rackOrder.front();
    }

    inline int topUnit() noexcept
    {
        for (auto it = rackOrder.rbegin(); it != rackOrder.rend(); ++it)
            if (isShown (*it))
                return *it;
        return rackOrder.back();
    }

    /** Distance along the arc from the bottom of the stack to the centre of a unit (the units shown). */
    inline float unitArcPos (int unit) noexcept
    {
        float pos = 0.0f;
        for (int u : rackOrder)
        {
            if (! isShown (u))
                continue;
            pos += unitHalfH (u);
            if (u == unit)
                return pos;
            pos += unitHalfH (u) + rackGap;
        }
        return pos;
    }

    inline float totalArcLength() noexcept
    {
        float total = 0.0f;
        int shown = 0;
        for (int u : rackOrder)
            if (isShown (u))
            {
                total += 2.0f * unitHalfH (u);
                ++shown;
            }
        return total + (float) std::max (0, shown - 1) * rackGap;
    }

    inline constexpr float caseOverhangArc = 0.30f, caseBoardTArc = 0.10f;   // (caseOverhang, caseBoardT: the floor the stand is on)

    /** How far a unit is rotated toward the viewer: 0 at the middle of the case. */
    inline float unitAngle (int unit) noexcept
    {
        if (unit == lunchboxUnit)
            return 0.0f;   // upright, facing the room
        return (unitArcPos (unit) - 0.5f * totalArcLength()) / arcRadius;
    }

    /** Centre of a unit's faceplate, in world space. */
    inline gfx::Vec3 unitOrigin (int unit) noexcept
    {
        if (! isShown (unit))
            return { 0.0f, hiddenY, 0.0f };   // out of the case: far below anything the camera sees
        if (unit == lunchboxUnit)
        {
            // On its stand, right of the case, a little forward of the middle units
            const float a0 = (-caseOverhangArc - 0.5f * totalArcLength()) / arcRadius;
            const float floorY = arcCentreY + arcRadius * std::sin (a0) - caseBoardTArc - 0.05f;
            return { faceHalfW + 0.115f + 0.28f + 0.55f + lbHalfW, floorY + lbStandH + lbHalfH, arcCentreZ - arcRadius + 0.85f };
        }
        const float a = unitAngle (unit);
        const float r = arcRadius + unitRecess;
        return { 0.0f, arcCentreY + r * std::sin (a), arcCentreZ - r * std::cos (a) };
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
        { "ADAPTIVE ENHANCER",   "ADAPTIVE EQ - HARMONIC EXCITER - SUB",                     2 },
        { "TONE & SPACE",        "FINISHING PROCESSOR - LOUDNESS-MATCHED",                   9 },
        { "ADAPTIVE COMPRESSOR", "PROGRAM-DEPENDENT - AUTO THRESHOLD",                       7 },
        { "UPWARD LEVELER",      "3-BAND - LIFTS QUIET DETAIL",                              3 },
        { "SPECTRAL LIMITER",    "ANTI-PUMP DYNAMIC EQ",                                     5 },
        { "LEVEL CONTROL",       "THE RACK'S WORKING LEVEL",                                 1 },
        { "MIX BALANCER",        "SIX-BAND DYNAMIC BALANCE",                                 6 },
        { "OUTPUT MONITOR",      "INPUT AGAINST OUTPUT - LOUDNESS",                         12 },
        { "DEEP SUB",            "SUB-HARMONIC SYNTH - RESONANT HULL",                       4 },
        { "CHARACTER",           "CONSOLES - TAPE - VALVES - MORPH",                        10 },
        { "FOOTSTEP RADAR",      "FINDS EVERY STEP - NEAR AND FAR",                          8 },
        { "POWER",               "CONDITIONED POWER - RACK LIGHTS",                          0 },
        { "LUNCHBOX",            "CLASS-A EQ - DE-HARSH - CROSSFEED",                       11 },
    }};

    // --- the case the units are screwed into -------------------------------------------
    inline constexpr float caseSideX     = faceHalfW + 0.115f;   // inner face of each cheek
    inline constexpr float caseCheekW    = 0.28f;                // solid walnut
    inline constexpr float caseFront     = 0.05f;                // the cheeks stand this far proud of the panels
    inline constexpr float caseBoardT    = 0.10f;                // crown and plinth thickness
    inline constexpr float caseDepth     = 0.78f;                // how far the case runs back
    inline constexpr float caseOverhang  = 0.30f;                // past the top and bottom unit
    inline constexpr int   caseArcSteps  = 72;                   // segments along the curve (smooth on the rounded edges)

    /*  Front mounting rails, one each side. In a curved cabinet they are straight segments, one per
        unit, lying flat against the back of that unit's faceplate (the ears are clamped to them by the
        ear screws), meeting at the middle of each gap. They run from just inside the ear screws out to
        the cheeks, and show in the gaps between units. */
    inline constexpr float railInnerX    = 2.28f;                // ear screws sit at +-2.34
    inline constexpr float railFront     = faceThick + 0.0015f;  // flush behind the faceplate (clear of the slot floors)
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
    inline constexpr Rect  seraphDisplayRect  { -0.30f, -0.46f, 1.22f, 0.26f };   // HEAVEN + AUTO on its right
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

    /** CHARACTER's model selectors: nine positions round three quarters of a turn, each engraved. */
    inline constexpr int characterModels = 9;
    inline constexpr std::array<const char*, characterModels> characterModelNames {
        "CLEAN", "BRIT", "AMER", "VINTAGE", "TAPE 15", "TAPE 30", "VALVE", "ARENA", "CINEMA" };
    // POWER strip: its mains switch (lit, fixed: not a control) and three lamps (PROTECTED, GROUNDED, POWER)
    inline constexpr float stripSwitchX = -1.42f, stripSwitchZ = -0.03f;
    inline constexpr std::array<float, 3> stripLedX { -1.02f, -0.78f, -0.54f };
    /*  Its eight front outlets (US, NEMA 5-15R: hot and neutral slots over a round ground hole), on 0.34
        centres from the middle to the right ear. Which are in use: the plugs of the rack and the LUNCHBOX. */
    inline constexpr int stripOutlets = 8;
    inline constexpr float stripOutletX (int i) noexcept { return -0.25f + 0.34f * (float) i; }
    inline constexpr float stripOutletZ = 0.0f;
    inline constexpr std::array<bool, stripOutlets> stripPlugged { true, true, true, false, true, true, true, false };
    inline constexpr float stripLedZ = -0.04f;

    inline float characterSelectorAngle (float normalised) noexcept { return (normalised - 0.5f) * 2.0f * (135.0f * pi / 180.0f); }

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
        return unit == limiterUnit ? Rect { -0.73f, 0.0f, 0.92f, 0.245f }
             : unit == levelUnit ? Rect { -0.99f, 0.0f, 0.66f, 0.245f }
             : unit == monitorUnit ? Rect { 1.83f, 0.52f, 0.40f, 0.28f }
             : unit == balancerUnit ? Rect { 1.73f, 0.0f, 0.55f, 0.74f }
             : unit == deepUnit ? Rect { -0.41f, 0.0f, 1.21f, 0.245f }
             : unit == characterUnit ? Rect { -0.10f, -0.02f, 1.46f, 0.47f }
             : unit == radarUnit ? Rect { -0.10f, -0.02f, 1.46f, 0.47f }
             : unit == powerUnit ? Rect { -1.00f, 0.0f, 0.62f, 0.245f }
             : unit == lunchboxUnit ? Rect { 0.0f, 0.0f, 0.0f, 0.0f }
                                    : Rect { -0.99f, 0.0f, 0.66f, 0.245f };
    }

    /** Where an outboard unit's maker block (its name) starts, down the panel. */
    inline constexpr float makerTopZ (int unit) noexcept { return isOneU (unit) || unit == levelUnit ? -0.176f : -unitHalfH (unit) + 0.20f; }

    // MONITOR: its big display (waveform and spectrum, in against out); MIX BALANCER: its display
    inline constexpr Rect  monitorDisplayRect  { -0.16f, -0.02f, 1.47f, 0.76f };
    inline constexpr Rect  balancerDisplayRect { -0.28f, -0.02f, 1.35f, 0.76f };   // narrower: its knobs are a 2 x 2 grid
    inline constexpr float windowDepth         = 0.045f;

    /** The windows cut into an outboard unit's plate besides its meters. */
    inline std::vector<Rect> outboardWindows (int unit)
    {
        if (unit == lunchboxUnit)
            return { { slotX (lbEmptySlot), 0.0f, 0.5f * lbSlotW - 0.012f, lbModuleHalfH - 0.012f } };   // the empty slot
        if (unit == monitorUnit) return { monitorDisplayRect };
        if (unit == balancerUnit) return { balancerDisplayRect };
        return {};
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

    inline constexpr float levelVuHalfW   = 0.76f;        // LEVEL: one wide INPUT meter, like the compressor's
    inline constexpr float levelVuX       = 1.34f;
    inline constexpr float deepVuHalfW    = 0.62f;        // DEEP SUB: one meter, what it is adding
    inline constexpr float deepVuX        = 1.55f;
    inline constexpr float characterVuHalfW = 0.40f;      // CHARACTER: one meter, the harmonics it adds
    inline constexpr float characterVuX     = 1.88f;
    inline constexpr float radarVuHalfW = 0.40f;          // FOOTSTEP RADAR: the same meter, the lift it gives a step
    inline constexpr float radarVuX     = 1.88f;
    inline constexpr float lbVuHalfW    = 0.155f;         // LUNCHBOX: the OUTPUT module's meter
    inline constexpr float lbVuZ        = -0.46f;
    inline constexpr float monitorVuHalfW = 0.36f;
    inline constexpr float monitorVuX     = 1.83f;        // MOMENTARY above SHORT-TERM
    inline constexpr std::array<float, 2> monitorVuZ { -0.60f, -0.08f };

    inline constexpr int numVus (int unit) noexcept
    {
        return unit == tideUnit || unit == levelUnit || unit == deepUnit || unit == characterUnit || unit == radarUnit || unit == lunchboxUnit ? 1
             : unit == limiterUnit || unit == monitorUnit ? 2 : unit == lumenUnit ? 3 : 0;
    }

    inline constexpr float vuHalfW (int unit) noexcept
    {
        return unit == tideUnit ? tideVuHalfW : unit == limiterUnit ? limiterVuHalfW : unit == levelUnit ? levelVuHalfW
             : unit == monitorUnit ? monitorVuHalfW : unit == deepUnit ? deepVuHalfW : unit == characterUnit ? characterVuHalfW
             : unit == radarUnit ? radarVuHalfW : unit == lunchboxUnit ? lbVuHalfW : lumenVuHalfW;
    }

    inline constexpr float vuX (int unit, int index) noexcept
    {
        return unit == tideUnit ? tideVuX
             : unit == limiterUnit ? limiterVuX + (float) index * limiterVuStep
             : unit == levelUnit ? levelVuX
             : unit == monitorUnit ? monitorVuX
             : unit == deepUnit ? deepVuX
             : unit == characterUnit ? characterVuX
             : unit == radarUnit ? radarVuX
             : unit == lunchboxUnit ? lbModuleX (3)
                                 : lumenVuX + (float) index * lumenVuStep;
    }

    inline constexpr float vuZ (int unit, int index) noexcept
    {
        return unit == monitorUnit ? monitorVuZ[(size_t) std::clamp (index, 0, 1)] : unit == lunchboxUnit ? lbVuZ : vuCentreZ;
    }

    /** First needle of each metered unit in the renderer's needle array (compressor 1, leveler 3, limiter 2,
        level 1, monitor 2). */
    inline constexpr int firstNeedle (int unit) noexcept
    {
        return unit == tideUnit ? 0 : unit == lumenUnit ? 1 : unit == levelUnit ? 6 : unit == monitorUnit ? 7 : unit == deepUnit ? 9
             : unit == characterUnit ? 10 : unit == radarUnit ? 11 : unit == lunchboxUnit ? 12 : 4;
    }
    inline constexpr int numNeedles = 13;

    inline constexpr float oneUDisplayDepth = vuDepth;

    inline constexpr std::array<Rect, 2> oneUEarSlots {{
        { -2.36f, 0.0f, 0.075f, 0.036f }, { 2.36f, 0.0f, 0.075f, 0.036f },
    }};

    /** An outboard unit's ear slots: one each side on a 1U, two each side on the taller ones. */
    inline std::vector<Rect> outboardEarSlots (int unit)
    {
        if (unit == lunchboxUnit)
            return {};   // a frame on a stand, not in a rack: each module has its own two screws
        if (isOneU (unit) || unit == levelUnit)
            return { oneUEarSlots.begin(), oneUEarSlots.end() };
        const float z = unitHalfH (unit) - oneUHalfH;   // one rack unit in from each edge (2U: CHARACTER, 3U: MONITOR, MIX BALANCER)
        return { { -2.36f, -z, 0.075f, 0.036f }, { -2.36f, z, 0.075f, 0.036f }, { 2.36f, -z, 0.075f, 0.036f }, { 2.36f, z, 0.075f, 0.036f } };
    }

    // PRESET PREV / NEXT, in the enhancer's maker block (right of the analyser window)
    inline constexpr std::array<float, 2> presetButtonX { 1.93f, 2.13f };
    inline constexpr float presetButtonZ = -0.235f;

    // CLARITY is one physical knob with two printed scales: NORM (0-30) and ADD + NORM (0-10).
    // Each mode keeps its own setting; the MODE button swaps which one the knob drives.
    inline constexpr std::array<ControlDef, 85> controls {{
        { ControlKind::button, -1.29f, buttonZ, pid::clarityMode, "MODE" },
        { ControlKind::knob,   -0.86f, knobZ,   pid::clarityNorm, "CLARITY", pid::clarityAdd, pid::clarityMode, enhUnit, nullptr, 1.0f, KnobStyle::smallRibbed },
        { ControlKind::knob,   -0.27f, knobZ,   pid::adaptSpeed,  "ADAPT", nullptr, nullptr, enhUnit, nullptr, 1.0f, KnobStyle::smallRibbed },
        { ControlKind::knob,    0.42f, knobZ,   pid::sub,         "SUB", nullptr, nullptr, enhUnit, nullptr, 1.0f, KnobStyle::smallRibbed },
        { ControlKind::button,  0.79f, buttonZ, pid::subBoost,    "BOOST" },
        { ControlKind::knob,   masterKnobX2[0], knobZ, pid::enhMultiply, "MULTIPLY", nullptr, nullptr, enhUnit, "ENHANCER", masterKnobSize, KnobStyle::smallRibbed },
        { ControlKind::knob,   masterKnobX2[1], knobZ, pid::enhStrength, "STRENGTH", nullptr, nullptr, enhUnit, "ENHANCER", masterKnobSize, KnobStyle::smallRibbed },

        // Rack-wide PRESET buttons in the maker block (momentary; the name shows on the analyser)
        { ControlKind::button, presetButtonX[0], presetButtonZ, pid::presetPrev, "PREV", nullptr, nullptr, enhUnit, "PRESET" },
        { ControlKind::button, presetButtonX[1], presetButtonZ, pid::presetNext, "NEXT", nullptr, nullptr, enhUnit, "PRESET" },

        { ControlKind::knob, seraphFirstKnobX + 0.0f * seraphKnobStep, seraphRow1Z, pid::silkSmooth, "SMOOTH", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 1.0f * seraphKnobStep, seraphRow1Z, pid::silkAir, "AIR", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 2.0f * seraphKnobStep, seraphRow1Z, pid::silkWarmth, "WARMTH", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 3.0f * seraphKnobStep, seraphRow1Z, pid::silkBody, "BODY", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 4.0f * seraphKnobStep, seraphRow1Z, pid::silkOutput, "OUTPUT", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 5.0f * seraphKnobStep, seraphRow1Z, pid::silkSub, "SUB", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::pultecTopHat },

        { ControlKind::knob, seraphFirstKnobX + 0.0f * seraphKnobStep, seraphRow2Z, pid::haloWidth, "WIDTH", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 1.0f * seraphKnobStep, seraphRow2Z, pid::haloSpace, "REVERB", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 2.0f * seraphKnobStep, seraphRow2Z, pid::haloDecay, "DECAY", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 3.0f * seraphKnobStep, seraphRow2Z, pid::haloShimmer, "SHIMMER", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphFirstKnobX + 4.0f * seraphKnobStep, seraphRow2Z, pid::haloTone, "TONE", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::pultecTopHat },

        // LOUDNESS: one knob, two printed scales (HOLD / LIFT + HOLD), a button to swap between them
        { ControlKind::knob,   0.74f, seraphRow2Z, pid::heavenHold, "LOUDNESS", pid::heavenLift, pid::heavenMode, tubeUnit, "TONE & SPACE", 1.18f, KnobStyle::pultecTopHat },
        { ControlKind::button, 1.22f, seraphRow2Z, pid::heavenMode, "LIFT", nullptr, nullptr, tubeUnit, "LOUDNESS", 1.0f, KnobStyle::proXl, SwitchStyle::rocker, ButtonStyle::chromeBezel },

        // AUTO heaven: the button hands the space to the unit, HEAVEN says how far it may take it
        { ControlKind::knob,   1.20f, -0.50f, pid::heavenAutoAmount, "HEAVEN", nullptr, nullptr, tubeUnit, "TONE & SPACE", 0.88f, KnobStyle::pultecTopHat },
        { ControlKind::button, 1.20f, -0.215f, pid::heavenAuto, "AUTO", nullptr, nullptr, tubeUnit, "HEAVEN", 1.0f, KnobStyle::proXl, SwitchStyle::rocker, ButtonStyle::round },

        { ControlKind::toggle, 1.78f, seraphRow1Z - 0.10f, pid::silkProtect, "PROTECT", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::toggle, 2.12f, seraphRow1Z - 0.10f, pid::silkTape, "TAPE", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::toggle, 1.44f, seraphRow1Z - 0.10f, pid::silkAuto, "MATCH", nullptr, nullptr, tubeUnit, "TONE", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::toggle, 1.78f, seraphRow2Z + 0.02f, pid::haloDuck, "DUCK", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::toggle, 2.12f, seraphRow2Z + 0.02f, pid::haloBassMono, "BASS MONO", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::toggle, 1.44f, seraphRow2Z + 0.02f, pid::haloMod, "MOD", nullptr, nullptr, tubeUnit, "SPACE", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },

        { ControlKind::selector, 1.80f, -0.44f, pid::seraphMode, "POWER", nullptr, nullptr, tubeUnit, "TONE & SPACE", 1.15f, KnobStyle::chickenHead },
        { ControlKind::knob, seraphMasterX[0], seraphMasterZ, pid::seraphMultiply, "MULTIPLY", nullptr, nullptr, tubeUnit, "TONE & SPACE", masterKnobSize, KnobStyle::pultecTopHat },
        { ControlKind::knob, seraphMasterX[1], seraphMasterZ, pid::seraphStrength, "STRENGTH", nullptr, nullptr, tubeUnit, "TONE & SPACE", masterKnobSize, KnobStyle::pultecTopHat },

        { ControlKind::knob,   oneUKnobX + 0.0f * oneUKnobStep, oneUKnobZ, pid::tideMix, "MIX", nullptr, nullptr, tideUnit, "COMPRESSOR", 1.0f, KnobStyle::fetSilver },
        { ControlKind::knob,   oneUKnobX + 1.0f * oneUKnobStep, oneUKnobZ, pid::tideResponse, "RESPONSE", nullptr, nullptr, tideUnit, "COMPRESSOR", 1.0f, KnobStyle::fetSilver },
        { ControlKind::toggle, oneUButtonX, oneUButtonZ, pid::tideActive, "IN", nullptr, nullptr, tideUnit, "COMPRESSOR" },

        { ControlKind::knob,   oneUKnobX + 0.0f * oneUKnobStep, oneUKnobZ, pid::lumenTarget, "TARGET", nullptr, nullptr, lumenUnit, "LEVELER", 1.0f, KnobStyle::la2aFluted },
        { ControlKind::knob,   oneUKnobX + 1.0f * oneUKnobStep, oneUKnobZ, pid::lumenResponse, "RESPONSE", nullptr, nullptr, lumenUnit, "LEVELER", 1.0f, KnobStyle::la2aFluted },
        { ControlKind::toggle, oneUButtonX, oneUButtonZ, pid::lumenActive, "IN", nullptr, nullptr, lumenUnit, "LEVELER", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },

        // SPECTRAL LIMITER: three knobs, then IN
        { ControlKind::knob,   oneUKnobX + 0.0f * oneUKnobStep, oneUKnobZ, pid::spectralRange, "RANGE", nullptr, nullptr, limiterUnit, "SPECTRAL LIMITER", 1.0f, KnobStyle::smallRibbed },
        { ControlKind::knob,   oneUKnobX + 1.0f * oneUKnobStep, oneUKnobZ, pid::spectralRelease, "RELEASE", nullptr, nullptr, limiterUnit, "SPECTRAL LIMITER", 1.0f, KnobStyle::smallRibbed },
        { ControlKind::knob,   oneUKnobX + 2.0f * oneUKnobStep, oneUKnobZ, pid::spectralCeiling, "CEILING", nullptr, nullptr, limiterUnit, "SPECTRAL LIMITER", 1.0f, KnobStyle::smallRibbed },
        { ControlKind::toggle, oneUKnobX + 2.0f * oneUKnobStep + 0.38f, oneUButtonZ, pid::spectralActive, "IN", nullptr, nullptr, limiterUnit, "SPECTRAL LIMITER", 1.0f, KnobStyle::proXl, SwitchStyle::rockerRed },

        // LEVEL & LOUDNESS: the working level, and RESET for the integrated loudness / true-peak hold
        { ControlKind::knob,   oneUKnobX + 0.26f, oneUKnobZ, pid::levelGain, "LEVEL", nullptr, nullptr, levelUnit, "LEVEL", 1.0f, KnobStyle::fetSilver },
        { ControlKind::knob,   1.66f, 0.52f, pid::monitorSpeed, "SPEED", nullptr, nullptr, monitorUnit, "MONITOR", 0.8f, KnobStyle::fetSilver },
        { ControlKind::button, 2.06f, 0.50f, pid::loudnessReset, "RESET", nullptr, nullptr, monitorUnit, "LOUDNESS" },
        // COMPARE: the input at the output's loudness, under the name (a level-matched A/B)
        { ControlKind::button, -1.98f, 0.50f, pid::abCompare, "COMPARE", nullptr, nullptr, monitorUnit, "MONITOR" },

        // MIX BALANCER: four knobs down the right, IN under the name
        { ControlKind::knob,   1.46f, -0.33f, pid::balAmount, "BALANCE", nullptr, nullptr, balancerUnit, "MIX BALANCER", 0.92f, KnobStyle::manleyRibbed },
        { ControlKind::knob,   2.00f, -0.33f, pid::balSpeed,  "SPEED",   nullptr, nullptr, balancerUnit, "MIX BALANCER", 0.92f, KnobStyle::manleyRibbed },
        { ControlKind::knob,   1.46f,  0.31f, pid::balTilt,   "TILT",    nullptr, nullptr, balancerUnit, "MIX BALANCER", 0.92f, KnobStyle::manleyRibbed },
        { ControlKind::knob,   2.00f,  0.31f, pid::balRange,  "RANGE",   nullptr, nullptr, balancerUnit, "MIX BALANCER", 0.92f, KnobStyle::manleyRibbed },
        { ControlKind::knob,   -1.98f, -0.10f, pid::balResolution, "RESOLUTION", nullptr, nullptr, balancerUnit, "MIX BALANCER", 1.0f, KnobStyle::manleyRibbed },
        { ControlKind::toggle, -1.98f, 0.50f, pid::balActive, "IN",      nullptr, nullptr, balancerUnit, "MIX BALANCER" },

        // DEEP SUB: four knobs and IN in the section box, one meter on the right
        { ControlKind::knob,   oneUKnobX + 0.0f * oneUKnobStep, oneUKnobZ, pid::deepDepth,    "DEPTH",    nullptr, nullptr, deepUnit, "DEEP SUB", 1.0f, KnobStyle::smallRibbed },
        { ControlKind::knob,   oneUKnobX + 1.0f * oneUKnobStep, oneUKnobZ, pid::deepHull,     "HULL",     nullptr, nullptr, deepUnit, "DEEP SUB", 1.0f, KnobStyle::smallRibbed },
        { ControlKind::knob,   oneUKnobX + 2.0f * oneUKnobStep, oneUKnobZ, pid::deepSize,     "SIZE",     nullptr, nullptr, deepUnit, "DEEP SUB", 1.0f, KnobStyle::smallRibbed },
        { ControlKind::knob,   oneUKnobX + 3.0f * oneUKnobStep, oneUKnobZ, pid::deepPressure, "PRESSURE", nullptr, nullptr, deepUnit, "DEEP SUB", 1.0f, KnobStyle::smallRibbed },
        { ControlKind::toggle, oneUKnobX + 3.0f * oneUKnobStep + 0.38f, oneUButtonZ, pid::deepActive, "IN", nullptr, nullptr, deepUnit, "DEEP SUB", 1.0f, KnobStyle::proXl, SwitchStyle::rockerRed },

        // CHARACTER (2U): A and B selectors with BLEND between them, DRIVE, IN; one meter on the right
        { ControlKind::selector, -1.22f, -0.05f, pid::charModelA, "A",     nullptr, nullptr, characterUnit, "CHARACTER", 1.10f, KnobStyle::chickenHead },
        { ControlKind::knob,     -0.705f, -0.05f, pid::charBlend,  "BLEND", nullptr, nullptr, characterUnit, "CHARACTER", 1.0f, KnobStyle::porticoBlack },
        { ControlKind::selector, -0.19f, -0.05f, pid::charModelB, "B",     nullptr, nullptr, characterUnit, "CHARACTER", 1.10f, KnobStyle::chickenHead },
        { ControlKind::knob,      0.325f, -0.05f, pid::charColour, "COLOUR", nullptr, nullptr, characterUnit, "CHARACTER", 1.0f, KnobStyle::porticoRed },
        { ControlKind::knob,      0.84f, -0.05f, pid::charDrive,  "DRIVE", nullptr, nullptr, characterUnit, "CHARACTER", 1.0f, KnobStyle::porticoBlack },
        { ControlKind::toggle,    1.20f, -0.24f, pid::charActive, "IN",    nullptr, nullptr, characterUnit, "CHARACTER", 1.0f, KnobStyle::proXl, SwitchStyle::rockerRed },
        { ControlKind::toggle,    1.20f,  0.16f, pid::charGrit,   "GRIT",  nullptr, nullptr, characterUnit, "CHARACTER", 1.0f, KnobStyle::proXl, SwitchStyle::rocker },

        // FOOTSTEP RADAR (2U, CHARACTER's layout): SENSITIVITY, BOOST, SPACE in the box, IN and LISTEN, one meter
        { ControlKind::knob,     -1.00f, -0.05f, pid::radarSens,  "SENSITIVITY", nullptr, nullptr, radarUnit, "RADAR", 1.25f, KnobStyle::fetSilver },
        { ControlKind::knob,     -0.20f, -0.05f, pid::radarBoost, "BOOST", nullptr, nullptr, radarUnit, "RADAR", 1.25f, KnobStyle::fetSilver },
        { ControlKind::knob,      0.60f, -0.05f, pid::radarSpace, "SPACE", nullptr, nullptr, radarUnit, "RADAR", 1.25f, KnobStyle::fetSilver },
        { ControlKind::toggle,    1.20f, -0.24f, pid::footstep,   "IN",    nullptr, nullptr, radarUnit, "RADAR", 1.0f, KnobStyle::proXl, SwitchStyle::rockerRed },
        // LUNCHBOX: CLASS-A EQ (double width: two staggered columns), DE-HARSH, CROSSFEED; OUTPUT is its meter
        { ControlKind::toggle,   lbEqX - 0.26f, -0.62f, pid::lbEqIn,   "IN",     nullptr, nullptr, lunchboxUnit, "EQ", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::toggle,   lbEqX,         -0.62f, pid::lbMidHiQ, "HI Q",   nullptr, nullptr, lunchboxUnit, "EQ", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::toggle,   lbEqX + 0.26f, -0.62f, pid::lbIron,   "IRON",   nullptr, nullptr, lunchboxUnit, "EQ", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::knob,     lbEqX - 0.22f, -0.26f, pid::lbHighGain, "HIGH", nullptr, nullptr, lunchboxUnit, "EQ", 0.74f, KnobStyle::neveSmallGrey },
        { ControlKind::knob,     lbEqX + 0.22f, -0.26f, pid::lbMidGain,  "MID",  nullptr, nullptr, lunchboxUnit, "EQ", 0.74f, KnobStyle::neveMaroon },
        { ControlKind::selector, lbEqX - 0.22f,  0.21f, pid::lbMidFreq,  "MID kHz", nullptr, nullptr, lunchboxUnit, "EQ", 0.62f, KnobStyle::neveGrey },
        { ControlKind::knob,     lbEqX + 0.22f,  0.21f, pid::lbLowGain,  "LOW",  nullptr, nullptr, lunchboxUnit, "EQ", 0.74f, KnobStyle::neveSmallGrey },
        { ControlKind::selector, lbEqX - 0.22f,  0.68f, pid::lbLowFreq,  "LOW Hz", nullptr, nullptr, lunchboxUnit, "EQ", 0.62f, KnobStyle::neveGrey },
        { ControlKind::selector, lbEqX + 0.22f,  0.68f, pid::lbHpf,      "HPF",  nullptr, nullptr, lunchboxUnit, "EQ", 0.62f, KnobStyle::neveGrey },
        { ControlKind::toggle,   lbHarshX, -0.62f, pid::lbHarshIn,     "IN",     nullptr, nullptr, lunchboxUnit, "DE-HARSH", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::knob,     lbHarshX, -0.23f, pid::lbHarshAmount, "AMOUNT", nullptr, nullptr, lunchboxUnit, "DE-HARSH", 0.66f, KnobStyle::apiRed },
        { ControlKind::selector, lbHarshX,  0.22f, pid::lbHarshFreq,   "FREQ",   nullptr, nullptr, lunchboxUnit, "DE-HARSH", 0.62f, KnobStyle::apiWhite },
        { ControlKind::knob,     lbHarshX,  0.62f, pid::lbHarshSpeed,  "SPEED",  nullptr, nullptr, lunchboxUnit, "DE-HARSH", 0.58f, KnobStyle::apiWhite },
        { ControlKind::toggle,   lbFeedX,  -0.62f, pid::lbFeedIn,      "IN",     nullptr, nullptr, lunchboxUnit, "CROSSFEED", 1.0f, KnobStyle::proXl, SwitchStyle::batToggle },
        { ControlKind::knob,     lbFeedX,  -0.10f, pid::lbFeedAmount,  "AMOUNT", nullptr, nullptr, lunchboxUnit, "CROSSFEED", 0.95f, KnobStyle::apiBlue },

        { ControlKind::toggle,    1.20f,  0.16f, pid::radarListen, "LISTEN", nullptr, nullptr, radarUnit, "RADAR", 1.0f, KnobStyle::proXl, SwitchStyle::rocker },
    }};

    inline constexpr int numControls = (int) controls.size();

    /** Momentary buttons (PRESET PREV / NEXT, loudness RESET) have no LED: there is no state to show. */
    inline bool hasLed (const ControlDef& c) noexcept
    {
        return std::string_view (c.paramId) != pad::params::id::presetPrev && std::string_view (c.paramId) != pad::params::id::presetNext
            && std::string_view (c.paramId) != pad::params::id::loudnessReset;
    }

    /** Where a button's LED sits relative to the button: above it, except AUTO heaven's, which goes to
        its right (above it is the HEAVEN knob). */
    inline std::pair<float, float> ledOffset (const ControlDef& c) noexcept
    {
        if (std::string_view (c.paramId) == pad::params::id::heavenAuto)
            return { 0.135f, 0.0f };
        return { 0.0f, buttonLedDz };
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

    inline constexpr Ladder detectLadder { 1.30f, 6, "STEPS" };    // the FOOTSTEP RADAR's finds, in METER
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
        if (isOutboard (c.unit))
            return oneUKnobRadius * c.size;
        return knobRadius * c.size;
    }

    /** The display window on a unit's panel (the 1U units carry VU meters instead). */
    inline constexpr Rect unitDisplayRect (int unit) noexcept
    {
        return unit == tubeUnit ? seraphDisplayRect : displayRect;
    }
}
