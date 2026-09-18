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

    /*  ENH Master's analyser: the incoming spectrum as a filled curve, what is coming out as a
        line over it, a decaying peak trace, and the EQ the plugin is applying on the same axes,
        so you can see the processing against the material it is reacting to.

        uTex  = the analyser strip (R = input, G = output, B = peak), one texel per point
        uTex2 = the printed graticule (Hz and dB marks, band names)
        uParams  = (_, brightness, _, footstep confidence)
        uParams2 = (spectrum floor dB, spectrum top dB, EQ range dB, scope 1 = have data)
        uBands   = the 24 adaptive EQ gains in dB
    */
    inline const hwk::shaders::Material enhDisplay { "enhDisplay", R"GLSL(
    vec2 uv = vUV;
    col = vec3 (0.008, 0.017, 0.020);

    const float plotL = 0.055, plotR = 0.975, plotT = 0.14, plotB = 0.90;
    float px = fwidth (uv.y), pxx = fwidth (uv.x);
    float inPlot = step (plotL, uv.x) * step (uv.x, plotR) * step (plotT, uv.y) * step (uv.y, plotB);

    // Where this pixel sits in the plot, 0..1 across and down
    float u = clamp ((uv.x - plotL) / (plotR - plotL), 0.0, 1.0);
    float v = clamp ((uv.y - plotT) / (plotB - plotT), 0.0, 1.0);

    // The graticule is printed into uTex2 rather than computed here: it never changes, and a
    // per-fragment loop of log() calls is the one thing this GPU cannot spare.

    // --- the spectrum --------------------------------------------------------------------
    vec3 strip = texture (uTex, vec2 (u, 0.5)).rgb;   // r = input, g = output, b = peak hold
    float inLevel  = strip.r;
    float outLevel = strip.g;
    float peak     = strip.b;

    // The strip is normalised over [minDb, maxDb]; show the useful top of that range
    float floorT = uParams2.x, topT = uParams2.y;
    float inV  = 1.0 - clamp ((inLevel  - floorT) / max (topT - floorT, 0.001), 0.0, 1.0);
    float outV = 1.0 - clamp ((outLevel - floorT) / max (topT - floorT, 0.001), 0.0, 1.0);
    float peakV = 1.0 - clamp ((peak    - floorT) / max (topT - floorT, 0.001), 0.0, 1.0);

    // Input: a filled area, brighter at its edge, the way a good analyser draws it
    float fillIn = step (inV, v) * (0.10 + 0.22 * (1.0 - v));
    float edgeIn = (1.0 - smoothstep (0.0, px * 2.4, abs (v - inV) * (plotB - plotT)));
    col += vec3 (0.16, 0.45, 0.62) * fillIn * inPlot * uParams.y;
    col += vec3 (0.35, 0.78, 0.95) * edgeIn * inPlot * uParams.y * 0.75;

    // Output: a crisp line over it, plus a soft bloom so it reads at a glance
    float dOut = abs (v - outV) * (plotB - plotT);
    float lineOut = (1.0 - smoothstep (px * 0.8, px * 2.4, dOut)) + exp (-dOut / 0.02) * 0.22;
    col += vec3 (0.45, 1.00, 0.80) * lineOut * inPlot * uParams.y;

    // Peak hold: a thin, dimmer trace above it
    float dPeak = abs (v - peakV) * (plotB - plotT);
    col += vec3 (0.85, 0.95, 1.00) * (1.0 - smoothstep (px * 0.6, px * 1.8, dPeak)) * inPlot * uParams.y * 0.35;

    // --- what the EQ is doing, on the same axes ------------------------------------------
    float t = u * 23.0;
    int k = int (floor (t));
    float f = t - float (k);
    float p0 = uBands[max (k - 1, 0)], p1 = uBands[k], p2 = uBands[min (k + 1, 23)], p3 = uBands[min (k + 2, 23)];
    float gainDb = 0.5 * ((2.0 * p1) + (-p0 + p2) * f + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * f * f
                          + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * f * f * f);

    float eqRange = max (uParams2.z, 1.0);
    float eqMid = 0.5;
    float eqV = eqMid - clamp (gainDb / eqRange, -0.5, 0.5) * 0.92;
    float dEq = abs (v - eqV) * (plotB - plotT);

    // The zero line it is drawn against, dashed so it never competes with the spectrum
    float zero = (1.0 - smoothstep (px * 0.5, px * 1.4, abs (v - eqMid) * (plotB - plotT)))
                 * step (0.45, fract (uv.x * 90.0)) * 0.30;
    col += vec3 (0.9, 0.75, 0.35) * zero * inPlot * uParams.y;

    float eqLine = 1.0 - smoothstep (px * 1.0, px * 2.8, dEq);
    float eqFill = step (min (eqV, eqMid), v) * step (v, max (eqV, eqMid)) * 0.13;
    col += vec3 (1.00, 0.76, 0.28) * (eqLine + eqFill) * inPlot * uParams.y;

    // --- print, scanlines, glass ---------------------------------------------------------
    float text = texture (uTex2, uv).r;
    col += vec3 (0.55, 0.95, 1.00) * text * uParams.y * 0.85;

    float scanPhase = uv.y * 190.0;
    float scan = 1.0 - 0.07 * (0.5 + 0.5 * sin (scanPhase * 6.28318)) * clamp (1.0 - fwidth (scanPhase) * 1.5, 0.0, 1.0);
    col *= scan;

    // Footstep detection: a lime edge glow around the whole window
    float edge = 1.0 - smoothstep (0.0, 0.05, min (min (uv.x, 1.0 - uv.x), min (uv.y, 1.0 - uv.y)));
    col += vec3 (0.45, 1.0, 0.15) * edge * uParams.w * 0.55;

    col += envColor (R) * (0.03 + 0.25 * pow (facing, 4.0));
    col += vec3 (0.9, 0.95, 1.0) * exp (-pow ((uv.x + uv.y * 0.6 - 0.35) * 6.0, 2.0)) * 0.035;
)GLSL", "uniform float uBands[24];   // adaptive EQ gains, dB\n" };

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
