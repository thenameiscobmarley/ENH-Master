#pragma once

/*  Blinn-Phong / Fresnel shaders. One source, compiled once per material with
    `#define MATERIAL n`, so the GPU never branches per fragment (important on
    Intel UHD 600). No shadow maps or post-processing: shadows are analytic SDF
    decals and glow is additive in-shader.
*/
namespace pad::shaders
{
    enum Material
    {
        chassis   = 0,   // matte pink with pearlescent pink shine
        faceplate = 1,   // pearl white with pink pearlescent clearcoat + printed decal
        chrome    = 2,   // polished / satin metal
        plastic   = 3,   // black bakelite, optional fluted grip
        table     = 4,
        emissive  = 5,
        recess    = 6,
        skirt     = 7,   // knob skirt with printed rotating scale
        display   = 8,
        shadow    = 9,
        numMaterials
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

    /** Prefixed at runtime with "#version 150\n#define MATERIAL n\n". */
    inline constexpr const char* fragmentBody = R"GLSL(
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in vec3 vLocal;

out vec4 fragColor;

uniform vec3  uCamPos;
uniform vec3  uLightDir;
uniform float uTime;
uniform vec2  uViewport;

uniform vec3  uBaseColor;
uniform vec3  uEmissive;
uniform vec4  uParams;       // material-specific
uniform vec4  uParams2;      // material-specific
uniform vec3  uGlow;         // vent / glyph glow colour
uniform vec3  uGlyphL;
uniform vec3  uGlyphR;

uniform sampler2D uTex;

const vec3 skyCol    = vec3 (0.80, 0.74, 0.80);
const vec3 groundCol = vec3 (0.30, 0.22, 0.23);

vec3 envColor (vec3 r)
{
    float t = clamp (r.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 c = mix (groundCol, skyCol, smoothstep (0.30, 0.75, t));
    // horizon glow + a large soft box in front of the unit (behind the viewer)
    c += vec3 (0.95, 0.85, 0.92) * exp (-pow ((r.y - 0.05) * 5.0, 2.0)) * 0.35;
    c += vec3 (1.0, 0.93, 0.97) * pow (max (dot (r, normalize (vec3 (-0.15, 0.30, 1.0))), 0.0), 4.0) * 0.55;
    // bright studio wall behind the viewer, with vertical light strips for chrome streaks
    float wall = smoothstep (0.15, 0.85, r.z);
    float strips = pow (0.5 + 0.5 * cos (r.x * 9.0), 8.0);
    c += vec3 (0.88, 0.80, 0.86) * wall * (0.35 + 0.55 * strips);
    // soft-box highlights (studio lights)
    c += vec3 (1.0, 0.95, 0.97) * pow (max (dot (r, normalize (vec3 (-0.45, 0.75, 0.55))), 0.0), 24.0) * 1.1;
    c += vec3 (1.0, 0.82, 0.90) * pow (max (dot (r, normalize (vec3 (0.70, 0.35, 0.60))), 0.0), 40.0) * 0.5;
    return c;
}

// Pearlescent pink: shifts between rose, peach and lilac with angle
vec3 pearlPink (float t)
{
    return vec3 (1.0, 0.62, 0.80) + vec3 (0.0, 0.16, 0.14) * cos (6.28318 * (t + vec3 (0.0, 0.28, 0.58)));
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

// Decorative response shape for the placeholder monitor (no controller maths)
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
    float wrap = max ((dot (N, L) + 0.4) / 1.4, 0.0);
    float ndv  = clamp (dot (N, V), 0.0, 1.0);
    float ndh  = max (dot (N, H), 0.0);
    float facing = 1.0 - ndv;

    vec3 lightCol = vec3 (1.0, 0.97, 0.95);
    vec3 amb  = mix (groundCol, skyCol, N.y * 0.5 + 0.5);
    float fill = max (dot (N, normalize (vec3 (0.75, 0.35, 0.9))), 0.0) * 0.22;
    vec3 R = reflect (-V, N);

    vec3 col = vec3 (0.0);
    float alpha = 1.0;

#if MATERIAL == 0   // matte pink chassis, pearlescent pink shine
    col  = uBaseColor * (amb * 0.62 + wrap * lightCol * 0.80 + fill);
    vec3 sheen = pearlPink (facing * 1.3 + ndh * 0.6 + vWorld.x * 0.05);
    col += sheen * (pow (ndh, 20.0) * 0.28 + pow (facing, 3.5) * 0.30);
    col += sheen * pow (max (dot (R, normalize (vec3 (-0.15, 0.30, 1.0))), 0.0), 10.0) * 0.18;
    col += sheen * pow (ndh, 140.0) * 0.35;

#elif MATERIAL == 1 // pearl faceplate + decal (panel-local)
    // uParams  = decal rect (minX, minZ, sizeX, sizeZ)
    // uParams2 = (sheenPhase, sheenStrength, ventCx, ventCz); uEmissive.xy = vent half size
    vec2 p = vLocal.xz;
    vec4 d = texture (uTex, (p - uParams.xy) / uParams.zw);
    float print = max (d.r, d.g);

    vec3 pearl = mix (vec3 (0.95, 0.935, 0.95), pearlPink (facing * 2.0 + p.x * 0.07), 0.10 + 0.30 * facing);
    vec3 albedo = mix (pearl, vec3 (0.13, 0.11, 0.13), d.r * 0.94);
    albedo = mix (albedo, vec3 (0.90, 0.28, 0.55), d.g);

    col = albedo * (amb * 0.70 + wrap * lightCol * 0.78 + fill);

    vec3 shine = pearlPink (ndh * 1.4 + facing + p.x * 0.05);
    float coat = 1.0 - print * 0.55;
    float softbox = pow (max (dot (R, normalize (vec3 (-0.15, 0.30, 1.0))), 0.0), 14.0);
    col += shine * softbox * 0.30 * coat;
    col += envColor (R) * shine * (0.04 + 0.45 * pow (facing, 4.0)) * coat;
    col += shine * (pow (ndh, 90.0) * 0.55 + pow (ndh, 12.0) * 0.12) * coat;

    // Idle breathing sheen sweeping across the faceplate
    float band = p.x * 0.9 - p.y * 0.5 - uParams2.x;
    col += pearlPink (p.x * 0.2 + uTime * 0.03) * exp (-band * band / 0.10) * uParams2.y * coat;

    // Vent glow spilling onto the faceplate
    float vd = rrectSdf (p - uParams2.zw, uEmissive.xy, 0.02);
    float halo = exp (-max (vd, 0.0) * 12.0) * step (0.0, vd);
    col = mix (col, col * 0.6 + uGlow * 1.1, clamp (halo * length (uGlow) * 0.5, 0.0, 0.6));

    // Footprint glyphs
    col = mix (col, uGlyphL * 1.5, clamp (d.b * length (uGlyphL) * 1.6, 0.0, 1.0));
    col = mix (col, uGlyphR * 1.5, clamp (d.a * length (uGlyphR) * 1.6, 0.0, 1.0));

#elif MATERIAL == 2 // chrome. uParams.x = polish (0 satin .. 1 mirror), uParams.y > 0.5 = brushed rings
    float polish = uParams.x;
    float fres = mix (0.55, 1.0, pow (facing, 5.0));
    col  = uBaseColor * (amb * 0.18 + ndl * lightCol * 0.25) * (1.0 - polish * 0.6);
    col += envColor (R) * mix (uBaseColor, vec3 (1.0), fres * 0.5) * mix (0.55, 0.95, polish) * fres;
    col += lightCol * pow (ndh, mix (40.0, 260.0, polish)) * mix (0.6, 2.2, polish);
    col += pearlPink (facing + ndh) * pow (facing, 3.0) * 0.12;
    if (uParams.y > 0.5)
    {
        float rings = length (vLocal.xz) * 700.0;
        col *= 1.0 + 0.07 * sin (rings) * clamp (1.0 - fwidth (rings) * 0.6, 0.0, 1.0);
    }

#elif MATERIAL == 3 // bakelite. uParams.x = flute count (0 = none), uParams.y = flutes only below this local y
    float shade = 1.0;
    if (uParams.x > 0.0 && vLocal.y < uParams.y)
        shade = 0.25 + 0.75 * smoothstep (-0.5, 0.5, sin (atan (vLocal.x, vLocal.z) * uParams.x));
    col  = uBaseColor * (amb * 0.60 + wrap * lightCol * 0.80 + fill);
    col += lightCol * (pow (ndh, 50.0) * 0.32 + pow (ndh, 8.0) * 0.06) * shade;
    col += envColor (R) * 0.05 * shade;
    col += pearlPink (facing) * pow (facing, 4.0) * 0.14;
    col += uEmissive;

#elif MATERIAL == 4 // wooden table
    vec2 p = vWorld.xz;
    float grain = sin (p.x * 6.0 + sin (p.y * 1.2 + p.x * 0.35) * 2.4 + sin (p.y * 0.29) * 4.0);
    float fine  = sin (p.x * 58.0 + sin (p.y * 2.9) * 3.0) * 0.5 + 0.5;
    vec3 albedo = mix (vec3 (0.16, 0.095, 0.075), vec3 (0.30, 0.18, 0.13), 0.5 + 0.3 * grain) * (0.90 + 0.10 * fine);
    col  = albedo * (amb * 0.5 + ndl * lightCol * 0.75);
    col += lightCol * pow (ndh, 24.0) * 0.08;
    col *= mix (1.0, 0.15, smoothstep (3.5, 10.0, length (p - vec2 (0.0, 0.2))));

#elif MATERIAL == 5 // emissive (LEDs, lamp, vent floors). uParams.x = glow amount
    col  = uBaseColor + uEmissive;
    col += uGlow * (1.6 + 0.5 * sin (vLocal.z * 14.0 - uTime * 2.4)) * uParams.x;
    col += vec3 (1.0) * pow (ndh, 60.0) * 0.45;

#elif MATERIAL == 6 // recess interior (panel-local, top at y = 0). uParams.x = depth
    float depthT = clamp ((vLocal.y + uParams.x) / uParams.x, 0.0, 1.0);
    col  = uBaseColor * (amb * 0.55 + wrap * 0.45) * (0.30 + 0.70 * depthT);
    col += uGlow * (1.0 - depthT) * 1.5;

#elif MATERIAL == 7 // knob skirt: bakelite + rotating printed scale. uParams.x = dial radius, uParams.y = dial face y
    col  = uBaseColor * (amb * 0.55 + wrap * lightCol * 0.75 + fill);
    col += lightCol * pow (ndh, 44.0) * 0.30;
    col += pearlPink (facing) * pow (facing, 4.0) * 0.14;
    if (vLocal.y > uParams.y - 0.001)
    {
        float num = texture (uTex, vLocal.xz / (2.0 * uParams.x) + 0.5).r;
        col = mix (col, vec3 (0.93, 0.91, 0.93) * (0.70 + 0.40 * wrap), num);
    }

#elif MATERIAL == 8 // display glass. uParams = (sweepHead, brightness, aspect, _)
    vec2 uv = vUV;
    vec3 phosphor = vec3 (0.40, 1.0, 0.80);
    col = vec3 (0.010, 0.026, 0.024);

    vec2 g  = uv * vec2 (10.0, 6.0);
    vec2 gw = fwidth (g);
    vec2 gl = 1.0 - smoothstep (vec2 (0.0), gw * 1.5, abs (fract (g + 0.5) - 0.5));
    float grid = max (gl.x, gl.y) * 0.12;

    float x = (uv.x - 0.05) / 0.90;
    float inX = step (0.0, x) * step (x, 1.0);
    float yTop = 0.32, yBot = 0.74;
    float f  = mix (yBot, yTop, traceShape (x));
    float f2 = mix (yBot, yTop, traceShape (x + 0.002));
    float slope = (f2 - f) / (0.002 * 0.90) / uParams.z;
    float dist = abs (uv.y - f) / sqrt (1.0 + slope * slope);
    float px = fwidth (uv.y);

    float persistence = exp (-fract (uParams.x - x) * 2.2);
    float core = 1.0 - smoothstep (px * 0.8, px * 2.2, dist);
    float trace = (core + exp (-dist / 0.03) * 0.35) * persistence * inX;
    float setpoint = (1.0 - smoothstep (px * 0.5, px * 1.5, abs (uv.y - yTop))) * step (0.5, fract (uv.x * 40.0)) * 0.35;

    float text = texture (uTex, uv).r;
    float scan = 0.92 + 0.08 * sin (uv.y * 420.0);

    col += phosphor * (grid + trace + setpoint + text) * uParams.y * scan;
    col += envColor (R) * (0.03 + 0.25 * pow (facing, 4.0));
    col += vec3 (0.9, 0.95, 1.0) * exp (-pow ((uv.x + uv.y * 0.6 - 0.35) * 6.0, 2.0)) * 0.04;

#elif MATERIAL == 9 // analytic soft shadow on a [-1,1] quad
    // uParams = (scaleX, scaleZ, halfW, halfD), uParams2 = (cornerRadius, blur, strength, _)
    vec2 p = vLocal.xz * uParams.xy;
    float sd = rrectSdf (p, uParams.zw, uParams2.x);
    alpha = (1.0 - smoothstep (-uParams2.y, uParams2.y, sd)) * uParams2.z;
    col = vec3 (0.03, 0.0, 0.015);
#endif

    // Highlight roll-off, vignette, dither
    col = col * (1.0 + col / 4.0) / (1.0 + col);
    vec2 sp = gl_FragCoord.xy / uViewport - 0.5;
    col *= 1.0 - 0.40 * pow (length (sp) * 1.25, 2.4);
    col += (hash12 (gl_FragCoord.xy) - 0.5) / 255.0;

    fragColor = vec4 (col, alpha);
}
)GLSL";
}
