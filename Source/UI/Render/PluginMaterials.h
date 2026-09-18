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
        waveScreen, balancerDisplay,
        numMaterials
    };

    /*  ENH Master's analyser: the incoming spectrum as a filled curve, what is coming out as a
        line over it, a decaying peak trace, and the EQ the plugin is applying on the same axes,
        so you can see the processing against the material it is reacting to.

        uTex  = the analyser strip (R = input, G = output, B = peak), one texel per point
        uTex2 = the printed graticule (Hz and dB marks, band names)
        uParams  = (_, brightness, SPECTRAL LIMITER broadband protection dB, footstep confidence)
        uParams2 = (spectrum floor dB, spectrum top dB, EQ range dB, scope 1 = have data)
        uBands   = the 24 adaptive EQ gains in dB
        uLimit   = the SPECTRAL LIMITER's cut (dB >= 0) at 48 points, 20 Hz - 20 kHz on the graticule's
                   log axis: drawn hanging from the top of the plot, where and as deep as it is cutting

        The plot rectangle (0.055 .. 0.975 across, 0.20 .. 0.84 down) must match renderDisplayOverlay.
    */
    inline const hwk::shaders::Material enhDisplay { "enhDisplay", R"GLSL(
    vec2 uv = vUV;
    col = vec3 (0.008, 0.017, 0.020);

    const float plotL = 0.055, plotR = 0.975, plotT = 0.20, plotB = 0.84;
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
    // One pale phosphor, as on a hardware analyser: input a dim fill, output the bright line
    const vec3 phosphor = vec3 (0.62, 0.86, 0.80);
    float fillIn = step (inV, v) * (0.05 + 0.10 * (1.0 - v));
    float edgeIn = (1.0 - smoothstep (0.0, px * 2.0, abs (v - inV) * (plotB - plotT)));
    col += phosphor * (fillIn + edgeIn * 0.30) * inPlot * uParams.y;

    float dOut = abs (v - outV) * (plotB - plotT);
    float lineOut = 1.0 - smoothstep (px * 0.7, px * 2.0, dOut);
    col += phosphor * lineOut * 0.85 * inPlot * uParams.y;

    // Peak hold: a thin, dim trace above it
    float dPeak = abs (v - peakV) * (plotB - plotT);
    col += phosphor * (1.0 - smoothstep (px * 0.6, px * 1.6, dPeak)) * inPlot * uParams.y * 0.22;

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
    const vec3 eqInk = vec3 (0.86, 0.68, 0.36);
    col += eqInk * zero * 0.7 * inPlot * uParams.y;

    float eqLine = 1.0 - smoothstep (px * 0.8, px * 2.2, dEq);
    float eqFill = step (min (eqV, eqMid), v) * step (v, max (eqV, eqMid)) * 0.06;
    col += eqInk * (eqLine * 0.75 + eqFill) * inPlot * uParams.y;

    // --- SPECTRAL LIMITER: where it is cutting, hanging from the top like gain reduction ----
    float lt = u * 47.0;
    int li = int (floor (lt));
    float cut = mix (uLimit[li], uLimit[min (li + 1, 47)], lt - float (li));
    float bb = uParams.z;
    float cutV = clamp ((cut + bb) / 18.0, 0.0, 1.0) * 0.60;     // 18 dB reaches 60 % down the plot
    float bbV = clamp (bb / 18.0, 0.0, 1.0) * 0.60;
    float inCut = step (v, cutV) * smoothstep (0.2, 0.8, cut + bb);
    float cutEdge = (1.0 - smoothstep (px * 0.8, px * 2.4, abs (v - cutV) * (plotB - plotT))) * smoothstep (0.4, 1.2, cut + bb);
    float shade = 0.10 + 0.22 * (1.0 - v / max (cutV, 0.001));
    vec3 cutCol = mix (vec3 (0.80, 0.30, 0.26), vec3 (0.86, 0.56, 0.30), step (v, bbV));   // muted red: spectral, amber: broadband
    col += cutCol * (inCut * shade * 0.7 + cutEdge * 0.6) * inPlot * uParams.y;

    // --- print, scanlines, glass ---------------------------------------------------------
    float text = texture (uTex2, uv).r;
    col += vec3 (0.66, 0.80, 0.78) * text * uParams.y * 0.70;

    float scanPhase = uv.y * 190.0;
    float scan = 1.0 - 0.04 * (0.5 + 0.5 * sin (scanPhase * 6.28318)) * clamp (1.0 - fwidth (scanPhase) * 1.5, 0.0, 1.0);
    col *= scan;

    col += envColor (R) * (0.03 + 0.25 * pow (facing, 4.0));
    col += vec3 (0.9, 0.95, 1.0) * exp (-pow ((uv.x + uv.y * 0.6 - 0.35) * 6.0, 2.0)) * 0.035;
)GLSL", "uniform float uBands[24];   // adaptive EQ gains, dB\nuniform float uLimit[48];   // SPECTRAL LIMITER cut, dB\n" };

    inline const hwk::shaders::Material seraphLive { "seraphLive", R"GLSL(
    vec2 uv = vUV;
    float px = fwidth (uv.y), pxu = fwidth (uv.x);
    vec3 violet = vec3 (0.66, 0.60, 0.84), gold = violet;   // one ink, as a single-colour display has
    col = vec3 (0.020, 0.010, 0.036);
    float power = uParams.x;
    vec3 light = vec3 (0.0);

    // --- resonance dips curve --------------------------------------------------------------
    float du0 = uParams.y, du1 = uParams.z;
    if (uv.x > du0 && uv.x < du1 && uv.y > 0.17 && uv.y < 0.76)
    {
        float t = (uv.x - du0) / (du1 - du0) * 27.0;
        int k = int (floor (t));
        float f = t - float (k);
        float p0 = uDips[max (k - 1, 0)], p1 = uDips[min (k, 27)], p2 = uDips[min (k + 1, 27)], p3 = uDips[min (k + 2, 27)];
        float dip = 0.5 * ((2.0 * p1) + (-p0 + p2) * f + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * f * f + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * f * f * f);
        float baseY = 0.30, curveY = baseY + clamp (-dip / 12.0, 0.0, 1.08) * 0.42;   // -12 dB at 0.72
        float d = abs (uv.y - curveY);
        float line = 1.0 - smoothstep (px * 0.7, px * 2.0, d);
        float fill = step (baseY, uv.y) * step (uv.y, curveY) * (0.10 + 0.18 * (uv.y - baseY) / max (0.001, curveY - baseY));
        float grid = (1.0 - smoothstep (0.0, px * 1.5, abs (uv.y - baseY))) * step (0.5, fract (uv.x * 120.0)) * 0.25
                   + (1.0 - smoothstep (0.0, px * 1.5, abs (uv.y - 0.72))) * step (0.5, fract (uv.x * 120.0)) * 0.10;
        light += mix (gold, violet, clamp ((curveY - baseY) * 2.0, 0.0, 1.0)) * (line + fill) + violet * grid;
    }

    // --- per-channel activity columns ------------------------------------------------------
    float cu0 = uParams.w, cu1 = uParams2.x;
    if (uv.x > cu0 && uv.x < cu1 && uv.y > 0.17 && uv.y < 0.73)
    {
        float cu = (uv.x - cu0) / (cu1 - cu0) * 9.0;
        int column = min (int (floor (cu)), 8);
        float fu = fract (cu);
        int side = fu < 0.5 ? 0 : 1;
        float inBar = side == 0 ? step (0.14, fu) * step (fu, 0.44) : step (0.56, fu) * step (fu, 0.86);
        float value = uActivity[column * 2 + side];
        float level = (0.72 - uv.y) / 0.54;                     // 0 at the bottom of a bar (0.72), 1 at the top (0.18)
        float segment = step (fract (level * 14.0), 0.72);        // 14 segments with gaps

        float lit;
        if (column == 5)   // LEVEL: bipolar around the middle
            lit = value >= 0.0 ? step (0.5, level) * step (level, 0.5 + 0.5 * value)
                               : step (0.5 + 0.5 * value, level) * step (level, 0.5);
        else
            lit = step (level, value);

        vec3 segCol = mix (violet, gold, clamp (level, 0.0, 1.0));
        light += segCol * inBar * segment * (0.06 + 0.80 * lit);
        // column separators
        light += violet * 0.12 * (1.0 - smoothstep (0.0, pxu * 1.5, min (fu, 1.0 - fu))) * step (0.5, fract (uv.y * 60.0));
    }

    float text = texture (uTex, uv).r;
    light += mix (violet, vec3 (1.0), 0.40) * text * 0.70;

    col += light * power;
    col += envColor (R) * (0.03 + 0.22 * pow (facing, 4.0));
    col += vec3 (0.8, 0.75, 1.0) * exp (-pow ((uv.x * 0.25 + uv.y * 0.9 - 0.30) * 5.0, 2.0)) * 0.03;

)GLSL", "uniform float uDips[28];      // SMOOTH dip per band (dB, <= 0)\nuniform float uActivity[18];  // per column [col * 2 + ch], 0..1 (LEVEL: -1..1)\n" };


    /*  LEVEL & LOUDNESS: the waveform screen, printed and lit like the meter faces - cream card,
        black ink. The rack's output scrolls right to left as a black band either side of the centre
        line (linear amplitude, full scale at 90 % of the card; the live readout sits in the strip below); where something loud went past, a faded afterimage of it
        stays on the card and slowly fades, the way a phosphor holds a trace.

        uTex  = wave strip (R = the band now, G = its afterimage), one texel per column, screen order
        uTex2 = the print on the card (dBFS marks, the readout line)
        uParams = (backlight 0..1, -, -, -)
    */
    inline const hwk::shaders::Material waveScreenMaterial { "waveScreen", R"GLSL(
    vec2 uv = vUV;
    float px = fwidth (uv.y);
    float lamp = uParams.x;
    vec3 ink  = vec3 (0.075, 0.065, 0.055);

    // The same card as the meter faces: warm, slightly uneven, darker toward its corners
    float grain = valueNoise (uv * vec2 (180.0, 90.0)) * 0.5 + valueNoise (uv * vec2 (480.0, 240.0)) * 0.5;
    vec3 card = vec3 (0.94, 0.90, 0.78) * (0.94 + 0.10 * grain);
    card *= 1.0 - 0.20 * smoothstep (0.45, 1.05, length ((uv - 0.5) * vec2 (1.2, 1.7)));
    col = card;

    // Scale: the centre line and the dB lines, printed faintly
    float h = abs (uv.y - 0.5) / 0.40;                       // 0 at the centre line, 1 at 0 dBFS
    float grid = (1.0 - smoothstep (0.0, px * 1.2, abs (uv.y - 0.5))) * 0.55;
    for (int k = 0; k < 4; ++k)
    {
        float lineH = k == 0 ? 1.0 : k == 1 ? 0.7079 : k == 2 ? 0.5012 : 0.2512;   // 0, -3, -6, -12 dB (linear)
        float d = abs (h - lineH) * 0.40;
        grid += (1.0 - smoothstep (0.0, px * 1.1, d)) * step (0.5, fract (uv.x * 70.0)) * 0.22;
    }

    // The waveform now, and its afterimage
    vec2 w = texture (uTex, vec2 (uv.x, 0.5)).rg;
    float edge = px * 1.6 / 0.40;
    float now = 1.0 - smoothstep (w.r - edge, w.r, h);
    float ghost = 1.0 - smoothstep (w.g - edge, w.g, h);

    col = mix (col, ink, grid * 0.35);
    col = mix (col, mix (col, ink, 0.32), ghost);
    col = mix (col, ink, now * 0.94);
    col = mix (col, ink, texture (uTex2, uv).r * 0.88);

    // Lit from behind like the meters, brightest near the top, and by the room
    vec3 lit = col * (0.30 + 0.95 * lamp * (0.72 + 0.28 * (1.0 - uv.y)));
    lit += vec3 (1.0, 0.92, 0.72) * lamp * 0.08 * (1.0 - smoothstep (0.0, 0.9, length (uv - vec2 (0.5, 0.15))));
    col = lit * (amb * 0.30 + wrap * lightCol * 0.55 + 0.35 + fill);
    col += envColor (R) * (0.02 + 0.14 * pow (facing, 4.0));
)GLSL" };

    /*  MIX BALANCER: a FabFilter-style dynamics display.

        Top (to 64 %): the spectrum going in (a soft filled area) and coming out (a bright line), and on
        the same axes the balancer's six moving faders as the curve they make (gold), filled toward 0 dB
        in each band's colour, with a handle on each band. Bottom (from 72 %): the last ten seconds
        scrolling right to left, Pro-C style - the level going in (grey), coming out (white) and the
        deepest cut hanging from the top (red).

        uTex  = data, four rows (one texel per column, worked out on the CPU so a pixel only looks up):
                grid (R decade line, G octave line), spectrum (R in, G out, 0..1 over -90..0 dB),
                history (R in, G out over -60..0 dB, B cut over 0..12 dB),
                the faders' curve (R = dB, 0.5 = 0 dB, +-12 at the ends; GBA = the band's colour)
        uTex2 = the print (frequency and dB axes)
        uParams = (power 0..1, -, -, -);  uBands = the six faders (dB)
    */
    inline const hwk::shaders::Material balancerMaterial { "balancerDisplay", R"GLSL(
    vec2 uv = vUV;
    float px = fwidth (uv.y), pxu = fwidth (uv.x);
    float power = uParams.x;

    vec3 bgTop = vec3 (0.105, 0.118, 0.145), bgBottom = vec3 (0.040, 0.045, 0.058);
    col = mix (bgTop, bgBottom, smoothstep (0.0, 1.0, uv.y));

    const float plotL = 0.02, plotR = 0.98, plotB = 0.64;
    float u = clamp ((uv.x - plotL) / (plotR - plotL), 0.0, 1.0);
    float inPlot = step (plotL, uv.x) * step (uv.x, plotR) * step (uv.y, plotB);

    // Per column, worked out on the CPU (row 3): the faders' curve (R, dB) and the band colour (GBA),
    // and whether a grid line runs down this column (row 2's A)
    vec4 curveRow = texture (uTex, vec2 (u, 0.875));
    vec4 histRow  = texture (uTex, vec2 (u, 0.625));
    vec4 specRow  = texture (uTex, vec2 (u, 0.375));
    vec4 gridRow  = texture (uTex, vec2 (u, 0.125));

    // Grid: frequency lines (from the grid row) and dB lines every 6
    float grid = gridRow.r * 0.16 + gridRow.g * 0.07;
    for (int k = -2; k <= 2; ++k)
    {
        float gy = 0.34 - float (k) * 0.14;
        grid += (1.0 - smoothstep (0.0, px * 1.2, abs (uv.y - gy))) * (k == 0 ? 0.20 : 0.08);
    }
    col += vec3 (0.55, 0.62, 0.75) * grid * inPlot;

    // Spectrum in (filled) and out (line)
    float v = uv.y / plotB;
    float inV = 1.0 - specRow.r, outV = 1.0 - specRow.g;
    col += vec3 (0.20, 0.30, 0.46) * step (inV, v) * (0.30 + 0.25 * (1.0 - v)) * inPlot * power;
    float dOut = abs (v - outV) * plotB;
    col += vec3 (0.78, 0.86, 1.00) * (1.0 - smoothstep (px * 0.7, px * 1.9, dOut)) * inPlot * power * 0.85;

    // The faders' curve, filled toward 0 dB in the band's colour
    float gainDb = (curveRow.r - 0.5) * 24.0;
    float zeroY = 0.34;
    float curveY = zeroY - clamp (gainDb / 12.0, -1.0, 1.0) * 0.28;
    float between = step (min (curveY, zeroY), uv.y) * step (uv.y, max (curveY, zeroY));
    col += curveRow.gba * between * 0.30 * inPlot * power;
    col += vec3 (1.00, 0.80, 0.35) * (1.0 - smoothstep (px * 0.8, px * 2.2, abs (uv.y - curveY))) * inPlot * power;

    // A handle on each band, where its fader stands (positions from uniforms)
    for (int k = 0; k < 6; ++k)
    {
        float hy = zeroY - clamp (uBands[k] / 12.0, -1.0, 1.0) * 0.28;
        vec2 dd = vec2 ((uv.x - uHandleU[k]) / pxu, (uv.y - hy) / px);
        float r = length (dd);
        col += vec3 (1.0, 0.92, 0.75) * (1.0 - smoothstep (4.0, 5.5, r)) * power;
        col += vec3 (0.9, 0.7, 0.3) * (1.0 - smoothstep (7.0, 9.0, r)) * smoothstep (5.5, 7.0, r) * 0.6 * power;
    }

    // History strip: in (grey fill), out (white line), cut hanging from the top (red)
    const float histT = 0.72, histB = 0.985;
    float inHist = step (histT, uv.y) * step (uv.y, histB) * step (plotL, uv.x) * step (uv.x, plotR);
    float hv = (uv.y - histT) / (histB - histT);
    col += vec3 (0.30, 0.34, 0.40) * step (1.0 - histRow.r, hv) * 0.55 * inHist * power;
    float dLine = abs (hv - (1.0 - histRow.g)) * (histB - histT);
    col += vec3 (0.92, 0.95, 1.00) * (1.0 - smoothstep (px * 0.7, px * 1.8, dLine)) * inHist * power * 0.9;
    col += vec3 (0.95, 0.25, 0.22) * step (hv, histRow.b) * 0.65 * inHist * power;
    col += vec3 (0.55, 0.62, 0.75) * (1.0 - smoothstep (0.0, px * 1.2, abs (uv.y - histT))) * 0.25;

    col += vec3 (0.80, 0.85, 0.95) * texture (uTex2, uv).r * 0.70;
    col += envColor (R) * (0.03 + 0.22 * pow (facing, 4.0));
)GLSL", "uniform float uBands[6];   // MIX BALANCER faders, dB\nuniform float uHandleU[6]; // where each band's handle sits across the display\n" };

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
            case waveScreen:    return waveScreenMaterial;
            case balancerDisplay: return balancerMaterial;
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
