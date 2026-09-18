#include "PanelArtwork.h"
#include "DeviceLayout.h"
#include <string_view>

namespace pad::artwork
{
    using namespace layout;

    static void copyChannel (const juce::Image& img, RawTexture& dest, int channel)
    {
        const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);

        for (int y = 0; y < img.getHeight(); ++y)
        {
            auto* src = data.getLinePointer (y);
            auto* dst = dest.pixels.data() + (size_t) (y * dest.width * dest.channels + channel);

            for (int x = 0; x < img.getWidth(); ++x)
                dst[x * dest.channels] = src[x * data.pixelStride];
        }
    }

    static juce::Font makeFont (float heightPx, bool bold, float tracking = 0.0f, bool mono = false)
    {
        auto opts = juce::FontOptions().withHeight (heightPx).withKerningFactor (tracking);

        if (mono)
            opts = opts.withName (juce::Font::getDefaultMonospacedFontName());
        if (bold)
            opts = opts.withStyle ("Bold");

        return juce::Font (opts);
    }

    //==============================================================================
    namespace
    {
        /** Maps panel-local (x, z) to decal pixels. */
        struct PanelMapper
        {
            float sx, sz, halfH = faceHalfH;
            float px (float x) const { return (x + faceHalfW) * sx; }
            float pz (float z) const { return (z + halfH) * sz; }
            float len (float w) const { return w * sx; }

            juce::Rectangle<float> rect (float x0, float z0, float x1, float z1) const
            {
                return juce::Rectangle<float>::leftTopRightBottom (px (x0), pz (z0), px (x1), pz (z1));
            }
        };

        /** Where text() records what it prints (set by the render functions). */
        struct Recorder
        {
            TextRegistry* items = nullptr;
            int unit = 0, control = -1, clarityScale = -1;
        };
        Recorder recorder;

        void text (juce::Graphics& g, const PanelMapper& m, const juce::String& s, float x, float z, float height,
                   juce::Justification just, bool bold, float tracking, float boxWidth)
        {
            const auto font = makeFont (m.len (height), bold, tracking);
            g.setFont (font);

            float x0 = x;
            if (just.testFlags (juce::Justification::horizontallyCentred)) x0 = x - boxWidth * 0.5f;
            else if (just.testFlags (juce::Justification::right))          x0 = x - boxWidth;

            if (recorder.items != nullptr && s.trim().isNotEmpty())
            {
                const float width = juce::GlyphArrangement::getStringWidth (font, s) / m.sx;
                float centre = x0 + 0.5f * width;
                if (just.testFlags (juce::Justification::horizontallyCentred)) centre = x;
                else if (just.testFlags (juce::Justification::right))          centre = x - 0.5f * width;

                recorder.items->push_back ({ recorder.unit, centre, z, 0.5f * width, 0.6f * height, s.trim(),
                                             recorder.control, recorder.clarityScale });
            }

            g.drawText (s, m.rect (x0, z - height, x0 + boxWidth, z + height),
                        juce::Justification (just.getOnlyHorizontalFlags() | juce::Justification::verticallyCentred), false);
        }
    }

    RawTexture renderFaceplateDecal (int textureWidth, TextRegistry* registry)
    {
        recorder = { registry, enhUnit, -1, -1 };
        const int w = textureWidth;
        const int h = juce::roundToInt ((float) textureWidth * faceHalfH / faceHalfW);
        const PanelMapper m { (float) w / (2.0f * faceHalfW), (float) h / (2.0f * faceHalfH) };

        juce::Image ink (juce::Image::SingleChannel, w, h, true);    // white silkscreen
        juce::Image lines (juce::Image::SingleChannel, w, h, true);  // grey section outlines
        juce::Image fills (juce::Image::SingleChannel, w, h, true);  // slightly lighter section fields

        const auto solid = juce::Colours::white;
        const auto centred = juce::Justification::horizontallyCentred;
        const auto left = juce::Justification::left;

        {
            juce::Graphics g (ink);
            g.setColour (solid);

            // Logo + product line, top left above the display
            text (g, m, "ENH MASTER", displayRect.minX() - 0.02f, -0.565f, 0.070f, left, true, 0.22f, 1.6f);
            text (g, m, "ADAPTIVE CLARITY PROCESSOR", displayRect.minX() - 0.02f, 0.575f, 0.030f, left, true, 0.20f, 1.4f);
            text (g, m, "EM-4", 2.20f, 0.575f, 0.030f, juce::Justification::right, true, 0.20f, 0.4f);

            for (auto& sec : sections)
                text (g, m, sec.title, sec.box.cx, sectionTitleZ, 0.044f, centred, true, 0.26f, sec.box.hw * 2.0f);

            for (auto& c : controls)
            {
                if (c.unit != enhUnit)
                    continue;

                recorder.control = (int) (&c - controls.data());
                if (c.size < 0.99f)   // small master knobs: label right under the knob
                    text (g, m, c.label, c.x, c.z + 0.215f, 0.028f, centred, true, 0.12f, 0.46f);
                else
                    text (g, m, c.label, c.x, labelZ, 0.038f, centred, true, 0.14f, 0.46f);
                recorder.control = -1;

                if (c.kind == ControlKind::button)
                {
                    if (std::string_view (c.paramId) == pad::params::id::clarityMode)
                    {
                        text (g, m, "NORM", c.x - modeLedDx, c.z + buttonLedDz - 0.075f, 0.024f, centred, true, 0.10f, 0.2f);
                        text (g, m, "ADD",  c.x + modeLedDx, c.z + buttonLedDz - 0.075f, 0.024f, centred, true, 0.10f, 0.2f);
                    }
                    else
                    {
                        text (g, m, "ON", c.x, c.z + buttonLedDz - 0.075f, 0.024f, centred, true, 0.10f, 0.2f);
                    }
                }
            }

            // Ladder captions and OUT dB marks
            for (auto* ladder : { &detectLadder, &outLadder, &enhLadder })
                text (g, m, ladder->label, ladder->x, labelZ, 0.036f, centred, true, 0.12f, 0.3f);

            for (int i = 0; i < outLadder.segments; ++i)
                if (i % 2 == 1 || i == 0)
                    text (g, m, juce::String (outLadderDb[(size_t) i]), outLadder.x - 0.045f, ladderLedZ (i), 0.020f,
                          juce::Justification::right, false, 0.0f, 0.12f);

            text (g, m, "MAX", enhLadder.x + 0.035f, ladderLedZ (enhLadder.segments - 1), 0.018f, left, true, 0.0f, 0.12f);
            text (g, m, "STEP", detectLadder.x + 0.035f, ladderLedZ (detectLadder.segments - 1), 0.018f, left, true, 0.0f, 0.12f);
        }

        {
            juce::Graphics g (lines);
            g.setColour (solid);
            const float t = m.len (0.006f);

            for (auto& sec : sections)
            {
                const auto r = m.rect (sec.box.minX(), sec.box.minZ(), sec.box.maxX(), sec.box.maxZ());
                g.drawRoundedRectangle (r, m.len (0.018f), t);

                // Rule under the section title
                g.fillRect (m.rect (sec.box.minX() + 0.03f, sectionTitleZ + 0.045f, sec.box.maxX() - 0.03f, sectionTitleZ + 0.045f + 0.004f));
            }

            // Rule under the logo
            g.fillRect (m.rect (displayRect.minX() - 0.02f, -0.505f, displayRect.maxX() + 0.05f, -0.500f));
        }

        {
            juce::Graphics g (fills);
            g.setColour (solid);
            for (auto& sec : sections)
                g.fillRoundedRectangle (m.rect (sec.box.minX(), sec.box.minZ(), sec.box.maxX(), sec.box.maxZ()), m.len (0.018f));
        }

        recorder = {};
        RawTexture tex { w, h, 4, {} };
        tex.pixels.assign ((size_t) (w * h * 4), 0);
        copyChannel (ink, tex, 0);
        copyChannel (lines, tex, 1);
        copyChannel (fills, tex, 2);
        return tex;
    }

    //==============================================================================
    RawTexture renderTubeDecal (int textureWidth, TextRegistry* registry)
    {
        recorder = { registry, tubeUnit, -1, -1 };
        const int w = textureWidth;
        const int h = juce::roundToInt ((float) textureWidth * tubeHalfH / faceHalfW);
        const PanelMapper m { (float) w / (2.0f * faceHalfW), (float) h / (2.0f * tubeHalfH), tubeHalfH };

        juce::Image ink (juce::Image::SingleChannel, w, h, true);
        juce::Graphics g (ink);
        g.setColour (juce::Colours::white);
        const auto centred = juce::Justification::horizontallyCentred;
        const auto left = juce::Justification::left;

        // Maker panel, top left
        text (g, m, "SERAPH", -2.22f, -0.445f, 0.058f, left, true, 0.20f, 0.60f);
        text (g, m, "CELESTIAL PROCESSOR", -2.22f, -0.375f, 0.019f, left, true, 0.12f, 0.32f);
        text (g, m, "SR 2  SILK + HALO", -2.22f, -0.335f, 0.019f, left, true, 0.12f, 0.32f);

        auto valueText = [] (float v)
        {
            const float a = std::abs (v);
            if (a >= 10.0f || a == 0.0f || std::abs (v - std::round (v)) < 1.0e-3f)
                return juce::String (juce::roundToInt (v));
            return juce::String (v, a < 0.1f ? 2 : 1).trimCharactersAtStart ("0");
        };

        for (auto& c : controls)
        {
            if (c.unit != tubeUnit)
                continue;

            recorder.control = (int) (&c - controls.data());
            if (c.kind == ControlKind::knob)
                text (g, m, c.label, c.x, c.z + 0.175f, 0.024f, centred, true, 0.14f, 0.40f);
            else if (c.kind == ControlKind::toggle)
                text (g, m, c.label, c.x, c.z + 0.118f, 0.020f, centred, true, 0.10f, 0.30f);
            recorder.control = -1;

            if (c.kind == ControlKind::knob)
            {
                // Scale: 11 ticks, min / middle / max values from the parameter range
                const float r = knobRadius * tubeKnobScale * c.size;
                for (int i = 0; i <= 10; ++i)
                {
                    const float angle = knobAngleForValue ((float) i / 10.0f);
                    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
                    const float r0 = r + 0.028f, r1 = r + (i % 5 == 0 ? 0.060f : 0.048f);
                    g.drawLine (juce::Line<float> (m.px (c.x + dir.x * r0), m.pz (c.z + dir.y * r0),
                                                   m.px (c.x + dir.x * r1), m.pz (c.z + dir.y * r1)),
                                m.len (i % 5 == 0 ? 0.008f : 0.005f));
                }

                if (auto* spec = pad::params::findSpec (c.paramId))
                {
                    juce::NormalisableRange<float> range (spec->minValue, spec->maxValue);
                    if (spec->skewCentre > 0.0f)
                        range.setSkewForCentre (spec->skewCentre);

                    for (float t : { 0.0f, 1.0f })
                    {
                        const float angle = knobAngleForValue (t);
                        const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
                        const float rr = r + 0.085f;
                        text (g, m, valueText (range.convertFrom0to1 (t)), c.x + dir.x * rr, c.z + dir.y * rr, 0.019f,
                              centred, true, 0.0f, 0.14f);
                    }
                }
            }
            else if (c.kind == ControlKind::selector)
            {
                const juce::StringArray names { "OFF", "SILK", "HEAVEN" };
                for (int k = 0; k < 3; ++k)
                {
                    const float angle = selectorAngleForValue ((float) k * 0.5f);
                    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
                    const float rr = knobRadius * tubeKnobScale + 0.07f;
                    text (g, m, names[k], c.x + dir.x * rr, c.z + dir.y * rr, 0.020f, centred, true, 0.08f, 0.16f);
                    g.fillEllipse (m.rect (c.x + dir.x * (rr - 0.045f) - 0.008f, c.z + dir.y * (rr - 0.045f) - 0.008f,
                                           c.x + dir.x * (rr - 0.045f) + 0.008f, c.z + dir.y * (rr - 0.045f) + 0.008f));
                }
            }
        }

        // Toggle grid caption ("up = on")
        text (g, m, "UP = ON", 1.58f, -0.485f, 0.017f, centred, true, 0.12f, 0.4f);

        recorder = {};
        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (ink, tex, 0);
        return tex;
    }

    /** TIDE and LUMEN: engraved print on a brushed plate - maker panel, a bordered control
        section, knob labels with numbered scales, and what the meters are showing. */
    RawTexture renderOneUDecal (int unit, int textureWidth, TextRegistry* registry)
    {
        recorder = { registry, unit, -1, -1 };
        const int w = textureWidth;
        const int h = juce::roundToInt ((float) textureWidth * oneUHalfH / faceHalfW);
        const PanelMapper m { (float) w / (2.0f * faceHalfW), (float) h / (2.0f * oneUHalfH), oneUHalfH };

        juce::Image ink (juce::Image::SingleChannel, w, h, true);
        juce::Graphics g (ink);
        g.setColour (juce::Colours::white);
        const auto centred = juce::Justification::horizontallyCentred;
        const auto left = juce::Justification::left;

        const auto& info = unitInfo[(size_t) unit];

        // Maker panel at the left edge, engraved: name, what it is, where it sits in the chain
        text (g, m, info.name, -2.34f, -0.120f, 0.078f, left, true, 0.20f, 0.60f);
        text (g, m, info.role, -2.34f, -0.038f, 0.020f, left, true, 0.13f, 0.34f);
        text (g, m, juce::String ("STAGE ") + juce::String (info.chainPosition) + " OF 4  -  ENH MASTER SERIES",
              -2.34f, 0.022f, 0.017f, left, true, 0.12f, 0.30f);

        // Bordered control section, as on a hardware compressor
        {
            const auto& b = oneUSectionBox;
            g.drawRoundedRectangle (m.rect (b.minX(), b.minZ(), b.maxX(), b.maxZ()), m.len (0.022f), m.len (0.006f));

            const auto title = unit == tideUnit ? juce::String ("COMPRESSOR") : juce::String ("LEVELER");
            const auto font = makeFont (m.len (0.024f), true, 0.30f);
            const float tw = juce::GlyphArrangement::getStringWidth (font, title);
            // Break the border where the title sits, the way engraved panels do
            g.setColour (juce::Colours::black);
            g.fillRect (juce::Rectangle<float> (m.px (b.cx) - 0.5f * tw - m.len (0.02f), m.pz (b.minZ()) - m.len (0.012f),
                                                tw + m.len (0.04f), m.len (0.024f)));
            g.setColour (juce::Colours::white);
            text (g, m, title, b.cx, b.minZ() + 0.004f, 0.024f, centred, true, 0.30f, 0.0f);
        }

        auto valueText = [] (float v)
        {
            const float a = std::abs (v);
            if (a >= 10.0f || a == 0.0f || std::abs (v - std::round (v)) < 1.0e-3f)
                return juce::String (juce::roundToInt (v));
            return juce::String (v, a < 0.1f ? 2 : 1).trimCharactersAtStart ("0");
        };

        for (auto& c : controls)
        {
            if (c.unit != unit)
                continue;

            recorder.control = (int) (&c - controls.data());
            text (g, m, c.label, c.x, c.z + (c.kind == ControlKind::knob ? 0.182f : 0.140f), 0.026f, centred, true, 0.16f, 0.40f);
            recorder.control = -1;

            if (c.kind != ControlKind::knob)
                continue;

            const float r = oneUKnobRadius * c.size * 1.26f;    // outside the skirt
            for (int i = 0; i <= 10; ++i)
            {
                const float angle = knobAngleForValue ((float) i / 10.0f);
                const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
                const float r0 = r + 0.012f, r1 = r + (i % 5 == 0 ? 0.040f : 0.028f);
                g.drawLine (juce::Line<float> (m.px (c.x + dir.x * r0), m.pz (c.z + dir.y * r0),
                                               m.px (c.x + dir.x * r1), m.pz (c.z + dir.y * r1)),
                            m.len (i % 5 == 0 ? 0.008f : 0.004f));
            }

            if (auto* spec = pad::params::findSpec (c.paramId))
            {
                juce::NormalisableRange<float> range (spec->minValue, spec->maxValue);
                if (spec->skewCentre > 0.0f)
                    range.setSkewForCentre (spec->skewCentre);

                for (int i = 0; i <= 4; ++i)
                {
                    const float t = (float) i / 4.0f;
                    const float angle = knobAngleForValue (t);
                    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
                    const float rr = r + 0.062f;
                    text (g, m, valueText (range.convertFrom0to1 (t)), c.x + dir.x * rr, c.z + dir.y * rr, 0.022f,
                          centred, true, 0.0f, 0.13f);
                }
            }
        }

        // What each meter is reading, engraved under its bezel
        for (int i = 0; i < numVus (unit); ++i)
        {
            const char* names[3] { "LOW", "MID", "HIGH" };
            const auto label = unit == tideUnit ? juce::String ("GAIN REDUCTION") : juce::String (names[i]);
            text (g, m, label, vuX (unit, i), vuCentreZ + vuHalfH + 0.062f, 0.023f, centred, true, 0.20f, 0.36f);
        }

        recorder = {};
        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (ink, tex, 0);
        return tex;
    }

    /*  A VU dial face, drawn from the same geometry the movement is built with (hwk::models::
        vuPivotDrop / vuArcRadius), so the needle tip tracks the printed arc exactly.
        R = ink, G = the red zone the meter material tints. */
    RawTexture renderVuFace (int unit, int width, TextRegistry* registry)
    {
        const float halfW = vuHalfW (unit);
        const int w = width, h = juce::jmax (8, juce::roundToInt ((float) width * vuHalfH / halfW));
        juce::Image ink (juce::Image::SingleChannel, w, h, true);
        juce::Image red (juce::Image::SingleChannel, w, h, true);

        const bool tide = unit == tideUnit;
        const float scale = (float) w / (2.0f * halfW);           // pixels per panel unit
        const juce::Point<float> pivot (0.5f * (float) w, (vuHalfH + vuHalfH * hwk::models::vuPivotDrop) * scale);
        const float arcR = vuHalfH * hwk::models::vuArcRadius * scale;
        const float sweep = hwk::models::vuSweep;

        auto pointAt = [&] (float t, float rFraction)
        {
            const float a = (t - 0.5f) * sweep;
            const float r = arcR * rFraction;
            return juce::Point<float> (pivot.x + std::sin (a) * r, pivot.y - std::cos (a) * r);
        };

        auto band = [&] (juce::Graphics& g, float from, float to, float rIn, float rOut)
        {
            juce::Path p;
            p.startNewSubPath (pointAt (from, rOut));
            for (float t = from; t <= to + 1.0e-4f; t += 0.01f)
                p.lineTo (pointAt (std::min (t, to), rOut));
            for (float t = to; t >= from - 1.0e-4f; t -= 0.01f)
                p.lineTo (pointAt (std::max (t, from), rIn));
            p.closeSubPath();
            g.fillPath (p);
        };

        {
            juce::Graphics g (red);
            g.setColour (juce::Colours::white);
            band (g, tide ? 0.70f : 0.75f, 1.0f, 0.955f, 1.015f);   // red zone over the top of the scale
        }

        juce::Graphics g (ink);
        g.setColour (juce::Colours::white);

        // The scale arc itself
        {
            juce::Path arc;
            arc.startNewSubPath (pointAt (0.0f, 1.0f));
            for (float t = 0.0f; t <= 1.0001f; t += 0.01f)
                arc.lineTo (pointAt (std::min (t, 1.0f), 1.0f));
            g.strokePath (arc, juce::PathStrokeType (scale * 0.004f));
        }

        // Ticks, longer where a number is printed
        const int majors = tide ? 4 : 3;
        for (int i = 0; i <= majors * 2; ++i)
        {
            const float t = (float) i / (float) (majors * 2);
            const bool major = (i % 2) == 0;
            g.drawLine (juce::Line<float> (pointAt (t, 1.0f), pointAt (t, major ? 0.915f : 0.950f)),
                        scale * (major ? 0.0065f : 0.0035f));
        }

        // Numbers inside the arc
        const auto font = makeFont (vuHalfH * 0.30f * scale, true, 0.05f);
        g.setFont (font);
        for (int i = 0; i <= majors; ++i)
        {
            const float t = (float) i / (float) majors;
            const auto label = juce::String (juce::roundToInt (t * (tide ? 12.0f : 18.0f)));
            const auto at = pointAt (t, 0.845f);
            const float tw = juce::GlyphArrangement::getStringWidth (font, label);
            g.drawText (label, juce::Rectangle<float> (at.x - 0.5f * tw - 2.0f, at.y - font.getHeight() * 0.5f,
                                                       tw + 4.0f, font.getHeight()),
                        juce::Justification::centred, false);
        }

        // Caption low on the card, where the needle never covers it
        const auto caption = tide ? juce::String ("GAIN REDUCTION   dB") : juce::String ("LIFT   dB");
        const auto capFont = makeFont (vuHalfH * 0.24f * scale, true, 0.22f);
        g.setFont (capFont);
        g.drawText (caption, juce::Rectangle<float> (0.0f, (float) h * 0.66f, (float) w, (float) h * 0.24f),
                    juce::Justification::centred, false);

        if (registry != nullptr)
            for (int i = 0; i < numVus (unit); ++i)
                registry->push_back ({ unit, vuX (unit, i), vuCentreZ + vuHalfH * 0.52f,
                                       vuHalfW (unit) * 0.6f, vuHalfH * 0.14f, caption, -1, -1 });

        RawTexture tex { w, h, 4, {} };
        tex.pixels.assign ((size_t) (w * h * 4), 0);
        copyChannel (ink, tex, 0);
        copyChannel (red, tex, 1);
        return tex;
    }

    RawTexture renderSeraphDisplayLabels (int width, TextRegistry* registry)
    {
        const auto& d = seraphDisplayRect;
        const int w = width, h = juce::roundToInt ((float) width * d.hd / d.hw);
        juce::Image img (juce::Image::SingleChannel, w, h, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);

        // Label at (u, v) in display uv; recorded in panel coordinates for the hover callouts
        auto label = [&] (const juce::String& t, float u, float v, float heightPx, juce::Justification just, const juce::String& explain)
        {
            const auto font = makeFont (heightPx, true, 0.08f);
            g.setFont (font);
            const float tw = juce::GlyphArrangement::getStringWidth (font, t);
            float x0 = u * (float) w;
            if (just == juce::Justification::centred) x0 -= 0.5f * tw;
            else if (just == juce::Justification::right) x0 -= tw;
            g.drawText (t, juce::Rectangle<float> (x0 - 2.0f, v * (float) h - heightPx, tw + 4.0f, 2.0f * heightPx), juce::Justification::centred, false);

            if (registry != nullptr)
                registry->push_back ({ tubeUnit, d.minX() + (x0 + 0.5f * tw) / (float) w * 2.0f * d.hw, d.minZ() + v * 2.0f * d.hd,
                                       0.5f * tw / (float) w * 2.0f * d.hw, 0.6f * heightPx / (float) h * 2.0f * d.hd,
                                       explain.isNotEmpty() ? explain : t });
        };

        const float small = (float) h * 0.060f, tiny = (float) h * 0.050f;

        // Left: where SMOOTH is dipping resonances right now
        label ("SMOOTH  -  RESONANCE DIPS", displayDipsU0, 0.085f, small, juce::Justification::left,
               "SMOOTH: where resonances are being dipped right now (shared by L + R)");
        for (auto [hz, name] : { std::pair { 150.0f, "150" }, std::pair { 1000.0f, "1k" }, std::pair { 4000.0f, "4k" }, std::pair { 16000.0f, "16k" } })
        {
            const float t = std::log (hz / 150.0f) / std::log (16000.0f / 150.0f);
            label (name, displayDipsU0 + t * (displayDipsU1 - displayDipsU0), 0.93f, tiny, juce::Justification::centred,
                   juce::String (name) + " Hz");
        }
        label ("0", displayDipsU0 - 0.012f, 0.30f, tiny, juce::Justification::right, "0 dB: no dip");
        label ("-12", displayDipsU0 - 0.012f, 0.80f, tiny, juce::Justification::right, "-12 dB dip");

        // Right: what every process is doing to each channel
        label ("LIVE  -  WHAT SERAPH IS DOING TO EACH CHANNEL", displayColsU0, 0.085f, small, juce::Justification::left, {});
        const std::array<std::pair<const char*, const char*>, displayColumns> columns {{
            { "SMOOTH",  "SMOOTH: energy of the resonance dips on L / R" },
            { "AIR",     "AIR: high shelf + generated highs added on L / R" },
            { "WARMTH",  "WARMTH: low-mid harmonics + tube curve on L / R" },
            { "BODY",    "BODY: low-mid fullness added on L / R" },
            { "TAPE",    "TAPE: transient softening on L / R" },
            { "LEVEL",   "LEVEL: AUTO loudness match + OUTPUT (centre = 0 dB)" },
            { "WIDTH",   "WIDTH: side change on L / R" },
            { "SPACE",   "SPACE: reverb tail added to L / R" },
            { "SHIMMER", "SHIMMER: octave-up shimmer inside the tail on L / R" },
        }};
        const float colW = (displayColsU1 - displayColsU0) / (float) displayColumns;
        for (int c = 0; c < displayColumns; ++c)
        {
            const float cu = displayColsU0 + ((float) c + 0.5f) * colW;
            label (columns[(size_t) c].first, cu, 0.93f, tiny, juce::Justification::centred, columns[(size_t) c].second);
            label ("L", cu - 0.24f * colW, 0.845f, tiny * 0.85f, juce::Justification::centred, "left channel");
            label ("R", cu + 0.24f * colW, 0.845f, tiny * 0.85f, juce::Justification::centred, "right channel");
        }

        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (img, tex, 0);
        return tex;
    }

    void collectKnobScaleText (TextRegistry& items)
    {
        // Same geometry as renderKnobScale: numbers at 0.212 of a 0.25 ring, scaled to the real ring and knob size

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            if (c.kind != ControlKind::knob || c.unit != enhUnit)
                continue;

            const float r = 0.212f * (scaleOuter / 0.25f) * c.size;
            const auto* spec = pad::params::findSpec (c.paramId);
            const int rangeMax = spec != nullptr ? juce::roundToInt (spec->maxValue) : 10;

            for (int scale = 0; scale < (c.altParamId != nullptr ? 2 : 1); ++scale)
            {
                const bool wide = c.altParamId != nullptr && scale == 0;   // CLARITY: NORM prints 0-30
                const int maxValue = wide ? 30 : rangeMax <= 5 ? rangeMax : 10;
                const int step = wide ? 5 : maxValue <= 5 ? 1 : 2;
                for (int v = 0; v <= maxValue; v += step)
                {
                    const float angle = knobAngleForValue ((float) v / (float) maxValue);
                    items.push_back ({ enhUnit, c.x + std::sin (angle) * r, c.z - std::cos (angle) * r, 0.03f * c.size, 0.02f * c.size,
                                       juce::String (v) + (maxValue == 3 && v == 3 ? "x" : ""), -1, c.altParamId != nullptr ? scale : -1 });
                }
            }
        }
    }

    RawTexture renderCallout (const juce::String& title, const juce::String& detail, float pixelScale)
    {
        const float ps = std::max (1.0f, pixelScale);
        const auto titleFont = makeFont (26.0f * ps, true, 0.04f);
        const auto detailFont = makeFont (20.0f * ps, false, 0.02f);
        const float pad = 14.0f * ps;

        const float tw = juce::GlyphArrangement::getStringWidth (titleFont, title);
        const float dw = detail.isNotEmpty() ? juce::GlyphArrangement::getStringWidth (detailFont, detail) : 0.0f;
        const int w = juce::roundToInt (std::max (tw, dw) + 2.0f * pad + 2.0f);
        const int h = juce::roundToInt ((detail.isNotEmpty() ? 64.0f : 42.0f) * ps);

        juce::Image img (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g (img);
            const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (1.5f * ps);
            g.setColour (juce::Colour (0xf0101014));
            g.fillRoundedRectangle (bounds, 8.0f * ps);
            g.setColour (juce::Colour (0xffe8e8ee).withAlpha (0.85f));
            g.drawRoundedRectangle (bounds, 8.0f * ps, 1.5f * ps);

            g.setColour (juce::Colours::white);
            g.setFont (titleFont);
            g.drawText (title, juce::Rectangle<float> (pad, 6.0f * ps, (float) w - 2.0f * pad, 32.0f * ps), juce::Justification::centredLeft, false);

            if (detail.isNotEmpty())
            {
                g.setColour (juce::Colour (0xffffc861));
                g.setFont (detailFont);
                g.drawText (detail, juce::Rectangle<float> (pad, 36.0f * ps, (float) w - 2.0f * pad, 24.0f * ps), juce::Justification::centredLeft, false);
            }
        }

        // Straight RGBA (un-premultiplied) for GL blending
        RawTexture tex { w, h, 4, {} };
        tex.pixels.assign ((size_t) (w * h * 4), 0);
        const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto c = data.getPixelColour (x, y);
                auto* d = tex.pixels.data() + (size_t) ((y * w + x) * 4);
                d[0] = c.getRed(); d[1] = c.getGreen(); d[2] = c.getBlue(); d[3] = c.getAlpha();
            }
        return tex;
    }

    //==============================================================================
    RawTexture renderKnobScale (int size, int maxValue)
    {
        juce::Image img (juce::Image::SingleChannel, size, size, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);

        const float scale = (float) size / (2.0f * scaleOuter) * (scaleOuter / 0.25f);   // layout drawn for a 0.25 ring
        const juce::Point<float> centre ((float) size * 0.5f, (float) size * 0.5f);
        // 0-30 by 5, 0-10 by 2, small ranges (0-3x, 0-5) by 1 with half steps between
        const bool wide = maxValue > 10, small = maxValue <= 5;
        const int numberStep = wide ? 5 : small ? 1 : 2;
        const int ticks = wide ? 30 : small ? maxValue * 4 : 20;
        const int majorEvery = wide ? 5 : small ? 4 : 2;

        // Fixed printed scale: value v sits where the pointer points at v
        g.setFont (makeFont (0.036f * scale, true));
        for (int i = 0; i <= maxValue; i += numberStep)
        {
            const float angle = knobAngleForValue ((float) i / (float) maxValue);
            const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
            const auto box = juce::Rectangle<float> (0.09f * scale, 0.05f * scale).withCentre (centre + dir * (0.212f * scale));
            g.drawText (juce::String (i) + (maxValue == 3 && i == maxValue ? "x" : ""), box, juce::Justification::centred, false);
        }

        for (int i = 0; i <= ticks; ++i)
        {
            const float angle = knobAngleForValue ((float) i / (float) ticks);
            const bool major = i % majorEvery == 0;
            const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
            const float r0 = (major ? 0.138f : 0.144f) * scale, r1 = (major ? 0.172f : 0.160f) * scale;
            g.drawLine ({ centre + dir * r0, centre + dir * r1 }, (major ? 0.009f : 0.005f) * scale);
        }

        RawTexture tex { size, size, 1, {} };
        tex.pixels.assign ((size_t) (size * size), 0);
        copyChannel (img, tex, 0);
        return tex;
    }

    //==============================================================================
    RawTexture renderDisplayOverlay (const DisplayText& t)
    {
        const int w = displayOverlayWidth, h = displayOverlayHeight;
        juce::Image img (juce::Image::SingleChannel, w, h, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);

        const float pad = 18.0f;

        g.setFont (makeFont (34.0f, true, 0.02f, true));
        g.drawText (t.title, juce::Rectangle<float> (pad, 8.0f, (float) w - 2.0f * pad, 42.0f), juce::Justification::centredLeft, false);
        g.drawText (t.tag,   juce::Rectangle<float> (pad, 8.0f, (float) w - 2.0f * pad, 42.0f), juce::Justification::centredRight, false);

        const float y = (float) h - 56.0f;
        g.setFont (makeFont (34.0f, true, 0.0f, true));

        g.drawText (t.focusLine.isNotEmpty() ? t.focusLine : t.lineLeft,
                    juce::Rectangle<float> (pad, y, (float) w - 2.0f * pad, 48.0f), juce::Justification::centredLeft, false);

        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (img, tex, 0);
        return tex;
    }
}
