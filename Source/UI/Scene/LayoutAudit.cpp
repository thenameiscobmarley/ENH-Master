#include "LayoutAudit.h"
#include "DeviceLayout.h"
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
                addBorder (obs, oneUSectionBox (unit), "control section");
                for (int i = 0; i < numVus (unit); ++i)
                    obs.push_back ({ Obstacle::hole, vuX (unit, i), vuZ (unit, i), vuHalfW (unit) + 0.03f, vuHalfH + 0.03f, "VU bezel" });
                for (auto& w : outboardWindows (unit))
                {
                    obs.push_back ({ Obstacle::hole, w.cx, w.cz, w.hw + 0.034f, w.hd + 0.034f, "display bezel" });
                    obs.push_back ({ Obstacle::hole, w.cx, w.cz, w.hw, w.hd, "display window" });
                }
            }

            // The panel's own edge
            const float h = unitHalfH (unit);
            obs.push_back ({ Obstacle::hole, 0.0f, 0.0f, faceHalfW - 0.02f, h - 0.02f, "panel edge" });
            return obs;
        }

        juce::Image toImage (const artwork::RawTexture& t)
        {
            juce::Image img (juce::Image::RGB, t.width, t.height, true);
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

    void writeLayoutAudit (const artwork::TextureSet& textures, const artwork::TextRegistry& items, const juce::File& dir)
    {
        dir.createDirectory();
        juce::String report;
        constexpr float margin = 0.015f;   // closer than this (panel units) is reported as cramped

        struct Panel { int unit; const artwork::RawTexture* tex; const char* file; };
        std::vector<Panel> panels { { enhUnit, &textures.faceplateDecal, "enh.png" }, { tubeUnit, &textures.tubeDecal, "tube.png" },
                                    { tideUnit, &textures.tideDecal, "tide.png" }, { lumenUnit, &textures.lumenDecal, "lumen.png" },
                                    { limiterUnit, &textures.limiterDecal, "limiter.png" }, { levelUnit, &textures.levelDecal, "level.png" },
                                    { balancerUnit, &textures.balancerDecal, "balancer.png" } };

        for (auto& panel : panels)
        {
            const int unit = panel.unit;
            const float halfH = unitHalfH (unit);
            auto img = toImage (*panel.tex);
            const float sx = (float) img.getWidth() / (2.0f * faceHalfW), sz = (float) img.getHeight() / (2.0f * halfH);
            auto px = [&] (float x) { return (x + faceHalfW) * sx; };
            auto pz = [&] (float z) { return (z + halfH) * sz; };

            juce::Graphics g (img);
            const auto obs = obstaclesFor (unit);

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

            report << "=== " << panel.file << " (" << unitInfo[(size_t) unit].name << ") ===\n";
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
                        report << juce::String::formatted ("  %-34s vs %-30s clearance %+.3f\n", t->text.substring (0, 34).toRawUTF8(), o.name.toRawUTF8(), c);
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
                        report << juce::String::formatted ("  %-34s vs text '%s' clearance %+.3f\n", t->text.substring (0, 34).toRawUTF8(), other->text.substring (0, 30).toRawUTF8(), c);
                        bad = true;
                    }
                }

                problems += bad ? 1 : 0;
                g.setColour (bad ? juce::Colours::magenta : juce::Colours::lime.withAlpha (0.7f));
                g.drawRect (juce::Rectangle<float>::leftTopRightBottom (px (b.x0), pz (b.z0), px (b.x1), pz (b.z1)), 1.5f);
            }

            report << "  " << problems << " cramped / overlapping print item(s)\n\n";
            juce::PNGImageFormat png;
            if (auto out = dir.getChildFile (panel.file).createOutputStream())
            {
                out->setPosition (0);
                out->truncate();
                png.writeImageToStream (img, *out);
            }
        }

        dir.getChildFile ("clearances.txt").replaceWithText (report);
    }
}
