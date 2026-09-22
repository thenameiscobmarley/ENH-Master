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
        waveScreen, balancerDisplay, present, wood,
        glassPanel, blurPass, outlineFrame, outlineHull,
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

    // Paper: the meters' cream card - warm, a little uneven, darker toward the corners
    float grainP = valueNoise (uv * vec2 (220.0, 110.0)) * 0.5 + valueNoise (uv * vec2 (560.0, 280.0)) * 0.5;
    vec3 paper = vec3 (0.94, 0.90, 0.78) * (0.95 + 0.08 * grainP);
    paper *= 1.0 - 0.18 * smoothstep (0.45, 1.05, length ((uv - 0.5) * vec2 (1.2, 1.7)));
    const vec3 ink = vec3 (0.08, 0.07, 0.06), pencil = vec3 (0.55, 0.52, 0.46), sepia = vec3 (0.48, 0.30, 0.10), red = vec3 (0.66, 0.13, 0.10);
    col = paper;

    const float plotL = 0.055, plotR = 0.975, plotT = 0.20, plotB = 0.84;
    float px = fwidth (uv.y);
    float inPlot = step (plotL, uv.x) * step (uv.x, plotR) * step (plotT, uv.y) * step (uv.y, plotB);
    float u = clamp ((uv.x - plotL) / (plotR - plotL), 0.0, 1.0);
    float v = clamp ((uv.y - plotT) / (plotB - plotT), 0.0, 1.0);

    vec3 strip = texture (uTex, vec2 (u, 0.5)).rgb;   // r = input, g = output, b = peak hold
    float floorT = uParams2.x, topT = uParams2.y;
    float inV  = 1.0 - clamp ((strip.r - floorT) / max (topT - floorT, 0.001), 0.0, 1.0);
    float outV = 1.0 - clamp ((strip.g - floorT) / max (topT - floorT, 0.001), 0.0, 1.0);
    float peakV = 1.0 - clamp ((strip.b - floorT) / max (topT - floorT, 0.001), 0.0, 1.0);

    // Input: a pencil-shaded area; output: the inked line; peak hold: a faint dotted trace
    col = mix (col, pencil, step (inV, v) * (0.22 + 0.20 * (1.0 - v)) * inPlot);
    float dOut = abs (v - outV) * (plotB - plotT);
    col = mix (col, ink, (1.0 - smoothstep (px * 0.6, px * 1.8, dOut)) * 0.90 * inPlot);
    float dPeak = abs (v - peakV) * (plotB - plotT);
    col = mix (col, ink, (1.0 - smoothstep (px * 0.5, px * 1.4, dPeak)) * step (0.5, fract (uv.x * 160.0)) * 0.35 * inPlot);

    // What the EQ is doing, in sepia ink on its own 0 dB line
    float t = u * 23.0;
    int k = int (floor (t));
    float f = t - float (k);
    float p0 = uBands[max (k - 1, 0)], p1 = uBands[k], p2 = uBands[min (k + 1, 23)], p3 = uBands[min (k + 2, 23)];
    float gainDb = 0.5 * ((2.0 * p1) + (-p0 + p2) * f + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * f * f
                          + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * f * f * f);
    float eqRange = max (uParams2.z, 1.0), eqMid = 0.5;
    float eqV = eqMid - clamp (gainDb / eqRange, -0.5, 0.5) * 0.92;
    float zero = (1.0 - smoothstep (px * 0.5, px * 1.4, abs (v - eqMid) * (plotB - plotT))) * step (0.45, fract (uv.x * 90.0));
    col = mix (col, sepia, zero * 0.45 * inPlot);
    float eqLine = 1.0 - smoothstep (px * 0.8, px * 2.0, abs (v - eqV) * (plotB - plotT));
    float eqFill = step (min (eqV, eqMid), v) * step (v, max (eqV, eqMid));
    col = mix (col, sepia, (eqLine * 0.85 + eqFill * 0.12) * inPlot);

    // SPECTRAL LIMITER: where it is cutting, hanging from the top in red ink
    float lt = u * 47.0;
    int li = int (floor (lt));
    float cut = mix (uLimit[li], uLimit[min (li + 1, 47)], lt - float (li));
    float bb = uParams.z;
    float cutV = clamp ((cut + bb) / 18.0, 0.0, 1.0) * 0.60;
    float inCut = step (v, cutV) * smoothstep (0.2, 0.8, cut + bb);
    float cutEdge = (1.0 - smoothstep (px * 0.8, px * 2.2, abs (v - cutV) * (plotB - plotT))) * smoothstep (0.4, 1.2, cut + bb);
    col = mix (col, red, (inCut * 0.22 + cutEdge * 0.75) * inPlot);

    // The graticule print (uTex2)
    col = mix (col, ink, texture (uTex2, uv).r * 0.85);

    // Lit from behind like the meter faces (brightest near the top), and by the room
    vec3 lit = col * (0.30 + 0.95 * (uParams.y) * (0.72 + 0.28 * (1.0 - uv.y)));
    lit += vec3 (1.0, 0.92, 0.72) * (uParams.y) * 0.08 * (1.0 - smoothstep (0.0, 0.9, length (uv - vec2 (0.5, 0.15))));
    col = lit * (amb * 0.30 + wrap * lightCol * 0.55 + 0.35 + fill);
    col += envColor (R) * (0.02 + 0.14 * pow (facing, 4.0));
)GLSL", "uniform float uBands[24];   // adaptive EQ gains, dB\nuniform float uLimit[48];   // SPECTRAL LIMITER cut, dB\n" };

    inline const hwk::shaders::Material seraphLive { "seraphLive", R"GLSL(
    vec2 uv = vUV;
    float px = fwidth (uv.y), pxu = fwidth (uv.x);
    float power = uParams.x;

    // Paper: the meters' cream card - warm, a little uneven, darker toward the corners
    float grainP = valueNoise (uv * vec2 (220.0, 110.0)) * 0.5 + valueNoise (uv * vec2 (560.0, 280.0)) * 0.5;
    vec3 paper = vec3 (0.94, 0.90, 0.78) * (0.95 + 0.08 * grainP);
    paper *= 1.0 - 0.18 * smoothstep (0.45, 1.05, length ((uv - 0.5) * vec2 (1.2, 1.7)));
    const vec3 ink = vec3 (0.08, 0.07, 0.06), pencil = vec3 (0.55, 0.52, 0.46), sepia = vec3 (0.48, 0.30, 0.10), red = vec3 (0.66, 0.13, 0.10);
    col = paper;

    // SMOOTH's dips: an inked curve over a pencil fill, on a dotted 0 dB line
    float du0 = uParams.y, du1 = uParams.z;
    if (uv.x > du0 && uv.x < du1 && uv.y > 0.17 && uv.y < 0.76)
    {
        float t = (uv.x - du0) / (du1 - du0) * 27.0;
        int k = int (floor (t));
        float f = t - float (k);
        float p0 = uDips[max (k - 1, 0)], p1 = uDips[min (k, 27)], p2 = uDips[min (k + 1, 27)], p3 = uDips[min (k + 2, 27)];
        float dip = 0.5 * ((2.0 * p1) + (-p0 + p2) * f + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * f * f + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * f * f * f);
        float baseY = 0.30, curveY = baseY + clamp (-dip / 12.0, 0.0, 1.08) * 0.42;
        float line = 1.0 - smoothstep (px * 0.7, px * 1.9, abs (uv.y - curveY));
        float fillA = step (baseY, uv.y) * step (uv.y, curveY);
        float grid = (1.0 - smoothstep (0.0, px * 1.3, abs (uv.y - baseY))) * step (0.5, fract (uv.x * 120.0));
        col = mix (col, pencil, fillA * 0.35);
        col = mix (col, ink, grid * 0.30 + line * 0.90 * power);
    }

    // Per-channel activity: printed bar segments, inked where lit
    float cu0 = uParams.w, cu1 = uParams2.x;
    if (uv.x > cu0 && uv.x < cu1 && uv.y > 0.17 && uv.y < 0.73)
    {
        float cu = (uv.x - cu0) / (cu1 - cu0) * 9.0;
        int column = min (int (floor (cu)), 8);
        float fu = fract (cu);
        int side = fu < 0.5 ? 0 : 1;
        float inBar = side == 0 ? step (0.14, fu) * step (fu, 0.44) : step (0.56, fu) * step (fu, 0.86);
        float value = uActivity[column * 2 + side];
        float level = (0.72 - uv.y) / 0.54;
        float segment = step (fract (level * 14.0), 0.72);
        float lit;
        if (column == 5)
            lit = value >= 0.0 ? step (0.5, level) * step (level, 0.5 + 0.5 * value)
                               : step (0.5 + 0.5 * value, level) * step (level, 0.5);
        else
            lit = step (level, value);
        col = mix (col, lit > 0.5 && level > 0.8 ? red : ink, inBar * segment * (0.10 + 0.78 * lit * power));
    }

    col = mix (col, ink, texture (uTex, uv).r * 0.85);

    // Lit from behind like the meter faces (brightest near the top), and by the room
    vec3 lit = col * (0.30 + 0.95 * (power) * (0.72 + 0.28 * (1.0 - uv.y)));
    lit += vec3 (1.0, 0.92, 0.72) * (power) * 0.08 * (1.0 - smoothstep (0.0, 0.9, length (uv - vec2 (0.5, 0.15))));
    col = lit * (amb * 0.30 + wrap * lightCol * 0.55 + 0.35 + fill);
    col += envColor (R) * (0.02 + 0.14 * pow (facing, 4.0));
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

    // Paper: the meters' cream card - warm, a little uneven, darker toward the corners
    float grainP = valueNoise (uv * vec2 (220.0, 110.0)) * 0.5 + valueNoise (uv * vec2 (560.0, 280.0)) * 0.5;
    vec3 paper = vec3 (0.94, 0.90, 0.78) * (0.95 + 0.08 * grainP);
    paper *= 1.0 - 0.18 * smoothstep (0.45, 1.05, length ((uv - 0.5) * vec2 (1.2, 1.7)));
    const vec3 ink = vec3 (0.08, 0.07, 0.06), pencil = vec3 (0.55, 0.52, 0.46), sepia = vec3 (0.48, 0.30, 0.10), red = vec3 (0.66, 0.13, 0.10);
    col = paper;

    // --- Top: the waveform, the input in pencil behind the output in ink. Where the rack takes
    // something down, the pencil shows past the ink; a faded afterimage of the output stays behind.
    vec4 w = texture (uTex, vec2 (uv.x, 0.25));            // R out, G out's afterimage, B in
    const float wc = 0.27, wh = 0.20;
    float h = abs (uv.y - wc) / wh;
    float edge = px * 1.6 / wh;
    float inBand = (1.0 - smoothstep (w.b - edge, w.b, h)) * step (abs (uv.y - wc), wh + px);
    float ghost = (1.0 - smoothstep (w.g - edge, w.g, h)) * step (abs (uv.y - wc), wh + px);
    float outBand = (1.0 - smoothstep (w.r - edge, w.r, h)) * step (abs (uv.y - wc), wh + px);
    float centre = (1.0 - smoothstep (0.0, px * 1.2, abs (uv.y - wc))) * 0.45;
    for (int k = 0; k < 3; ++k)
    {
        float lineH = k == 0 ? 1.0 : k == 1 ? 0.5012 : 0.2512;   // 0, -6, -12 dB
        centre += (1.0 - smoothstep (0.0, px * 1.1, abs (h - lineH) * wh)) * step (0.5, fract (uv.x * 90.0)) * 0.20 * step (abs (uv.y - wc), wh + px);
    }
    col = mix (col, ink, centre * 0.6);
    col = mix (col, pencil, inBand * 0.55);
    col = mix (col, mix (col, ink, 0.30), ghost);
    col = mix (col, ink, outBand * 0.92);

    // --- Bottom: the spectrum in (pencil fill) and out (ink), and in red what the rack is doing to
    // the tone (out minus in, +-12 dB about its own dotted line)
    const float sT = 0.55, sB = 0.90;
    float inSpec = step (sT, uv.y) * step (uv.y, sB) * step (0.02, uv.x) * step (uv.x, 0.98);
    float su = clamp ((uv.x - 0.02) / 0.96, 0.0, 1.0);
    vec4 s = texture (uTex, vec2 (su, 0.75));               // R in, G out (0..1 over -90..0 dB)
    float sv = (uv.y - sT) / (sB - sT);
    col = mix (col, pencil, step (1.0 - s.r, sv) * (0.25 + 0.20 * (1.0 - sv)) * inSpec);
    col = mix (col, ink, (1.0 - smoothstep (px * 0.6, px * 1.7, abs (sv - (1.0 - s.g)) * (sB - sT))) * 0.85 * inSpec);
    float diffDb = (s.g - s.r) * 90.0;
    float midY = 0.5 * (sT + sB);
    float diffY = midY - clamp (diffDb / 12.0, -1.0, 1.0) * 0.5 * (sB - sT) * 0.9;
    float quiet = smoothstep (0.08, 0.18, max (s.r, s.g));   // no difference drawn where there is nothing
    col = mix (col, ink, (1.0 - smoothstep (0.0, px * 1.2, abs (uv.y - midY))) * step (0.5, fract (uv.x * 110.0)) * 0.35 * inSpec);
    col = mix (col, red, (1.0 - smoothstep (px * 0.8, px * 2.0, abs (uv.y - diffY))) * 0.90 * inSpec * quiet * lamp);
    col = mix (col, ink, (1.0 - smoothstep (0.0, px * 1.2, abs (uv.y - 0.515))) * 0.40);

    col = mix (col, ink, texture (uTex2, uv).r * 0.88);

    // Lit from behind like the meter faces (brightest near the top), and by the room
    vec3 lit = col * (0.30 + 0.95 * (lamp) * (0.72 + 0.28 * (1.0 - uv.y)));
    lit += vec3 (1.0, 0.92, 0.72) * (lamp) * 0.08 * (1.0 - smoothstep (0.0, 0.9, length (uv - vec2 (0.5, 0.15))));
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

    // Paper: the meters' cream card - warm, a little uneven, darker toward the corners
    float grainP = valueNoise (uv * vec2 (220.0, 110.0)) * 0.5 + valueNoise (uv * vec2 (560.0, 280.0)) * 0.5;
    vec3 paper = vec3 (0.94, 0.90, 0.78) * (0.95 + 0.08 * grainP);
    paper *= 1.0 - 0.18 * smoothstep (0.45, 1.05, length ((uv - 0.5) * vec2 (1.2, 1.7)));
    const vec3 ink = vec3 (0.08, 0.07, 0.06), pencil = vec3 (0.55, 0.52, 0.46), sepia = vec3 (0.48, 0.30, 0.10), red = vec3 (0.66, 0.13, 0.10);
    col = paper;

    const float plotL = 0.02, plotR = 0.98, plotB = 0.64;
    float u = clamp ((uv.x - plotL) / (plotR - plotL), 0.0, 1.0);
    float inPlot = step (plotL, uv.x) * step (uv.x, plotR) * step (uv.y, plotB);
    vec4 curveRow = texture (uTex, vec2 (u, 0.875));
    vec4 histRow  = texture (uTex, vec2 (u, 0.625));
    vec4 specRow  = texture (uTex, vec2 (u, 0.375));
    vec4 gridRow  = texture (uTex, vec2 (u, 0.125));

    // Grid, printed faintly
    float grid = gridRow.r * 0.30 + gridRow.g * 0.14;
    for (int k = -2; k <= 2; ++k)
        grid += (1.0 - smoothstep (0.0, px * 1.2, abs (uv.y - (0.34 - float (k) * 0.14)))) * (k == 0 ? 0.35 : 0.14);
    col = mix (col, ink, grid * 0.5 * inPlot);

    // Spectrum in (pencil) and out (ink)
    float v = uv.y / plotB;
    col = mix (col, pencil, step (1.0 - specRow.r, v) * (0.22 + 0.18 * (1.0 - v)) * inPlot);
    col = mix (col, ink, (1.0 - smoothstep (px * 0.6, px * 1.7, abs (v - (1.0 - specRow.g)) * plotB)) * 0.80 * inPlot);

    // The faders' curve in sepia, each band's region lightly tinted toward 0 dB
    float gainDb = (curveRow.r - 0.5) * 24.0;
    float zeroY = 0.34;
    float curveY = zeroY - clamp (gainDb / 12.0, -1.0, 1.0) * 0.28;
    float between = step (min (curveY, zeroY), uv.y) * step (uv.y, max (curveY, zeroY));
    col = mix (col, curveRow.gba * 0.75, between * 0.22 * inPlot);
    col = mix (col, sepia, (1.0 - smoothstep (px * 0.8, px * 2.1, abs (uv.y - curveY))) * 0.95 * inPlot * power);

    for (int k = 0; k < 6; ++k)
    {
        float hy = zeroY - clamp (uBands[k] / 12.0, -1.0, 1.0) * 0.28;
        float r = length (vec2 ((uv.x - uHandleU[k]) / pxu, (uv.y - hy) / px));
        col = mix (col, ink, (1.0 - smoothstep (5.5, 7.0, r)) * smoothstep (3.5, 4.5, r) * power * uParams.y);   // an inked ring (fades in spectral mode)
    }

    // History: in (pencil), out (ink), cut hanging from the top (red)
    const float histT = 0.72, histB = 0.985;
    float inHist = step (histT, uv.y) * step (uv.y, histB) * step (plotL, uv.x) * step (uv.x, plotR);
    float hv = (uv.y - histT) / (histB - histT);
    col = mix (col, pencil, step (1.0 - histRow.r, hv) * 0.40 * inHist);
    col = mix (col, ink, (1.0 - smoothstep (px * 0.6, px * 1.7, abs (hv - (1.0 - histRow.g)) * (histB - histT))) * 0.85 * inHist);
    col = mix (col, red, step (hv, histRow.b) * 0.40 * inHist);
    col = mix (col, ink, (1.0 - smoothstep (0.0, px * 1.2, abs (uv.y - histT))) * 0.35);

    col = mix (col, ink, texture (uTex2, uv).r * 0.85);

    // Lit from behind like the meter faces (brightest near the top), and by the room
    vec3 lit = col * (0.30 + 0.95 * (power) * (0.72 + 0.28 * (1.0 - uv.y)));
    lit += vec3 (1.0, 0.92, 0.72) * (power) * 0.08 * (1.0 - smoothstep (0.0, 0.9, length (uv - vec2 (0.5, 0.15))));
    col = lit * (amb * 0.30 + wrap * lightCol * 0.55 + 0.35 + fill);
    col += envColor (R) * (0.02 + 0.14 * pow (facing, 4.0));
)GLSL", "uniform float uBands[6];   // MIX BALANCER faders, dB\nuniform float uHandleU[6]; // where each band's handle sits across the display\n" };

    /*  The finished frame, from the plugin's own multisampled (and, on small windows, supersampled)
        buffer to the screen: four bilinear taps per screen pixel spread over the pixel's footprint in
        the source, so a supersampled frame comes down clean rather than shimmering.
        uParams = (tap offset x, tap offset y in source uv), uParams2 = (1 / screen width, 1 / screen height) */
    inline const hwk::shaders::Material presentMaterial { "present", R"GLSL(
    vec2 uv = gl_FragCoord.xy * uParams2.xy;
    vec2 t = uParams.xy;
    col = 0.25 * (texture (uTex, uv + vec2 (-t.x, -t.y)).rgb + texture (uTex, uv + vec2 (t.x, -t.y)).rgb
                + texture (uTex, uv + vec2 (-t.x, t.y)).rgb + texture (uTex, uv + vec2 (t.x, t.y)).rgb);
)GLSL", "", false };

    /*  The glass panel (GlassPanel.h), one quad over the panel plus a margin for its shadow. Frosted
        glass and nothing else: the scene behind it blurred and lifted a little toward white, a hard-
        cornered edge that catches the light (brightest along the top), a soft shadow falling under it,
        and the panel's white print on top. No tint. Everything is in screen pixels about its centre.
        vLocal.xz  -1..1 over the quad; uParams  = (panel half width px, half height px, px per logical px, opacity)
        uParams2   = (quad half width px, quad half height px, have blur 0/1, _)
        uTex = blurred scene (screen uv); uTex2 = the print (panel uv); uEmissive = (1 / screen w, 1 / screen h, _) */
    inline const hwk::shaders::Material glassPanelMaterial { "glassPanel", R"GLSL(
    vec2 p = vLocal.xz * uParams2.xy;                 // px from the panel's centre, y down
    vec2 half_ = uParams.xy;
    float s = uParams.z;
    vec2 q = abs (p) - half_;
    float d = max (q.x, q.y);                          // hard rectangle: square corners
    float inside = 1.0 - smoothstep (-0.5, 0.5, d);

    // Frost: the blurred scene, a little brighter and lifted toward white
    vec2 screenUv = gl_FragCoord.xy * uEmissive.xy;
    vec3 behind = uParams2.z > 0.5 ? texture (uTex, screenUv).rgb : vec3 (0.10);
    float t = clamp ((p.y + half_.y) / (2.0 * half_.y), 0.0, 1.0);   // 0 top .. 1 bottom
    vec3 glassCol = mix (behind * 1.08, vec3 (1.0), mix (0.16, 0.10, t));   // a touch more light at the top

    // The edge: a crisp 1 px line of light just inside, brightest along the top
    float fromEdge = -d;                                                // px inside
    float edgeLine = (1.0 - smoothstep (0.0, 1.2 * s, fromEdge)) * inside;
    float topLight = 1.0 - smoothstep (0.0, 1.5 * s, p.y + half_.y);  // the top edge
    glassCol = mix (glassCol, vec3 (1.0), edgeLine * mix (0.30, 0.65, topLight));

    // The print
    vec2 puv = (p + half_) / (2.0 * half_);
    vec4 ink = texture (uTex2, puv);
    glassCol = mix (glassCol, ink.rgb, ink.a);

    // Soft shadow under it, offset down, outside the glass only
    vec2 sq = abs (p - vec2 (0.0, 7.0 * s)) - half_;
    float sd = length (max (sq, 0.0)) + min (max (sq.x, sq.y), 0.0);
    float shadow = exp (-max (sd, 0.0) / (14.0 * s)) * 0.45 * (1.0 - inside);

    col = inside > 0.001 ? glassCol : vec3 (0.0);
    alpha = max (inside, shadow) * uParams.w;
)GLSL", "", false };

    /*  One pass of the panel's blur, at a quarter of the screen's resolution: a 9-tap Gaussian along
        uParams.xy (source texel steps); with uParams.z > 0.5 it is instead the 4-tap box that brings the
        full-resolution scene down. uParams2.xy = 1 / target size. */
    inline const hwk::shaders::Material blurPassMaterial { "blurPass", R"GLSL(
    vec2 uv = gl_FragCoord.xy * uParams2.xy;
    vec2 s = uParams.xy;
    if (uParams.z > 0.5)
        col = 0.25 * (texture (uTex, uv + vec2 (-s.x, -s.y)).rgb + texture (uTex, uv + vec2 (s.x, -s.y)).rgb
                    + texture (uTex, uv + vec2 (-s.x, s.y)).rgb + texture (uTex, uv + vec2 (s.x, s.y)).rgb);
    else
        col = texture (uTex, uv).rgb * 0.227
            + (texture (uTex, uv + s * 1.385).rgb + texture (uTex, uv - s * 1.385).rgb) * 0.316
            + (texture (uTex, uv + s * 3.231).rgb + texture (uTex, uv - s * 3.231).rgb) * 0.070;
    alpha = 1.0;
)GLSL", "", false };

    /*  Hover outline for a unit: one quad over its faceplate, a crisp line on the silhouette and a soft
        glow inside it, whatever the distance (widths in screen pixels via fwidth).
        uParams = (half width, half height of the quad in local units, line px, opacity), uBaseColor. */
    inline const hwk::shaders::Material outlineFrameMaterial { "outlineFrame", R"GLSL(
    vec2 q = (1.0 - abs (vLocal.xz)) * uParams.xy;      // local distance to each edge
    float edge = min (q.x, q.y);
    float px = max (fwidth (edge), 1.0e-6);
    float e = edge / px;                                 // in screen pixels
    float line = 1.0 - smoothstep (uParams.z, uParams.z + 1.0, e);
    float glowIn = exp (-e / 9.0) * 0.30;
    col = uBaseColor;
    alpha = (line + glowIn) * uParams.w;
)GLSL", "", false };

    /*  Hover outline for a control: its own mesh again, scaled up a little about its axis and drawn with
        front faces culled, so only the rim that sticks out past the silhouette shows (the inverted hull).
        Flat colour. uParams.w = opacity. */
    inline const hwk::shaders::Material outlineHullMaterial { "outlineHull", R"GLSL(
    col = uBaseColor;
    alpha = uParams.w;
)GLSL", "", false };

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
            case present:       return presentMaterial;
            case wood:          return lib::walnut;
            case glassPanel:    return glassPanelMaterial;
            case blurPass:      return blurPassMaterial;
            case outlineFrame:  return outlineFrameMaterial;
            case outlineHull:   return outlineHullMaterial;
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
