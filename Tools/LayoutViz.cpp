/*  LayoutViz - an analysis tool around ENH Master's real layout code. It does not change the plugin.

    It includes the same DeviceLayout.h and CameraRig.h the renderer and picking use, asks them where
    every unit and control is, and projects that through the real camera for a chosen editor size.

        WIDTH=1200 HEIGHT=700 [FOCUS=<unit 0-4> AMOUNT=0..1] [OUT=file.svg] build/.../LayoutViz

    Writes an SVG (window boundary, pixel grid and rulers, every unit's faceplate as projected, its
    centre, every control's footprint) and prints the underlying values to stdout, each labelled
    FIXED (a constant), MULTIPLIER (a constant times another value) or CALCULATED (derived).

    How the layout works (read from the code, see the report printed by this tool):
      - Units and controls live in world units, not pixels. Their positions and sizes are constants in
        DeviceLayout.h (panel-local x across, z down the faceplate), placed on a vertical arc by
        unitArcPos() / unitOrigin() / panelToWorld(). Nothing in the scene depends on the window.
      - The window only reaches the camera: CameraRig::build (aspect) moves the eye back until the
        whole case fits (width- or height-limited). So on screen every model scales together with
        the editor size - that is the only proportional part, and it is what the pixels here show.
*/
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "UI/Scene/DeviceLayout.h"
#include "UI/Scene/CameraRig.h"

using namespace pad;
using namespace pad::layout;

namespace
{
    int envInt (const char* name, int fallback)
    {
        const char* v = std::getenv (name);
        return v != nullptr ? std::atoi (v) : fallback;
    }

    float envFloat (const char* name, float fallback)
    {
        const char* v = std::getenv (name);
        return v != nullptr ? (float) std::atof (v) : fallback;
    }

    struct Px { float x = 0, y = 0; bool ok = false; };

    /** Text for the SVG (XML): unit names contain '&'. */
    std::string xml (std::string s)
    {
        std::string out;
        for (char c : s)
            out += c == '&' ? std::string ("&amp;") : c == '<' ? std::string ("&lt;") : c == '>' ? std::string ("&gt;") : std::string (1, c);
        return out;
    }

    /** World point -> editor pixels (origin top left), exactly as the renderer's projection does. */
    Px toPixels (const CameraRig& cam, gfx::Vec3 world, int w, int h)
    {
        float nx = 0, ny = 0;
        Px p;
        p.ok = gfx::projectToNdc (cam.viewProj, world, nx, ny);
        p.x = (nx + 1.0f) * 0.5f * (float) w;
        p.y = (1.0f - ny) * 0.5f * (float) h;
        return p;
    }

    Px panelPx (const CameraRig& cam, int unit, float x, float z, float out, int w, int h)
    {
        return toPixels (cam, panelToWorld (unit).transformPoint ({ x, out, z }), w, h);
    }

    const char* colourFor (int unit)
    {
        switch (unit)
        {
            case enhUnit:     return "#e8a33c";
            case lumenUnit:   return "#4fc3f7";
            case limiterUnit: return "#ff4f8b";
            case tideUnit:    return "#8bd35a";
            case tubeUnit:    return "#b388ff";
            default:          return "#ffffff";
        }
    }

    const char* kindName (ControlKind k)
    {
        return k == ControlKind::knob ? "knob" : k == ControlKind::button ? "button" : k == ControlKind::toggle ? "toggle" : "selector";
    }

    /** How a control's panel position is written in DeviceLayout.h (read from the source). */
    std::string positionRule (const ControlDef& c)
    {
        const std::string id = c.paramId;
        if (c.unit == tubeUnit && c.kind == ControlKind::knob && c.size > 0.99f && c.size < 1.01f)
            return "CALCULATED  x = seraphFirstKnobX + i * seraphKnobStep, z = seraphRow1Z / seraphRow2Z";
        if (isOneU (c.unit) && c.kind == ControlKind::knob)
            return "CALCULATED  x = oneUKnobX + i * oneUKnobStep, z = oneUKnobZ";
        if (isOneU (c.unit) && c.kind == ControlKind::toggle)
            return c.unit == limiterUnit ? "CALCULATED  x = oneUKnobX + 2 * oneUKnobStep + 0.38, z = oneUButtonZ"
                                         : "FIXED       x = oneUButtonX, z = oneUButtonZ";
        if (c.unit == tubeUnit && c.kind == ControlKind::toggle)
            return "CALCULATED  z = seraphRow1Z - 0.10 / seraphRow2Z + 0.02, x fixed";
        if (c.unit == tubeUnit && c.size < 0.99f)
            return "FIXED       x = seraphMasterX[i], z = seraphMasterZ";
        if (c.unit == enhUnit && c.size < 0.99f)
            return "FIXED       x = masterKnobX2[i], z = knobZ";
        return "FIXED       literal x, z = knobZ / buttonZ / literal";
    }
}

int main()
{
    const int W = std::max (100, envInt ("WIDTH", 1000));
    const int H = std::max (100, envInt ("HEIGHT", 740));
    const int focusUnit = std::clamp (envInt ("FOCUS", 0), 0, numUnits - 1);
    const float focusAmount = std::clamp (envFloat ("AMOUNT", std::getenv ("FOCUS") != nullptr ? 1.0f : 0.0f), 0.0f, 1.0f);
    const std::string outPath = std::getenv ("OUT") != nullptr ? std::getenv ("OUT") : "layout-viz.svg";

    // The renderer builds its camera from the editor's logical size exactly like this (no parallax)
    const auto cam = CameraRig::build ((float) W / (float) H, 0.0f, 0.0f, { focusUnit, focusAmount });

    // ---------------------------------------------------------------- text report
    std::printf ("ENH Master layout at %d x %d px (aspect %.3f), focus %s\n\n", W, H, (double) W / H,
                 focusAmount > 0.0f ? (std::string (unitInfo[(size_t) focusUnit].name) + " " + std::to_string (focusAmount)).c_str() : "none (whole rack)");
    std::printf ("Scene units are world units (not pixels). Window size affects ONLY the camera distance.\n");
    std::printf ("Camera: eye (%.3f, %.3f, %.3f)  FIXED fovY 22 deg, FIXED base pitch 11 deg, distance CALCULATED from aspect\n\n",
                 cam.eye.x, cam.eye.y, cam.eye.z);

    std::printf ("Arc: FIXED arcRadius %.3f, arcCentreY %.3f, arcCentreZ %.3f, rackGap %.3f; total arc length CALCULATED %.3f\n\n",
                 arcRadius, arcCentreY, arcCentreZ, rackGap, totalArcLength());

    std::printf ("%-20s %-9s %-9s %-9s %-9s %-10s %-10s | %-15s %-15s %-10s\n", "unit", "halfW", "halfH", "arcPos", "angle",
                 "origin.y", "origin.z", "centre px", "size px", "of window");
    struct UnitPx { int unit; Px c, tl, tr, br, bl; };
    std::vector<UnitPx> unitsPx;

    for (int u : rackOrder)
    {
        const float hh = unitHalfH (u);
        UnitPx up { u, panelPx (cam, u, 0, 0, 0, W, H), panelPx (cam, u, -faceHalfW, -hh, 0, W, H), panelPx (cam, u, faceHalfW, -hh, 0, W, H),
                    panelPx (cam, u, faceHalfW, hh, 0, W, H), panelPx (cam, u, -faceHalfW, hh, 0, W, H) };
        unitsPx.push_back (up);
        const auto o = unitOrigin (u);
        const float pw = up.tr.x - up.tl.x, ph = up.bl.y - up.tl.y;
        std::printf ("%-20s %-9.3f %-9.3f %-9.3f %-9.4f %-10.3f %-10.3f | %6.1f,%6.1f   %6.1f x %5.1f  %.3fW %.3fH\n",
                     unitInfo[(size_t) u].name, faceHalfW, hh, unitArcPos (u), unitAngle (u), o.y, o.z, up.c.x, up.c.y, pw, ph, pw / W, ph / H);
    }
    std::printf ("\n  halfW  FIXED faceHalfW (all units)\n"
                 "  halfH  FIXED faceHalfH (enhancer) / tubeHalfH (tone & space) / oneUHalfH (1U units)\n"
                 "  arcPos CALCULATED: sum over rackOrder below the unit of 2*halfH + rackGap, plus its own halfH\n"
                 "  angle  CALCULATED: (arcPos - totalArcLength/2) / arcRadius   (rotation about world X, radians)\n"
                 "  origin CALCULATED: (0, arcCentreY + arcRadius*sin(angle), arcCentreZ - arcRadius*cos(angle)); x always 0\n"
                 "  rotation CALCULATED: panelToWorld = translate(origin) * rotateX(pi/2 + angle); scale 1 (none)\n"
                 "  px     CALCULATED: projection of those world points through CameraRig at this window size\n\n");

    std::printf ("Controls (panel-local: x across, z down the faceplate; world = panelToWorld(unit) * (x, 0, z))\n");
    std::printf ("%-18s %-12s %-9s %-8s %-8s %-6s %-8s %-15s %s\n", "unit", "control", "kind", "x", "z", "size", "radius", "centre px", "rule");
    for (auto& c : controls)
    {
        const auto p = panelPx (cam, c.unit, c.x, c.z, 0, W, H);
        const float r = (c.kind == ControlKind::knob || c.kind == ControlKind::selector) ? knobBodyRadius (c) : 0.0f;
        std::printf ("%-18s %-12s %-9s %-8.3f %-8.3f %-6.2f %-8.3f %6.1f,%6.1f   %s\n", unitInfo[(size_t) c.unit].name, c.label, kindName (c.kind),
                     c.x, c.z, c.size, r, p.x, p.y, positionRule (c).c_str());
    }
    std::printf ("\n  size   FIXED multiplier per control (1 = the unit's standard knob)\n"
                 "  radius MULTIPLIER: knobBodyRadius = base radius (knobRadius / tubeKnobBodyRadius / oneUKnobRadius, FIXED) * size\n");

    // ---------------------------------------------------------------- SVG
    const int side = 470, Wt = W + side;
    std::FILE* f = std::fopen (outPath.c_str(), "w");
    if (f == nullptr)
    {
        std::fprintf (stderr, "cannot write %s\n", outPath.c_str());
        return 1;
    }

    std::fprintf (f, "<svg xmlns='http://www.w3.org/2000/svg' width='%d' height='%d' viewBox='0 0 %d %d' font-family='monospace'>\n", Wt, H + 40, Wt, H + 40);
    std::fprintf (f, "<rect width='%d' height='%d' fill='#15161a'/>\n", Wt, H + 40);
    std::fprintf (f, "<g transform='translate(0,20)'>\n");

    // Grid and rulers (pixels)
    for (int x = 0; x <= W; x += 50)
        std::fprintf (f, "<line x1='%d' y1='0' x2='%d' y2='%d' stroke='#2a2c33' stroke-width='%s'/>%s\n", x, x, H, x % 100 == 0 ? "1" : "0.5",
                      x % 100 == 0 ? ("<text x='" + std::to_string (x + 2) + "' y='-6' fill='#8a8f99' font-size='10'>" + std::to_string (x) + "</text>").c_str() : "");
    for (int y = 0; y <= H; y += 50)
        std::fprintf (f, "<line x1='0' y1='%d' x2='%d' y2='%d' stroke='#2a2c33' stroke-width='%s'/>%s\n", y, W, y, y % 100 == 0 ? "1" : "0.5",
                      y % 100 == 0 ? ("<text x='2' y='" + std::to_string (y - 2) + "' fill='#8a8f99' font-size='10'>" + std::to_string (y) + "</text>").c_str() : "");

    // Window boundary
    std::fprintf (f, "<rect x='0' y='0' width='%d' height='%d' fill='none' stroke='#ffffff' stroke-width='2'/>\n", W, H);
    std::fprintf (f, "<text x='%d' y='%d' fill='#ffffff' font-size='12' text-anchor='end'>editor %d x %d px</text>\n", W - 4, H - 6, W, H);

    // Case cheeks' front edges, for context
    for (float side2 : { -1.0f, 1.0f })
    {
        std::string pts;
        const float s0 = -caseOverhang, s1 = totalArcLength() + caseOverhang;
        for (int i = 0; i <= 40; ++i)
        {
            const float s = s0 + (s1 - s0) * (float) i / 40.0f, a = (s - 0.5f * totalArcLength()) / arcRadius;
            const auto p = toPixels (cam, { side2 * caseSideX, arcCentreY + arcRadius * std::sin (a), arcCentreZ - arcRadius * std::cos (a) }, W, H);
            pts += std::to_string (p.x) + "," + std::to_string (p.y) + " ";
        }
        std::fprintf (f, "<polyline points='%s' fill='none' stroke='#6b6f7a' stroke-width='1.5' stroke-dasharray='4 3'/>\n", pts.c_str());
    }

    // Units: faceplate outline, centre, name, size; controls inside
    for (auto& up : unitsPx)
    {
        const char* col = colourFor (up.unit);
        std::fprintf (f, "<polygon points='%.1f,%.1f %.1f,%.1f %.1f,%.1f %.1f,%.1f' fill='%s' fill-opacity='0.08' stroke='%s' stroke-width='2'/>\n",
                      up.tl.x, up.tl.y, up.tr.x, up.tr.y, up.br.x, up.br.y, up.bl.x, up.bl.y, col, col);
        std::fprintf (f, "<circle cx='%.1f' cy='%.1f' r='3.5' fill='%s'/>\n", up.c.x, up.c.y, col);
        std::fprintf (f, "<line x1='%.1f' y1='%.1f' x2='%.1f' y2='%.1f' stroke='%s' stroke-width='0.7'/><line x1='%.1f' y1='%.1f' x2='%.1f' y2='%.1f' stroke='%s' stroke-width='0.7'/>\n",
                      up.c.x - 8, up.c.y, up.c.x + 8, up.c.y, col, up.c.x, up.c.y - 8, up.c.x, up.c.y + 8, col);
        std::fprintf (f, "<text x='%.1f' y='%.1f' fill='%s' font-size='12' font-weight='bold'>%s</text>\n", up.tl.x + 4, up.tl.y + 13, col,
                      xml (unitInfo[(size_t) up.unit].name).c_str());
        std::fprintf (f, "<text x='%.1f' y='%.1f' fill='%s' font-size='10'>centre %.0f,%.0f px  %.0f x %.0f px</text>\n", up.tl.x + 4, up.tl.y + 25, col,
                      up.c.x, up.c.y, up.tr.x - up.tl.x, up.bl.y - up.tl.y);
    }

    for (auto& c : controls)
    {
        const auto p = panelPx (cam, c.unit, c.x, c.z, 0, W, H);
        const char* col = colourFor (c.unit);
        if (c.kind == ControlKind::knob || c.kind == ControlKind::selector)
        {
            const auto e = panelPx (cam, c.unit, c.x + knobBodyRadius (c), c.z, 0, W, H);
            std::fprintf (f, "<circle cx='%.1f' cy='%.1f' r='%.1f' fill='none' stroke='%s' stroke-width='1'><title>%s %s: panel x %.3f z %.3f, size %.2f, radius %.3f</title></circle>\n",
                          p.x, p.y, std::abs (e.x - p.x), col, xml (unitInfo[(size_t) c.unit].name).c_str(), c.label, c.x, c.z, c.size, knobBodyRadius (c));
        }
        else
        {
            const float hw = c.kind == ControlKind::button ? buttonHalfW + 0.016f : hwk::models::rockerHalfW;
            const float hd = c.kind == ControlKind::button ? buttonHalfD + 0.016f : hwk::models::rockerHalfD;
            const auto a = panelPx (cam, c.unit, c.x - hw, c.z - hd, 0, W, H), b = panelPx (cam, c.unit, c.x + hw, c.z + hd, 0, W, H);
            std::fprintf (f, "<rect x='%.1f' y='%.1f' width='%.1f' height='%.1f' fill='none' stroke='%s' stroke-width='1' stroke-dasharray='2 1'><title>%s %s (%s): panel x %.3f z %.3f</title></rect>\n",
                          std::min (a.x, b.x), std::min (a.y, b.y), std::abs (b.x - a.x), std::abs (b.y - a.y), col, xml (unitInfo[(size_t) c.unit].name).c_str(), c.label, kindName (c.kind), c.x, c.z);
        }
        std::fprintf (f, "<circle cx='%.1f' cy='%.1f' r='1' fill='%s'/>\n", p.x, p.y, col);
    }

    // Side panel: the values behind the drawing
    int ty = 14;
    auto line = [&] (const std::string& s, const char* col = "#d8dae0", int size = 11)
    {
        std::fprintf (f, "<text x='%d' y='%d' fill='%s' font-size='%d'>%s</text>\n", W + 14, ty, col, size, xml (s).c_str());
        ty += size + 5;
    };
    char buf[256];
    line ("ENH MASTER - LAYOUT (read from DeviceLayout.h)", "#ffffff", 12);
    line ("world units; the window only moves the camera");
    std::snprintf (buf, sizeof buf, "camera eye y %.2f z %.2f  (CALCULATED from aspect %.3f)", cam.eye.y, cam.eye.z, (double) W / H);
    line (buf);
    ty += 6;
    for (auto& up : unitsPx)
    {
        const int u = up.unit;
        const auto o = unitOrigin (u);
        line (unitInfo[(size_t) u].name, colourFor (u), 12);
        std::snprintf (buf, sizeof buf, "  halfW %.3f FIXED   halfH %.3f FIXED", faceHalfW, unitHalfH (u));
        line (buf);
        std::snprintf (buf, sizeof buf, "  arcPos %.3f  angle %.4f rad  CALCULATED", unitArcPos (u), unitAngle (u));
        line (buf);
        std::snprintf (buf, sizeof buf, "  origin (0, %.3f, %.3f)  CALCULATED  scale 1", o.y, o.z);
        line (buf);
        std::snprintf (buf, sizeof buf, "  on screen: centre %.0f,%.0f px = %.3f W, %.3f H", up.c.x, up.c.y, up.c.x / W, up.c.y / H);
        line (buf);
        std::snprintf (buf, sizeof buf, "  size %.0f x %.0f px = %.3f W x %.3f H", up.tr.x - up.tl.x, up.bl.y - up.tl.y, (up.tr.x - up.tl.x) / W, (up.bl.y - up.tl.y) / H);
        line (buf);
        ty += 4;
    }
    line ("controls: hover a circle / box for its panel values", "#8a8f99");
    std::fprintf (f, "</g>\n</svg>\n");
    std::fclose (f);

    std::printf ("\nSVG written to %s\n", outPath.c_str());
    return 0;
}
