#pragma once

#include "../HardwareKit.h"

/*  ENH Master's material table: HardwareKit library materials plus the two plugin-specific display
    surfaces. One GL program per entry, indexed by `Material`. */
namespace pad::shaders
{
    enum Material
    {
        chassis = 0, faceplate, chrome, plastic, table, emissive, recess, print, display, shadow,
        paint, seraphDisplay, brushed, vuFace, vuGlass, callout, glow, valueArc, lens, sunlight,
        numMaterials
    };

    /** ENH Master display glass: live spectral gain curve. uParams = (_, brightness, aspect, footstep confidence) */
    inline const hwk::shaders::Material enhDisplay { "enhDisplay", R"GLSL(
    vec2 uv = vUV;
    vec3 phosphor = vec3 (0.40, 1.0, 0.80);
    col = vec3 (0.010, 0.024, 0.022);

    float px = fwidth (uv.y);
    float plotL = 0.05, plotR = 0.95, zeroY = 0.56, dbScale = 0.27 / 12.0;

    // Decade grid lines (100 Hz, 1 kHz, 10 kHz) and the 0 dB line
    float grid = 0.0;
    for (int i = 0; i < 3; ++i)
    {
        float gx = plotL + (plotR - plotL) * log (100.0 * pow (10.0, float (i)) / 40.0) / log (400.0);
        grid = max (grid, 1.0 - smoothstep (0.0, fwidth (uv.x) * 1.5, abs (uv.x - gx)));
    }
    grid *= step (0.20, uv.y) * step (uv.y, 0.84) * 0.10;
    float zero = (1.0 - smoothstep (px * 0.5, px * 1.5, abs (uv.y - zeroY))) * step (0.5, fract (uv.x * 48.0)) * 0.25;

    // Catmull-Rom through the 24 band gains
    float t = clamp ((uv.x - plotL) / (plotR - plotL), 0.0, 1.0) * 23.0;
    int k = int (floor (t));
    float f = t - float (k);
    float p0 = uBands[max (k - 1, 0)], p1 = uBands[k], p2 = uBands[min (k + 1, 23)], p3 = uBands[min (k + 2, 23)];
    float gainDb = 0.5 * ((2.0 * p1) + (-p0 + p2) * f + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * f * f + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * f * f * f);
    float curveY = zeroY - gainDb * dbScale;

    float inPlot = step (plotL, uv.x) * step (uv.x, plotR);
    float d = abs (uv.y - curveY);
    float line = (1.0 - smoothstep (px * 1.0, px * 2.6, d)) + exp (-d / 0.025) * 0.30;
    float plotFill = step (min (curveY, zeroY), uv.y) * step (uv.y, max (curveY, zeroY)) * 0.10;

    float text = texture (uTex, uv).r;
    // Faint scanlines, faded out where they would alias
    float scanPhase = uv.y * 170.0;
    float scan = 1.0 - 0.10 * (0.5 + 0.5 * sin (scanPhase * 6.28318)) * clamp (1.0 - fwidth (scanPhase) * 1.5, 0.0, 1.0);
    col += phosphor * ((grid + zero + (line + plotFill) * inPlot) + text) * uParams.y * scan;

    // Footstep detection: soft lime edge glow
    float edge = 1.0 - smoothstep (0.0, 0.06, min (min (uv.x, 1.0 - uv.x), min (uv.y, 1.0 - uv.y)));
    col += vec3 (0.45, 1.0, 0.15) * edge * uParams.w * 0.6;

    col += envColor (R) * (0.03 + 0.25 * pow (facing, 4.0));
    col += vec3 (0.9, 0.95, 1.0) * exp (-pow ((uv.x + uv.y * 0.6 - 0.35) * 6.0, 2.0)) * 0.035;

)GLSL", "uniform float uBands[24];   // live adaptive EQ gains (dB)\n" };

    /** SERAPH live display. uParams = (power, dipsU0, dipsU1, colsU0), uParams2.x = colsU1 */
    inline const hwk::shaders::Material seraphLive { "seraphLive", R"GLSL(
    vec2 uv = vUV;
    float px = fwidth (uv.y), pxu = fwidth (uv.x);
    vec3 violet = vec3 (0.55, 0.36, 1.00), gold = vec3 (1.00, 0.90, 0.66);
    col = vec3 (0.020, 0.010, 0.036);
    float power = uParams.x;
    vec3 light = vec3 (0.0);

    // --- resonance dips curve --------------------------------------------------------------
    float du0 = uParams.y, du1 = uParams.z;
    if (uv.x > du0 && uv.x < du1 && uv.y > 0.18 && uv.y < 0.86)
    {
        float t = (uv.x - du0) / (du1 - du0) * 27.0;
        int k = int (floor (t));
        float f = t - float (k);
        float p0 = uDips[max (k - 1, 0)], p1 = uDips[min (k, 27)], p2 = uDips[min (k + 1, 27)], p3 = uDips[min (k + 2, 27)];
        float dip = 0.5 * ((2.0 * p1) + (-p0 + p2) * f + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * f * f + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * f * f * f);
        float baseY = 0.30, curveY = baseY + clamp (-dip / 12.0, 0.0, 1.1) * 0.50;
        float d = abs (uv.y - curveY);
        float line = (1.0 - smoothstep (px * 0.8, px * 2.4, d)) + exp (-d / 0.03) * 0.25;
        float fill = step (baseY, uv.y) * step (uv.y, curveY) * (0.25 + 0.35 * (uv.y - baseY) / max (0.001, curveY - baseY));
        float grid = (1.0 - smoothstep (0.0, px * 1.5, abs (uv.y - baseY))) * step (0.5, fract (uv.x * 120.0)) * 0.25
                   + (1.0 - smoothstep (0.0, px * 1.5, abs (uv.y - 0.80))) * step (0.5, fract (uv.x * 120.0)) * 0.10;
        light += mix (gold, violet, clamp ((curveY - baseY) * 2.0, 0.0, 1.0)) * (line + fill) + violet * grid;
    }

    // --- per-channel activity columns ------------------------------------------------------
    float cu0 = uParams.w, cu1 = uParams2.x;
    if (uv.x > cu0 && uv.x < cu1 && uv.y > 0.15 && uv.y < 0.80)
    {
        float cu = (uv.x - cu0) / (cu1 - cu0) * 9.0;
        int column = min (int (floor (cu)), 8);
        float fu = fract (cu);
        int side = fu < 0.5 ? 0 : 1;
        float inBar = side == 0 ? step (0.14, fu) * step (fu, 0.44) : step (0.56, fu) * step (fu, 0.86);
        float value = uActivity[column * 2 + side];
        float level = (0.78 - uv.y) / 0.60;                     // 0 at the bottom of a bar, 1 at the top
        float segment = step (fract (level * 14.0), 0.72);        // 14 segments with gaps

        float lit;
        if (column == 5)   // LEVEL: bipolar around the middle
            lit = value >= 0.0 ? step (0.5, level) * step (level, 0.5 + 0.5 * value)
                               : step (0.5 + 0.5 * value, level) * step (level, 0.5);
        else
            lit = step (level, value);

        vec3 segCol = mix (violet, gold, clamp (level, 0.0, 1.0));
        light += segCol * inBar * segment * (0.08 + 1.05 * lit);
        // column separators
        light += violet * 0.12 * (1.0 - smoothstep (0.0, pxu * 1.5, min (fu, 1.0 - fu))) * step (0.5, fract (uv.y * 60.0));
    }

    float text = texture (uTex, uv).r;
    light += mix (violet, vec3 (1.0), 0.65) * text * 0.85;

    col += light * power;
    col += envColor (R) * (0.03 + 0.22 * pow (facing, 4.0));
    col += vec3 (0.8, 0.75, 1.0) * exp (-pow ((uv.x * 0.25 + uv.y * 0.9 - 0.30) * 5.0, 2.0)) * 0.03;

)GLSL", "uniform float uDips[28];      // SMOOTH dip per band (dB, <= 0)\nuniform float uActivity[18];  // per column [col * 2 + ch], 0..1 (LEVEL: -1..1)\n" };

    inline const hwk::shaders::Material& materialFor (int m)
    {
        namespace lib = hwk::shaders::library;
        switch (m)
        {
            case chassis:       return lib::powderCoat;
            case faceplate:     return lib::anodisedPanel;
            case chrome:        return lib::chrome;
            case plastic:       return lib::plastic;
            case table:         return lib::woodTable;
            case emissive:      return lib::emissive;
            case recess:        return lib::recess;
            case print:         return lib::printRing;
            case display:       return enhDisplay;
            case shadow:        return lib::softShadow;
            case paint:         return lib::lacquerPanel;
            case seraphDisplay: return seraphLive;
            case brushed:       return lib::brushedFace;
            case vuFace:        return lib::meterFace;
            case vuGlass:       return lib::coverGlass;
            case callout:       return lib::screenOverlay;
            case glow:          return lib::glowSprite;
            case valueArc:      return lib::valueArc;
            case lens:          return lib::magnifierLens;
            case sunlight:      return lib::windowLight;
            default:            return lib::plastic;
        }
    }
}
