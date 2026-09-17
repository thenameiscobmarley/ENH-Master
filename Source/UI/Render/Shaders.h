#pragma once

/*  One Blinn-Phong/Fresnel uber-shader with a material switch.
    Cheap enough for Intel UHD 600: no shadow maps, no post-processing,
    all shadows are analytic SDF decals, all glow is additive in-shader.
*/
namespace pad::shaders
{
    enum Material
    {
        satinChassis = 0,
        pearlPanel   = 1,
        metal        = 2,
        plastic      = 3,
        table        = 4,
        emissive     = 5,
        recess       = 6,
        knobFlange   = 7,
        scopeGlass   = 8,
        ledRing      = 9,
        softShadow   = 10
    };

    inline constexpr const char* vertex = R"GLSL(
#version 150
in vec3 aPos;
in vec3 aNormal;
in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uViewProj;

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out vec3 vLocal;

void main()
{
    vec4 world = uModel * vec4 (aPos, 1.0);
    vWorld  = world.xyz;
    vNormal = mat3 (uModel) * aNormal;
    vUV     = aUV;
    vLocal  = aPos;
    gl_Position = uViewProj * world;
}
)GLSL";

    inline constexpr const char* fragment = R"GLSL(
#version 150
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec3 vLocal;

out vec4 fragColor;

uniform vec3  uCamPos;
uniform vec3  uLightDir;
uniform float uTime;
uniform vec2  uViewport;

uniform int   uMaterial;
uniform vec3  uBaseColor;
uniform vec3  uEmissive;

uniform sampler2D uDecal;
uniform sampler2D uDial;
uniform sampler2D uOverlay;

uniform vec4  uPanelRect;   // minX, minZ, sizeX, sizeZ
uniform vec4  uVentRect;    // cx, cz, hw, hd
uniform vec3  uVentGlow;
uniform vec3  uGlyphL;
uniform vec3  uGlyphR;
uniform vec2  uSheen;       // phase, strength
uniform vec4  uRing;        // valueAngle, activity, hover, dialRadius
uniform vec3  uRingColor;
uniform vec4  uScope;       // sweepHead, brightness, aspect, unused
uniform vec2  uRecess;      // topY, depth
uniform vec4  uShadow;      // cx, cz, halfW, halfD
uniform vec3  uShadowParams;// cornerRadius, blur, strength

const vec3 skyCol    = vec3 (0.66, 0.63, 0.70);
const vec3 groundCol = vec3 (0.17, 0.11, 0.12);
const float PI = 3.14159265;

vec3 envColor (vec3 r)
{
    float t = clamp (r.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 c = mix (groundCol, skyCol, smoothstep (0.35, 0.8, t));
    c += vec3 (1.0, 0.96, 0.98) * pow (max (dot (r, normalize (vec3 (-0.35, 0.8, 0.45))), 0.0), 20.0) * 0.9;
    return c;
}

float rrectSdf (vec2 p, vec2 halfSize, float r)
{
    vec2 q = abs (p) - halfSize + r;
    return length (max (q, 0.0)) + min (max (q.x, q.y), 0.0) - r;
}

float hash12 (vec2 p)
{
    vec3 p3 = fract (vec3 (p.xyx) * 0.1031);
    p3 += dot (p3, p3.yzx + 33.33);
    return fract ((p3.x + p3.y) * p3.z);
}

// Decorative step-response shape for the placeholder monitor (no controller maths).
float traceShape (float x)
{
    float s = max (x - 0.14, 0.0);
    float rise = 1.0 - exp (-s * 7.5) * (cos (s * 17.0) + 0.45 * sin (s * 17.0));
    return mix (0.0, rise, step (0.14, x));
}

void main()
{
    vec3 N = normalize (vNormal);
    vec3 V = normalize (uCamPos - vWorld);
    vec3 L = normalize (uLightDir);
    vec3 H = normalize (L + V);

    float ndl  = max (dot (N, L), 0.0);
    float wrap = max ((dot (N, L) + 0.35) / 1.35, 0.0);
    float ndv  = clamp (dot (N, V), 0.0, 1.0);
    float ndh  = max (dot (N, H), 0.0);

    vec3 lightCol = vec3 (1.0, 0.97, 0.94);
    vec3 amb  = mix (groundCol, skyCol, N.y * 0.5 + 0.5);
    float fill = max (dot (N, normalize (vec3 (0.8, 0.45, 0.9))), 0.0) * 0.22;

    vec3 col = vec3 (0.0);
    float alpha = 1.0;

    if (uMaterial == 0) // satin pink chassis
    {
        float fres = pow (1.0 - ndv, 3.0);
        col  = uBaseColor * (amb * 0.62 + wrap * lightCol * 0.85 + fill);
        col += lightCol * pow (ndh, 16.0) * 0.12;
        col += vec3 (1.0, 0.80, 0.90) * fres * 0.20;
    }
    else if (uMaterial == 1) // pearlescent panel
    {
        vec2 uv = (vWorld.xz - uPanelRect.xy) / uPanelRect.zw;
        vec4 d = texture (uDecal, uv);
        float facing = 1.0 - ndv;

        // Fresnel-driven view-angle hue shift
        float t = facing * 2.4 + dot (vWorld.xz, vec2 (0.10, 0.06)) + dot (V.xz, vec2 (0.8, 0.5)) + 0.1;
        vec3 irid = 0.5 + 0.5 * cos (6.28318 * (vec3 (0.0, 0.33, 0.67) + t));
        vec3 pearl = mix (vec3 (0.95, 0.94, 0.965), irid, 0.08 + 0.45 * facing * facing);

        float print = max (d.r, d.g);
        vec3 albedo = mix (pearl, vec3 (0.15, 0.13, 0.16), d.r * 0.92);
        albedo = mix (albedo, vec3 (0.92, 0.30, 0.56), d.g);

        col = albedo * (amb * 0.72 + wrap * lightCol * 0.82 + fill);

        vec3 R = reflect (-V, N);
        float fres = 0.04 + 0.96 * pow (facing, 5.0);
        col += envColor (R) * mix (vec3 (1.0), irid, 0.6) * fres * 0.55 * (1.0 - print * 0.6);
        col += mix (vec3 (1.0), irid, 0.35) * pow (ndh, 70.0) * 0.5 * (1.0 - print * 0.5);

        // Idle breathing sheen
        float bandCoord = dot (vWorld.xz, normalize (vec2 (1.0, -0.55))) - uSheen.x;
        float band = exp (-bandCoord * bandCoord / 0.22);
        col += mix (vec3 (1.0, 0.96, 1.0), irid, 0.55) * band * uSheen.y * (0.45 + 0.55 * facing) * (1.0 - print * 0.7);

        // Vent glow spilling onto the surface
        float vd = rrectSdf (vWorld.xz - uVentRect.xy, uVentRect.zw, 0.02);
        float halo = exp (-max (vd, 0.0) * 11.0);
        col = mix (col, col * 0.55 + uVentGlow * 1.1, clamp (halo * length (uVentGlow) * 0.55, 0.0, 0.6));

        // Footstep glyphs
        col = mix (col, uGlyphL * 1.6, clamp (d.b * length (uGlyphL) * 1.5, 0.0, 1.0));
        col = mix (col, uGlyphR * 1.6, clamp (d.a * length (uGlyphR) * 1.5, 0.0, 1.0));
        float glyphHalo = exp (-length (vWorld.xz - vec2 (2.08, -0.93)) * 7.0);
        vec3 glyphGlow = uGlyphL + uGlyphR;
        col = mix (col, col * 0.6 + glyphGlow, clamp (glyphHalo * length (glyphGlow) * 0.35, 0.0, 0.5));
    }
    else if (uMaterial == 2) // metal
    {
        vec3 R = reflect (-V, N);
        float fres = 0.55 + 0.45 * pow (1.0 - ndv, 5.0);
        col  = uBaseColor * (amb * 0.25 + ndl * lightCol * 0.35);
        col += envColor (R) * uBaseColor * 0.8 * fres;
        col += lightCol * pow (ndh, 90.0) * 1.1;

        if (N.y > 0.9)
        {
            float rings = length (vLocal.xz) * 700.0;
            float aa = clamp (1.0 - fwidth (rings) * 0.6, 0.0, 1.0);
            col *= 1.0 + 0.06 * sin (rings) * aa;
        }
    }
    else if (uMaterial == 3) // satin plastic / rubber
    {
        col  = uBaseColor * (amb * 0.6 + wrap * lightCol * 0.8 + fill);
        col += lightCol * pow (ndh, 42.0) * 0.30;
        col += vec3 (0.75, 0.75, 0.85) * pow (1.0 - ndv, 4.0) * 0.16;
        col += uEmissive;
    }
    else if (uMaterial == 4) // wooden table
    {
        vec2 p = vWorld.xz;
        float grain = sin (p.x * 6.0 + sin (p.y * 1.2 + p.x * 0.35) * 2.4 + sin (p.y * 0.29) * 4.0);
        float fine  = sin (p.x * 58.0 + sin (p.y * 2.9) * 3.0) * 0.5 + 0.5;
        vec3 albedo = mix (vec3 (0.17, 0.10, 0.08), vec3 (0.31, 0.19, 0.13), 0.5 + 0.3 * grain) * (0.90 + 0.10 * fine);
        col  = albedo * (amb * 0.5 + ndl * lightCol * 0.75);
        col += lightCol * pow (ndh, 24.0) * 0.10;
        col *= mix (1.0, 0.18, smoothstep (3.5, 11.0, length (p - vec2 (0.0, 0.4))));
        col += uVentGlow * exp (-length (p - vec2 (0.5, -1.4)) * 1.2) * 0.04;
    }
    else if (uMaterial == 5) // emissive: LEDs + vent floors
    {
        col  = uBaseColor + uEmissive;
        col += uVentGlow * (1.8 + 0.5 * sin (vWorld.x * 5.0 - uTime * 2.4));
        col += vec3 (1.0) * pow (ndh, 50.0) * 0.35;
    }
    else if (uMaterial == 6) // recess interior
    {
        float depthT = clamp ((vWorld.y - (uRecess.x - uRecess.y)) / uRecess.y, 0.0, 1.0);
        col  = uBaseColor * (amb * 0.55 + wrap * 0.45) * (0.30 + 0.70 * depthT);
        col += uVentGlow * (1.0 - depthT) * 1.6;
    }
    else if (uMaterial == 7) // knob flange (skirt + printed dial face)
    {
        col  = uBaseColor * (amb * 0.55 + wrap * lightCol * 0.7 + fill);
        col += lightCol * pow (ndh, 36.0) * 0.28;
        col += vec3 (0.8) * pow (1.0 - ndv, 4.0) * 0.12;

        if (N.y > 0.95)
        {
            vec2 dialUV = vLocal.xz / (2.0 * uRing.w) + 0.5;
            float num = texture (uDial, dialUV).r;
            col = mix (col, vec3 (0.90, 0.89, 0.92) * (0.65 + 0.45 * wrap), num);
        }
    }
    else if (uMaterial == 8) // oscilloscope glass
    {
        vec2 uv = vUV;
        vec3 phosphor = vec3 (0.38, 1.0, 0.78);
        col = vec3 (0.012, 0.030, 0.026);

        vec2 g  = uv * vec2 (10.0, 6.0);
        vec2 gw = fwidth (g);
        vec2 gl = 1.0 - smoothstep (vec2 (0.0), gw * 1.5, abs (fract (g + 0.5) - 0.5));
        float grid = max (gl.x, gl.y) * 0.13;

        // Trace region
        float x = (uv.x - 0.05) / 0.90;
        float inX = step (0.0, x) * step (x, 1.0);
        float yTop = 0.30, yBot = 0.76;
        float f  = mix (yBot, yTop, traceShape (x));
        float f2 = mix (yBot, yTop, traceShape (x + 0.002));
        float slope = (f2 - f) / (0.002 * 0.90) / uScope.z;
        float dist = abs (uv.y - f) / sqrt (1.0 + slope * slope);
        float px = fwidth (uv.y);

        float age  = fract (uScope.x - x);
        float persistence = exp (-age * 2.2);
        float core = 1.0 - smoothstep (px * 0.8, px * 2.2, dist);
        float glow = exp (-dist / 0.03) * 0.35;
        float trace = (core + glow) * persistence * inX;

        float setpoint = (1.0 - smoothstep (px * 0.5, px * 1.5, abs (uv.y - yTop))) * step (0.5, fract (uv.x * 40.0)) * 0.35;
        float head = exp (-pow ((x - uScope.x) * 60.0, 2.0)) * exp (-pow ((uv.y - mix (yBot, yTop, traceShape (uScope.x))) * 40.0, 2.0)) * 1.2;

        float text = texture (uOverlay, uv).r;
        float scan = 0.92 + 0.08 * sin (uv.y * 420.0);

        col += phosphor * (grid + trace + setpoint + head + text * 0.95) * uScope.y * scan;
        col += phosphor * 0.03 * exp (-length ((uv - 0.5) * vec2 (1.4, 2.0)) * 2.0) * uScope.y;

        vec3 R = reflect (-V, N);
        col += envColor (R) * (0.04 + 0.20 * pow (1.0 - ndv, 4.0));
        float streak = exp (-pow ((uv.x + uv.y * 0.6 - 0.45 - uSheen.x * 0.05) * 7.0, 2.0));
        col += vec3 (0.9, 0.95, 1.0) * streak * 0.035;
    }
    else if (uMaterial == 9) // LED value ring printed around each knob
    {
        float a = atan (vLocal.x, -vLocal.z);
        float start = -0.75 * PI;
        float sweep = 1.5 * PI;
        float inRange = step (start - 0.02, a) * step (a, start + sweep + 0.02);

        float seg = fract ((a - start) / (sweep / 20.0) + 0.5);
        float dotMask = smoothstep (0.18, 0.30, seg) * (1.0 - smoothstep (0.70, 0.82, seg));
        float lit = 1.0 - smoothstep (uRing.x - 0.01, uRing.x + 0.04, a);

        float r = length (vLocal.xz) / uRing.w;
        float radial = smoothstep (1.06, 1.09, r) * (1.0 - smoothstep (1.16, 1.19, r));

        vec3 idle = vec3 (0.42, 0.40, 0.45);
        vec3 onCol = uRingColor * (0.75 + uRing.y * 1.2 + uRing.z * 0.3);
        col = mix (idle, onCol, lit);
        float headGlow = exp (-pow ((a - uRing.x) * 6.0, 2.0)) * uRing.y;
        col += uRingColor * headGlow * 0.8;

        alpha = inRange * radial * dotMask * mix (0.30, 1.0, lit);
    }
    else if (uMaterial == 10) // analytic soft shadow
    {
        float sd = rrectSdf (vWorld.xz - uShadow.xy, uShadow.zw, uShadowParams.x);
        alpha = (1.0 - smoothstep (-uShadowParams.y, uShadowParams.y, sd)) * uShadowParams.z;
        col = vec3 (0.02, 0.0, 0.01);
    }

    // Gentle highlight roll-off, vignette, dither
    col = col * (1.0 + col / 4.0) / (1.0 + col);
    vec2 sp = gl_FragCoord.xy / uViewport - 0.5;
    col *= 1.0 - 0.45 * pow (length (sp) * 1.25, 2.4);
    col += (hash12 (gl_FragCoord.xy) - 0.5) / 255.0;

    fragColor = vec4 (col, alpha);
}
)GLSL";
}
