#include "LayoutAudit.h"
#include "DeviceLayout.h"
#include "DesignedLayout.h"
#include "UnitPanels.h"
#include "../../DSP/units/UnitList.h"
#include <juce_graphics/juce_graphics.h>

namespace pad::audit
{
    using namespace layout;

    namespace
    {
        /** Something print must keep clear of, in panel-local units. */
        struct Obstacle
        {
            enum Shape { circle, rect, ring, hole } shape;
            float x, z, a, b;          // circle: r = a | rect / hole: half sizes a, b | ring: radii a..b
            juce::String name;
        };

        struct Box { float x0, z0, x1, z1; };

        Box boxOf (const artwork::TextItem& t) { return { t.x - t.halfW, t.z - t.halfH, t.x + t.halfW, t.z + t.halfH }; }

        /** Distance from a point to a box (0 inside). */
        float pointToBox (float x, float z, const Box& b)
        {
            const float dx = std::max ({ b.x0 - x, 0.0f, x - b.x1 });
            const float dz = std::max ({ b.z0 - z, 0.0f, z - b.z1 });
            return std::hypot (dx, dz);
        }

        float farthestCorner (float x, float z, const Box& b)
        {
            return std::hypot (std::max (std::abs (b.x0 - x), std::abs (b.x1 - x)), std::max (std::abs (b.z0 - z), std::abs (b.z1 - z)));
        }

        /** Clearance between print and an obstacle: negative = overlapping. */
        float clearance (const Box& t, const Obstacle& o)
        {
            switch (o.shape)
            {
                case Obstacle::circle: return pointToBox (o.x, o.z, t) - o.a;
                case Obstacle::rect:
                {
                    const float dx = std::max (t.x0 - (o.x + o.a), (o.x - o.a) - t.x1);
                    const float dz = std::max (t.z0 - (o.z + o.b), (o.z - o.b) - t.z1);
                    if (dx <= 0.0f && dz <= 0.0f)
                        return std::max (dx, dz);
                    return std::hypot (std::max (0.0f, dx), std::max (0.0f, dz));
                }
                case Obstacle::ring:
                {
                    const float nearD = pointToBox (o.x, o.z, t), farD = farthestCorner (o.x, o.z, t);
                    if (farD < o.a) return o.a - farD;      // wholly inside the ring's hole
                    if (nearD > o.b) return nearD - o.b;    // wholly outside
                    return -std::min (farD - o.a, o.b - nearD);
                }
                case Obstacle::hole:
                {
                    // Print inside a window (display labels) must keep clear of its edge from the inside
                    const bool inside = t.x0 > o.x - o.a && t.x1 < o.x + o.a && t.z0 > o.z - o.b && t.z1 < o.z + o.b;
                    if (inside)
                        return std::min ({ t.x0 - (o.x - o.a), (o.x + o.a) - t.x1, t.z0 - (o.z - o.b), (o.z + o.b) - t.z1 });
                    const float dx = std::max (t.x0 - (o.x + o.a), (o.x - o.a) - t.x1);
                    const float dz = std::max (t.z0 - (o.z + o.b), (o.z - o.b) - t.z1);
                    if (dx <= 0.0f && dz <= 0.0f)
                        return std::max (dx, dz);
                    return std::hypot (std::max (0.0f, dx), std::max (0.0f, dz));
                }
            }
            return 1.0f;
        }

        float boxGap (const Box& a, const Box& b)
        {
            const float dx = std::max (a.x0 - b.x1, b.x0 - a.x1);
            const float dz = std::max (a.z0 - b.z1, b.z0 - a.z1);
            if (dx <= 0.0f && dz <= 0.0f)
                return std::max (dx, dz);
            return std::hypot (std::max (0.0f, dx), std::max (0.0f, dz));
        }

        float knobFootprint (const ControlDef& c)
        {
            return hwk::models::knob (c.style, knobBodyRadius (c), {}, 0).footprintRadius;   // the model's own
        }

        void addScrews (std::vector<Obstacle>& obs, const Rect* slots, size_t count)
        {
            for (size_t i = 0; i < count; ++i)
                obs.push_back ({ Obstacle::circle, slots[i].cx + (slots[i].cx > 0 ? -0.02f : 0.02f), slots[i].cz, 0.05f, 0.0f, "rack screw" });
        }

        void addBorder (std::vector<Obstacle>& obs, const Rect& b, const juce::String& name)
        {
            constexpr float t = 0.003f;
            obs.push_back ({ Obstacle::rect, b.cx, b.minZ(), b.hw, t, name + " border (top)" });
            obs.push_back ({ Obstacle::rect, b.cx, b.maxZ(), b.hw, t, name + " border (bottom)" });
            obs.push_back ({ Obstacle::rect, b.minX(), b.cz, t, b.hd, name + " border (left)" });
            obs.push_back ({ Obstacle::rect, b.maxX(), b.cz, t, b.hd, name + " border (right)" });
        }

        std::vector<Obstacle> obstaclesFor (int unit)
        {
            std::vector<Obstacle> obs;

            for (auto& c : controls)
            {
                if (c.unit != unit)
                    continue;

                const juce::String name = juce::String (c.label) + (c.kind == ControlKind::knob ? " knob" : c.kind == ControlKind::selector ? " selector" : c.kind == ControlKind::toggle ? " toggle" : " button");

                if (c.kind == ControlKind::knob || c.kind == ControlKind::selector)
                {
                    obs.push_back ({ Obstacle::circle, c.x, c.z, knobFootprint (c), 0.0f, name });

                    // The printed ticks around it: 270 degrees of them, open at the bottom
                    float r0 = 0.0f, r1 = 0.0f;
                    int ticks = 10;
                    if (unit == enhUnit)
                    {
                        r0 = 0.157f * c.size; r1 = 0.196f * c.size;
                        ticks = 30;
                    }
                    else if (unit == tubeUnit && c.kind == ControlKind::knob)
                    {
                        const float r = knobRadius * tubeKnobScale * c.size;
                        r0 = r + 0.028f; r1 = r + 0.060f;
                    }
                    else if (c.kind == ControlKind::knob)
                    {
                        const float r = oneUKnobRadius * c.size * 1.26f;
                        r0 = r + 0.012f; r1 = r + 0.040f;
                    }
                    for (int i = 0; r1 > 0.0f && i <= ticks; ++i)
                    {
                        const float angle = knobAngleForValue ((float) i / (float) ticks);
                        for (float f : { 0.0f, 0.5f, 1.0f })
                        {
                            const float rr = r0 + (r1 - r0) * f;
                            obs.push_back ({ Obstacle::circle, c.x + std::sin (angle) * rr, c.z - std::cos (angle) * rr, 0.005f, 0.0f,
                                             juce::String (c.label) + " ticks" });
                        }
                    }
                }
                else if (c.kind == ControlKind::toggle)
                {
                    // The rocker's bezel
                    obs.push_back ({ Obstacle::rect, c.x, c.z, switchOutline (c.switchStyle).halfW, switchOutline (c.switchStyle).halfD, name });
                }
                else
                {
                    obs.push_back ({ Obstacle::rect, c.x, c.z, buttonOutline (c.buttonStyle).halfW, buttonOutline (c.buttonStyle).halfD, name });
                    if (hasLed (c))
                    {
                        if (std::string_view (c.paramId) == pad::params::id::clarityMode)
                            for (float dx : { -modeLedDx, modeLedDx })
                                obs.push_back ({ Obstacle::circle, c.x + dx, c.z + buttonLedDz, ledRadius * 1.1f, 0.0f, "MODE LED" });
                        else
                        {
                            const auto [ledDx, ledDz] = ledOffset (c);
                            obs.push_back ({ Obstacle::circle, c.x + ledDx, c.z + ledDz, ledRadius * 1.1f, 0.0f, juce::String (c.label) + " LED" });
                        }
                    }
                }
            }

            if (unit == enhUnit)
            {
                addScrews (obs, earSlots.data(), earSlots.size());
                obs.push_back ({ Obstacle::hole, displayRect.cx, displayRect.cz, displayRect.hw + 0.07f, displayRect.hd + 0.07f, "display bezel" });
                for (auto& s : sections)
                {
                    addBorder (obs, s.box, juce::String (s.title) + " section");
                    obs.push_back ({ Obstacle::rect, s.box.cx, sectionTitleZ + 0.047f, s.box.hw - 0.03f, 0.002f, juce::String (s.title) + " title rule" });
                }
                obs.push_back ({ Obstacle::circle, powerLedX, powerLedZ, ledRadius * 1.1f, 0.0f, "power LED" });
                for (auto* ladder : { &detectLadder, &outLadder, &enhLadder })
                    for (int k = 0; k < ladder->segments; ++k)
                        obs.push_back ({ Obstacle::circle, ladder->x, ladderLedZ (k), ledRadius * 1.1f, 0.0f, juce::String (ladder->label) + " LED" });
            }
            else if (unit == tubeUnit)
            {
                addScrews (obs, tubeEarSlots.data(), tubeEarSlots.size());
                obs.push_back ({ Obstacle::circle, lampX, lampZ, 0.072f, 0.0f, "lamp" });
                obs.push_back ({ Obstacle::hole, seraphDisplayRect.cx, seraphDisplayRect.cz, seraphDisplayRect.hw + 0.032f, seraphDisplayRect.hd + 0.032f, "display bezel" });
                obs.push_back ({ Obstacle::hole, seraphDisplayRect.cx, seraphDisplayRect.cz, seraphDisplayRect.hw, seraphDisplayRect.hd, "display window" });
            }
            else
            {
                const auto ears = outboardEarSlots (unit);
                addScrews (obs, ears.data(), ears.size());
                if (unit == lunchboxUnit)
                {
                    // Each module's plate ends at its seam: print may not cross into the next module
                    for (int m = 0; m < lb::numModules; ++m)
                    {
                        if (! lb::installed (m)) continue;
                        const float hw = 0.5f * lbSlotW * (float) lb::widthOf (m);
                        for (float sx : { -1.0f, 1.0f })
                            obs.push_back ({ Obstacle::rect, lbModuleX (m) + sx * hw, 0.0f, 0.012f, lbModuleHalfH, juce::String (lb::nameOf (m)) + " seam" });
                        for (float sz : { -1.0f, 1.0f })   // and its two screws
                            obs.push_back ({ Obstacle::circle, lbModuleX (m), sz * (lbModuleHalfH - 0.055f), 0.028f, 0.0f, juce::String (lb::nameOf (m)) + " screw" });
                    }
                    if (lb::installed (1))
                        obs.push_back ({ Obstacle::circle, lbModuleX (1) + lbCutLedDx, lbCutLedZ, ledRadius * 1.1f, 0.0f, "CUT LED" });
                }
                else
                    addBorder (obs, oneUSectionBox (unit), "control section");
                if (unit == powerUnit)
                {
                    for (float x : stripLedX)
                        obs.push_back ({ Obstacle::circle, x, stripLedZ, ledRadius * 1.1f, 0.0f, "strip LED" });
                    const auto so = switchOutline (hwk::models::SwitchStyle::rockerRed);
                    obs.push_back ({ Obstacle::rect, stripSwitchX, stripSwitchZ, so.halfW, so.halfD, "mains switch" });
                    for (int k = 0; k < stripOutlets; ++k)   // each outlet's face (0.72 of a real one: 0.68 x 0.60 in)
                        obs.push_back ({ Obstacle::rect, stripOutletX (k), stripOutletZ, 0.72f * 0.68f * 0.263f, 0.72f * 0.60f * 0.263f, "outlet" });
                }
                for (int i = 0; i < numVus (unit); ++i)
                    obs.push_back ({ Obstacle::hole, vuX (unit, i), vuZ (unit, i), vuHalfW (unit) + 0.03f, vuHalfHFor (unit) + 0.03f, "VU bezel" });
                for (auto& w : outboardWindows (unit))
                {
                    obs.push_back ({ Obstacle::hole, w.cx, w.cz, w.hw + 0.034f, w.hd + 0.034f, "display bezel" });
                    obs.push_back ({ Obstacle::hole, w.cx, w.cz, w.hw, w.hd, "display window" });
                }
            }

            // The panel's own edge
            const float h = unitHalfH (unit);
            obs.push_back ({ Obstacle::hole, 0.0f, 0.0f, unitHalfW (unit) - 0.02f, h - 0.02f, "panel edge" });
            return obs;
        }

        juce::Image toImage (const artwork::RawTexture& t)
        {
            juce::Image img (juce::Image::RGB, t.width, t.height, true, juce::SoftwareImageType());
            for (int y = 0; y < t.height; ++y)
                for (int x = 0; x < t.width; ++x)
                {
                    const auto* p = t.pixels.data() + (size_t) ((y * t.width + x) * t.channels);
                    const float ink = p[0] / 255.0f, line = t.channels > 1 ? p[1] / 255.0f : 0.0f;
                    const auto v = (juce::uint8) juce::jlimit (0, 255, (int) (40 + 215 * std::max (ink, 0.55f * line)));
                    img.setPixelAt (x, y, juce::Colour (v, v, v));
                }
            return img;
        }
    }

    namespace
    {
        /** A designed or newer unit's print list (DesignedLayout.h, UnitPanels.h). */
        std::vector<designed::Print> printsOf (int unit)
        {
            if (unit >= firstGenUnit && unit < firstGenUnit + gen::count) { const auto [p, n] = gen::printOf (unit - firstGenUnit); return { p, p + n }; }
            if (unit == x4Unit) return { designed::x4Print.begin(), designed::x4Print.end() };
            if (unit == velvetUnit) return { designed::velPrint.begin(), designed::velPrint.end() };
            if (unit == takebackUnit) return { designed::tbPrint.begin(), designed::tbPrint.end() };
            if (unit == scopeUnit) return { designed::scPrint.begin(), designed::scPrint.end() };
            return {};
        }

        /** What a designed / newer unit's print must keep clear of: each knob's body and the ticks printed round
            it (as renderDesignedDecal draws them: r x 1.18 .. 1.38, or a selector's dots at r x 1.25), its displays'
            bezels, and its switches and buttons (from the controls, as the older panels'). */
        std::vector<Obstacle> printedObstacles (int unit)
        {
            std::vector<Obstacle> obs;
            for (auto& o : obstaclesFor (unit))
                if (! o.name.endsWith (" ticks") && ! o.name.endsWith (" knob") && ! o.name.endsWith (" selector")
                    && ! o.name.contains ("section border"))   // (the 1U layout's section frame: not printed on these)
                    obs.push_back (o);
            for (const auto& p : printsOf (unit))
            {
                if (p.kind == 'K')
                {
                    const float r = p.w;
                    obs.push_back ({ Obstacle::circle, p.x, p.z, r * 1.05f, 0.0f, juce::String (p.text) + " knob" });
                    const auto* spec = pad::params::findSpec (p.param);
                    const bool selector = spec != nullptr && spec->kind == pad::params::Kind::choice;
                    const int n = selector ? std::max (2, spec->texts.size()) - 1 : std::max (2, p.steps);
                    for (int i = 0; i <= n && (selector || p.nums != 3); ++i)
                    {
                        const float angle = selector ? characterSelectorAngle ((float) i / (float) n) : knobAngleForValue ((float) i / (float) n);
                        for (float rr : selector ? std::initializer_list<float> { r * 1.25f } : std::initializer_list<float> { r * 1.18f, r * 1.28f, r * 1.38f })
                            obs.push_back ({ Obstacle::circle, p.x + std::sin (angle) * rr, p.z - std::cos (angle) * rr, 0.006f, 0.0f, juce::String (p.text) + " ticks" });
                    }
                }
                else if (p.kind == 'D')
                    obs.push_back ({ Obstacle::hole, p.x, p.z, 0.5f * p.w + 0.017f, 0.5f * p.h + 0.017f, "display bezel" });   // (a strip display's own text sits inside it)
            }
            return obs;
        }
    }

    void writeLayoutAudit (const artwork::TextureSet& textures, const artwork::TextRegistry& items, const juce::File& dir)
    {
        dir.createDirectory();
        juce::String report;
        constexpr float margin = 0.015f;   // closer than this (panel units) is reported as cramped

        struct Panel { int unit; const artwork::RawTexture* tex; const char* file; };
        std::vector<Panel> panels { { enhUnit, &textures.faceplateDecal, "enh.png" }, { tubeUnit, &textures.tubeDecal, "tube.png" },
                                    { tideUnit, &textures.tideDecal, "tide.png" }, { lumenUnit, &textures.lumenDecal, "lumen.png" },
                                    { limiterUnit, &textures.limiterDecal, "limiter.png" }, { deepUnit, &textures.deepDecal, "deepsub.png" }, { characterUnit, &textures.characterDecal, "character.png" }, { levelUnit, &textures.levelDecal, "level.png" },
                                    { balancerUnit, &textures.balancerDecal, "balancer.png" }, { monitorUnit, &textures.monitorDecal, "monitor.png" },
                                    { radarUnit, &textures.radarDecal, "radar.png" }, { powerUnit, &textures.powerDecal, "power.png" },
                                    { lunchboxUnit, &textures.lunchboxDecal, "lunchbox.png" } };
        // The designed units and the newer ones (their print from a list: DesignedLayout.h, UnitPanels.h)
        static std::vector<std::string> genFiles;
        genFiles.clear();
        for (size_t k = 0; k < textures.genDecal.size(); ++k) genFiles.push_back (std::string ("gen-") + enh::dsp::units::info[k].key + ".png");
        const size_t firstPrinted = panels.size();
        panels.push_back ({ x4Unit, &textures.x4Decal, "x4.png" }); panels.push_back ({ velvetUnit, &textures.velvetDecal, "velvet.png" });
        panels.push_back ({ takebackUnit, &textures.takebackDecal, "takeback.png" }); panels.push_back ({ scopeUnit, &textures.scopeDecal, "scope.png" });
        for (size_t k = 0; k < textures.genDecal.size() && k < (size_t) gen::count; ++k) panels.push_back ({ firstGenUnit + (int) k, &textures.genDecal[k], genFiles[k].c_str() });
        // CUSTOM, when a design is loaded for the test (PAD_UI_TEST_CUSTOM): its print, from the share code
        static artwork::RawTexture customTex;
        if (juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_CUSTOM", {}).isNotEmpty())
        {
            customTex = artwork::renderDesignedDecal (customUnit, textures.faceplateDecal.width, nullptr);
            panels.push_back ({ customUnit, &customTex, "custom.png" });
        }
        juce::String printedReport;
        int printedProblems = 0;

        for (size_t pi = 0; pi < panels.size(); ++pi)
        {
            auto& panel = panels[pi];
            const bool printed = pi >= firstPrinted;
            if (panel.tex == nullptr || panel.tex->width <= 0) continue;
            const int unit = panel.unit;
            const float halfH = unitHalfH (unit);
            auto img = toImage (*panel.tex);
            const float halfW = unitHalfW (unit);
            const float sx = (float) img.getWidth() / (2.0f * halfW), sz = (float) img.getHeight() / (2.0f * halfH);
            auto px = [&] (float x) { return (x + halfW) * sx; };
            auto pz = [&] (float z) { return (z + halfH) * sz; };

            juce::Graphics g (img);
            const auto obs = printed ? printedObstacles (unit) : obstaclesFor (unit);

            g.setColour (juce::Colours::red.withAlpha (0.8f));
            for (auto& o : obs)
            {
                if (o.shape == Obstacle::circle)
                    g.drawEllipse (px (o.x - o.a), pz (o.z - o.a), 2.0f * o.a * sx, 2.0f * o.a * sz, 2.0f);
                else if (o.shape == Obstacle::ring)
                {
                    g.drawEllipse (px (o.x - o.a), pz (o.z - o.a), 2.0f * o.a * sx, 2.0f * o.a * sz, 1.0f);
                    g.drawEllipse (px (o.x - o.b), pz (o.z - o.b), 2.0f * o.b * sx, 2.0f * o.b * sz, 1.0f);
                }
                else
                    g.drawRect (juce::Rectangle<float>::leftTopRightBottom (px (o.x - o.a), pz (o.z - o.b), px (o.x + o.a), pz (o.z + o.b)), 2.0f);
            }

            juce::String& out = printed ? printedReport : report;
            out << "=== " << panel.file << " (" << unitInfo[(size_t) unit].name << ") ===\n";
            int problems = 0;

            std::vector<const artwork::TextItem*> mine;
            for (auto& t : items)
                if (t.unit == unit)
                    mine.push_back (&t);

            for (auto* t : mine)
            {
                const auto b = boxOf (*t);
                bool bad = false;

                for (auto& o : obs)
                {
                    // An engraved section title breaks the border it sits on, by design
                    if (isOutboard (unit) && o.name.endsWith ("border (top)") && std::abs (t->z - (oneUSectionBox (unit).minZ() + 0.004f)) < 1.0e-3f)
                        continue;

                    const float c = clearance (b, o);
                    if (c < margin)
                    {
                        out << juce::String::formatted ("  %-34s vs %-30s clearance %+.3f\n", t->text.substring (0, 34).toRawUTF8(), o.name.toRawUTF8(), c);
                        bad = true;
                    }
                }

                for (auto* other : mine)
                {
                    if (other <= t || (t->clarityScale >= 0 && other->clarityScale >= 0 && t->clarityScale != other->clarityScale))
                        continue;
                    const float c = boxGap (b, boxOf (*other));
                    if (c < margin * 0.6f)
                    {
                        out << juce::String::formatted ("  %-34s vs text '%s' clearance %+.3f\n", t->text.substring (0, 34).toRawUTF8(), other->text.substring (0, 30).toRawUTF8(), c);
                        bad = true;
                    }
                }

                problems += bad ? 1 : 0;
                g.setColour (bad ? juce::Colours::magenta : juce::Colours::lime.withAlpha (0.7f));
                g.drawRect (juce::Rectangle<float>::leftTopRightBottom (px (b.x0), pz (b.z0), px (b.x1), pz (b.z1)), 1.5f);
            }

            out << "  " << problems << (printed ? " tight / overlapping item(s)\n\n" : " cramped / overlapping print item(s)\n\n");
            if (printed) printedProblems += problems;
            juce::PNGImageFormat png;
            if (auto out = dir.getChildFile (panel.file).createOutputStream())
            {
                out->setPosition (0);
                out->truncate();
                png.writeImageToStream (img, *out);
            }
        }

        dir.getChildFile ("clearances.txt").replaceWithText (report);
        // (the designed and newer units in their own file: their count is held to its own limit)
        dir.getChildFile ("clearances-units.txt").replaceWithText (printedReport + juce::String (printedProblems) + " tight item(s) in all\n");
    }
}
