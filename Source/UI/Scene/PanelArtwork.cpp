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

            // The box never clips: drawText drops whatever does not fit, which is how a line of
            // print used to lose its last word.
            boxWidth = std::max (boxWidth, juce::GlyphArrangement::getStringWidth (font, s) / m.sx + 0.01f);

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

    namespace
    {
        /** The largest height (up to `height`) at which `s` fits in `maxWidth` panel units. */
        float fitHeight (const PanelMapper& m, const juce::String& s, float height, float maxWidth, bool bold, float tracking)
        {
            const float w = juce::GlyphArrangement::getStringWidth (makeFont (m.len (height), bold, tracking), s) / m.sx;
            return w <= maxWidth ? height : height * maxWidth / w;
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

            // Maker block, right of the analyser window: what the unit is, then what it does. (It used
            // to sit at z -0.565 - under the window's cut-out - with its role line printed across the
            // MASTER section's lower knob.)
            {
                const float w = 2.0f * makerHalfW;
                auto line = [&] (const juce::String& s, float z, float h, float tracking)
                {
                    text (g, m, s, makerX, z, fitHeight (m, s, h, w, true, tracking), centred, true, tracking, w);
                };
                line ("ADAPTIVE", -0.655f, 0.052f, 0.16f);
                line ("ENHANCER", -0.585f, 0.052f, 0.16f);
                line (juce::String ("MODEL EM-") + juce::String (unitInfo[(size_t) enhUnit].chainPosition), -0.505f, 0.020f, 0.14f);   // a model number, as real units carry
                line ("PRESET", -0.325f, 0.022f, 0.30f);   // over the PREV / NEXT buttons
            }

            for (auto& sec : sections)
                text (g, m, sec.title, sec.box.cx, sectionTitleZ, 0.044f, centred, true, 0.26f, sec.box.hw * 2.0f);

            for (auto& c : controls)
            {
                if (c.unit != enhUnit)
                    continue;

                recorder.control = (int) (&c - controls.data());
                // Every label on the shared line; the small master knobs' labels a size down, so the
                // pair fits side by side inside MASTER. The PRESET buttons are labelled right under them.
                if (! hasLed (c))
                    text (g, m, c.label, c.x, c.z + 0.090f, 0.022f, centred, true, 0.16f, 0.20f);
                else if (c.size < 0.99f)
                    text (g, m, c.label, c.x, labelZ, 0.032f, centred, true, 0.10f, 0.30f);
                else
                    text (g, m, c.label, c.x, labelZ, 0.038f, centred, true, 0.14f, 0.46f);
                recorder.control = -1;

                if (c.kind == ControlKind::button && hasLed (c))
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

            // Rules in the maker block: under the name, and above the PRESET controls
            g.fillRect (m.rect (makerX - makerHalfW, -0.538f, makerX + makerHalfW, -0.534f));
            g.fillRect (m.rect (makerX - makerHalfW, -0.365f, makerX + makerHalfW, -0.361f));
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

        // Maker panel, top left: right of the lamp, left of the display window's bezel, above the
        // master knobs' scales
        {
            constexpr float x0 = -2.04f, width = seraphDisplayRect.minX() - 0.032f - 0.03f - x0;
            auto line = [&] (const juce::String& s, float z, float h, float tracking)
            {
                text (g, m, s, x0, z, fitHeight (m, s, h, width, true, tracking), left, true, tracking, width);
            };
            line ("TONE & SPACE", -0.745f, 0.050f, 0.16f);
            line (juce::String ("MODEL EM-") + juce::String (unitInfo[(size_t) tubeUnit].chainPosition), -0.675f, 0.020f, 0.12f);
        }

        // Row headings, so it is obvious which knobs belong to which section
        text (g, m, "TONE", -2.42f, seraphRow1Z - 0.20f, 0.026f, left, true, 0.34f, 0.0f);
        text (g, m, "SPACE", -2.42f, seraphRow2Z - 0.20f, 0.026f, left, true, 0.34f, 0.0f);

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

            // Labels sit a fixed gap below what they label: the knob's skirt, the toggle lever's reach
            // (0.13 either side of its nut), or the button's collar
            recorder.control = (int) (&c - controls.data());
            if (c.kind == ControlKind::knob)
                text (g, m, c.label, c.x, c.z + (c.size < 0.99f ? 0.155f : hwk::models::knob (c.style, knobBodyRadius (c), {}, 0).footprintRadius + 0.052f),
                      c.size < 0.99f ? 0.022f : 0.024f, centred, true, 0.14f, 0.40f);
            else if (c.kind == ControlKind::toggle)
                text (g, m, c.label, c.x, c.z + switchOutline (c.switchStyle).halfD + 0.050f, 0.020f, centred, true, 0.10f, 0.30f);
            else if (c.kind == ControlKind::button)
                text (g, m, c.label, c.x, c.z + 0.105f, 0.022f, centred, true, 0.14f, 0.30f);
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

                // Numbers at every major tick. A mode-switched knob prints both of its scales,
                // and the renderer cross-fades between them when the mode changes.
                for (int pass = 0; pass < (c.altParamId != nullptr ? 2 : 1); ++pass)
                {
                    const char* id = pass == 0 ? c.paramId : c.altParamId;
                    auto* spec = pad::params::findSpec (id);
                    if (spec == nullptr)
                        continue;

                    juce::NormalisableRange<float> range (spec->minValue, spec->maxValue);
                    if (spec->skewCentre > 0.0f)
                        range.setSkewForCentre (spec->skewCentre);

                    recorder.clarityScale = c.altParamId != nullptr ? pass : -1;
                    const float ring = c.altParamId != nullptr ? (pass == 0 ? 0.086f : 0.140f) : 0.086f;

                    for (int i = 0; i <= 4; ++i)
                    {
                        const float t = (float) i / 4.0f;
                        const float angle = knobAngleForValue (t);
                        const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
                        const float rr = r + ring;
                        text (g, m, valueText (range.convertFrom0to1 (t)), c.x + dir.x * rr, c.z + dir.y * rr, 0.020f,
                              centred, true, 0.0f, 0.14f);
                    }
                    recorder.clarityScale = -1;
                }
            }
            else if (c.kind == ControlKind::selector)
            {
                // Positions printed beyond the reach of the chicken-head's beak (1.6x its body radius),
                // with a dot at each one for the beak to point at
                const juce::StringArray names { "OFF", "TONE", "+SPACE" };
                const float beak = knobBodyRadius (c) * 1.6f;
                for (int k = 0; k < 3; ++k)
                {
                    const float angle = selectorAngleForValue ((float) k * 0.5f);
                    const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
                    const float rr = beak + 0.055f, rd = beak + 0.016f;
                    text (g, m, names[k], c.x + dir.x * rr, c.z + dir.y * rr, 0.020f, centred, true, 0.08f, 0.16f);
                    g.fillEllipse (m.rect (c.x + dir.x * rd - 0.008f, c.z + dir.y * rd - 0.008f,
                                           c.x + dir.x * rd + 0.008f, c.z + dir.y * rd + 0.008f));
                }
            }
        }

        // Switch grid caption, above the grid - between its rockers and the POWER selector
        text (g, m, "I = ON", 1.78f, -0.215f, 0.017f, centred, true, 0.12f, 0.4f);

        recorder = {};
        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (ink, tex, 0);
        return tex;
    }

    /** The 1U units (ADAPTIVE COMPRESSOR, UPWARD LEVELER, SPECTRAL LIMITER): engraved print on a
        brushed plate - maker panel, a bordered control section, knob labels with numbered scales,
        and what the meters are showing. */
    RawTexture renderOneUDecal (int unit, int textureWidth, TextRegistry* registry)
    {
        recorder = { registry, unit, -1, -1 };
        const int w = textureWidth;
        const float halfH = unitHalfH (unit);
        const int h = juce::roundToInt ((float) textureWidth * halfH / faceHalfW);
        const PanelMapper m { (float) w / (2.0f * faceHalfW), (float) h / (2.0f * halfH), halfH };

        juce::Image ink (juce::Image::SingleChannel, w, h, true);
        juce::Graphics g (ink);
        g.setColour (juce::Colours::white);
        const auto centred = juce::Justification::horizontallyCentred;
        const auto left = juce::Justification::left;

        const auto& info = unitInfo[(size_t) unit];

        // Maker panel between the rack screw and the control section: the name on two lines (what the
        // unit is), what it does, and where it sits in the chain
        {
            const float width = oneUMakerX1 - oneUMakerX0;
            auto line = [&] (const juce::String& s, float z, float hgt, float tracking)
            {
                text (g, m, s, oneUMakerX0, z, fitHeight (m, s, hgt, width, true, tracking), left, true, tracking, width);
            };
            const juce::String name (info.name);
            const float top = makerTopZ (unit);
            line (name.upToFirstOccurrenceOf (" ", false, false), top, 0.050f, 0.16f);
            line (name.fromFirstOccurrenceOf (" ", false, false), top + 0.072f, 0.050f, 0.16f);
            line (juce::String ("MODEL EM-") + juce::String (info.chainPosition), top + 0.138f, 0.019f, 0.10f);
        }

        // Bordered control section, as on a hardware compressor
        const auto box = oneUSectionBox (unit);
        {
            g.drawRoundedRectangle (m.rect (box.minX(), box.minZ(), box.maxX(), box.maxZ()), m.len (0.022f), m.len (0.006f));

            const auto title = unit == tideUnit ? juce::String ("COMPRESSOR") : unit == lumenUnit ? juce::String ("LEVELER")
                             : unit == levelUnit ? juce::String ("LEVEL") : unit == balancerUnit ? juce::String ("BALANCE")
                             : unit == monitorUnit ? juce::String ("MONITOR")
                                                                                                   : juce::String ("DYNAMIC EQ");
            const auto font = makeFont (m.len (0.024f), true, 0.30f);
            const float tw = juce::GlyphArrangement::getStringWidth (font, title);
            // Break the border where the title sits, the way engraved panels do
            g.setColour (juce::Colours::black);
            g.fillRect (juce::Rectangle<float> (m.px (box.cx) - 0.5f * tw - m.len (0.02f), m.pz (box.minZ()) - m.len (0.012f),
                                                tw + m.len (0.04f), m.len (0.024f)));
            g.setColour (juce::Colours::white);
            text (g, m, title, box.cx, box.minZ() + 0.004f, 0.024f, centred, true, 0.30f, 0.0f);
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

            // Labels one line below the skirt and the lever's reach, above the section border
            recorder.control = (int) (&c - controls.data());
            const float labelDz = c.kind == ControlKind::knob
                                ? std::max (oneULabelDz, hwk::models::knob (c.style, knobBodyRadius (c), {}, 0).footprintRadius + 0.050f)
                                : oneULabelDz;   // a bigger knob's label moves down with its skirt
            text (g, m, c.label, c.x, c.z + labelDz, 0.026f, centred, true, 0.16f, 0.40f);
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
                    const float rr = r + 0.072f;   // clear of the major tick it names
                    text (g, m, valueText (range.convertFrom0to1 (t)), c.x + dir.x * rr, c.z + dir.y * rr, 0.022f,
                          centred, true, 0.0f, 0.13f);
                }
            }
        }

        // What each meter is reading, engraved under its bezel
        for (int i = 0; i < numVus (unit); ++i)
        {
            const char* bands[3] { "LOW", "MID", "HIGH" };
            const char* limits[2] { "SPECTRAL", "BROADBAND" };
            const char* loudness[2] { "MOMENTARY", "SHORT-TERM" };
            const auto label = unit == tideUnit ? juce::String ("GAIN REDUCTION")
                             : unit == limiterUnit ? juce::String (limits[i])
                             : unit == monitorUnit ? juce::String (loudness[i])
                             : unit == levelUnit ? juce::String ("INPUT") : juce::String (bands[i]);
            text (g, m, label, vuX (unit, i), vuZ (unit, i) + vuHalfH + 0.056f, 0.021f, centred, true, 0.20f, 0.36f);
        }

        // Under the display: what it shows
        if (unit == monitorUnit)
            text (g, m, "IN AGAINST OUT   -   WAVEFORM  AND  SPECTRUM", monitorDisplayRect.cx, monitorDisplayRect.maxZ() + 0.074f,
                  0.021f, centred, true, 0.20f, 1.2f);

        recorder = {};
        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (ink, tex, 0);
        return tex;
    }

    /*  A VU dial face, drawn from the same geometry the movement is built with (hwk::models::
        vuPivotDrop / vuArcRadius), so the needle tip tracks the printed arc exactly.
        R = ink, G = the red zone the meter material tints. */
    RawTexture renderVuFace (int unit, int width, TextRegistry* registry, int meter)
    {
        const float halfW = vuHalfW (unit);
        const int w = width, h = juce::jmax (8, juce::roundToInt ((float) width * vuHalfH / halfW));
        juce::Image ink (juce::Image::SingleChannel, w, h, true);
        juce::Image red (juce::Image::SingleChannel, w, h, true);

        // Full scale: the compressor's GR 0-12, the leveler's lift 0-18, the limiter's spectral cut 0-18
        // (RANGE + headroom protection) and its broadband protection 0-12
        const bool twelve = unit == tideUnit || (unit == limiterUnit && meter == 1);
        const bool lufs = unit == monitorUnit || unit == levelUnit;   // -40 .. 0 (LUFS on MONITOR, dBFS RMS on LEVEL's INPUT)
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
            band (g, lufs ? 0.775f : twelve ? 0.70f : 0.75f, 1.0f, 0.955f, 1.015f);   // red zone over the top of the scale (LUFS: above -9)
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
        const int majors = lufs || twelve ? 4 : 3;
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
            if (lufs && (i % 2) != 0)
                continue;   // -40, -20 and 0 only: the arc is too short for five numbers
            const float t = (float) i / (float) majors;
            const auto label = juce::String (juce::roundToInt (lufs ? -40.0f + 40.0f * t : t * (twelve ? 12.0f : 18.0f)));
            const auto at = pointAt (t, 0.845f);
            const float tw = juce::GlyphArrangement::getStringWidth (font, label);
            g.drawText (label, juce::Rectangle<float> (at.x - 0.5f * tw - 2.0f, at.y - font.getHeight() * 0.5f,
                                                       tw + 4.0f, font.getHeight()),
                        juce::Justification::centred, false);
        }

        // Caption low on the card, where the needle never covers it
        const auto caption = unit == tideUnit ? juce::String ("GAIN REDUCTION   dB")
                           : unit == limiterUnit ? juce::String (meter == 0 ? "CUT   dB" : "BROADBAND   dB")
                           : unit == monitorUnit ? juce::String ("LUFS") : unit == levelUnit ? juce::String ("INPUT   dBFS")
                                                 : juce::String ("LIFT   dB");
        const auto capFont = makeFont (vuHalfH * 0.24f * scale, true, 0.22f);
        g.setFont (capFont);
        g.drawText (caption, juce::Rectangle<float> (0.0f, (float) h * 0.66f, (float) w, (float) h * 0.24f),
                    juce::Justification::centred, false);

        if (registry != nullptr)
            for (int i = 0; i < numVus (unit); ++i)
                if ((unit != limiterUnit && unit != monitorUnit) || i == meter)   // meters with faces of their own
                    registry->push_back ({ unit, vuX (unit, i), vuZ (unit, i) + vuHalfH * 0.52f,
                                           vuHalfW (unit) * 0.6f, vuHalfH * 0.14f, caption, -1, -1 });

        RawTexture tex { w, h, 4, {} };
        tex.pixels.assign ((size_t) (w * h * 4), 0);
        copyChannel (ink, tex, 0);
        copyChannel (red, tex, 1);
        return tex;
    }

    /** Print inside the MONITOR and MIX BALANCER displays (R8, uv 0..1 across the window):
        MONITOR      - the waveform's dB marks, the spectrum's frequency axis, the legends, the readout;
        MIX BALANCER - the frequency axis, the dB axis. */
    RawTexture renderWindowLabels (int unit, int width, TextRegistry* registry, const juce::String& readout)
    {
        const auto& d = unit == monitorUnit ? monitorDisplayRect : balancerDisplayRect;
        const int w = width, h = juce::roundToInt ((float) width * d.hd / d.hw);
        juce::Image img (juce::Image::SingleChannel, w, h, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);
        const float px = (float) w / (2.0f * d.hw);   // pixels per panel unit

        auto label = [&] (const juce::String& s, float u, float v, float heightPanel, juce::Justification just)
        {
            const auto font = makeFont (heightPanel * px, true, 0.08f);
            g.setFont (font);
            const float tw = juce::GlyphArrangement::getStringWidth (font, s) + 4.0f;
            float x = u * (float) w;
            if (just.testFlags (juce::Justification::horizontallyCentred)) x -= 0.5f * tw;
            else if (just.testFlags (juce::Justification::right)) x -= tw;
            g.drawText (s, juce::Rectangle<float> (x, v * (float) h - 0.6f * font.getHeight(), tw, 1.2f * font.getHeight()),
                        juce::Justification::centred, false);
            if (registry != nullptr)
                registry->push_back ({ unit, d.minX() + (x + 0.5f * tw) / px, d.minZ() + v * 2.0f * d.hd,
                                       0.5f * tw / px, 0.6f * heightPanel, s, -1, -1 });
        };

        if (unit == monitorUnit)
        {
            // Top: the waveform (centre 0.27, +-0.20), marks at 0, -6 and -12 dB up its right edge
            for (int db : { 0, -6, -12 })
            {
                const float a = std::pow (10.0f, (float) db / 20.0f);
                label (juce::String (db), 0.99f, 0.27f - 0.20f * a, 0.026f, juce::Justification::right);
            }
            label ("IN  (PENCIL)     OUT  (INK)", 0.012f, 0.035f, 0.026f, juce::Justification::left);
            // Bottom: the spectrum (0.55 .. 0.90) and the tone change in red about its dotted line
            for (auto [hz, name] : { std::pair { 50.0f, "50" }, std::pair { 100.0f, "100" }, std::pair { 200.0f, "200" }, std::pair { 500.0f, "500" },
                                     std::pair { 1000.0f, "1k" }, std::pair { 2000.0f, "2k" }, std::pair { 5000.0f, "5k" }, std::pair { 10000.0f, "10k" } })
            {
                const float u = std::log (hz / 20.0f) / std::log (20000.0f / 20.0f);
                label (name, 0.02f + 0.96f * u, 0.918f, 0.026f, juce::Justification::horizontallyCentred);
            }
            label ("TONE CHANGE  (RED)   +-12 dB", 0.012f, 0.54f, 0.026f, juce::Justification::left);
            if (readout.isNotEmpty())
            {
                // The live readout along the bottom of the card (not a hover target: it changes)
                auto* keep = registry;
                registry = nullptr;
                label (readout, 0.5f, 0.965f, 0.028f, juce::Justification::horizontallyCentred);
                registry = keep;
            }
        }
        else
        {
            // Frequency axis along the bottom, dB up the right, as a FabFilter display prints them
            for (auto [hz, name] : { std::pair { 50.0f, "50" }, std::pair { 100.0f, "100" }, std::pair { 200.0f, "200" }, std::pair { 500.0f, "500" },
                                     std::pair { 1000.0f, "1k" }, std::pair { 2000.0f, "2k" }, std::pair { 5000.0f, "5k" }, std::pair { 10000.0f, "10k" } })
            {
                const float u = std::log (hz / 20.0f) / std::log (20000.0f / 20.0f);
                label (name, 0.02f + 0.96f * u, 0.675f, 0.032f, juce::Justification::horizontallyCentred);
            }
            for (int db : { 12, 6, 0, -6, -12 })
                label ((db > 0 ? "+" : "") + juce::String (db), 0.992f, 0.34f - (float) db / 12.0f * 0.28f, 0.030f, juce::Justification::right);
            label ("IN", 0.012f, 0.745f, 0.028f, juce::Justification::left);
            label ("OUT", 0.012f, 0.965f, 0.028f, juce::Justification::left);
        }

        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (img, tex, 0);
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
            const auto font = makeFont (heightPx, true, 0.12f);
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

        /*  Vertical plan (display v): headings 0.10, plots 0.17-0.72, L / R 0.775, names 0.865. The
            window is recessed and this unit sits above eye level, so the lowest strip of the glass is
            hidden by the recess wall: nothing is printed below v 0.90. */
        const float small = (float) h * 0.062f, tiny = (float) h * 0.056f;

        // Left: where SMOOTH is dipping resonances right now
        label ("SMOOTH", displayDipsU0, 0.10f, small, juce::Justification::left,
               "SMOOTH: where resonances are being dipped right now (shared by L + R)");
        for (auto [hz, name] : { std::pair { 150.0f, "150" }, std::pair { 1000.0f, "1k" }, std::pair { 4000.0f, "4k" }, std::pair { 16000.0f, "16k" } })
        {
            const float t = std::log (hz / 150.0f) / std::log (16000.0f / 150.0f);
            label (name, displayDipsU0 + t * (displayDipsU1 - displayDipsU0), 0.865f, tiny, juce::Justification::centred,
                   juce::String (name) + " Hz");
        }
        label ("0", displayDipsU0 - 0.004f, 0.30f, tiny, juce::Justification::right, "0 dB: no dip");
        label ("-12", displayDipsU0 - 0.004f, 0.72f, tiny, juce::Justification::right, "-12 dB dip");

        // Right: what every process is doing to each channel
        label ("L / R", displayColsU0, 0.10f, small, juce::Justification::left,
               "What each process is doing to the left and right channel, right now");
        const std::array<std::pair<const char*, const char*>, displayColumns> columns {{
            { "SMOOTH",  "SMOOTH: energy of the resonance dips on L / R" },
            { "AIR",     "AIR: high shelf + generated highs added on L / R" },
            { "WARMTH",  "WARMTH: low-mid harmonics + tube curve on L / R" },
            { "BODY",    "BODY: low-mid fullness added on L / R" },
            { "TAPE",    "TAPE: transient softening on L / R" },
            { "LEVEL",   "LEVEL: AUTO loudness match + LOUDNESS + OUTPUT (centre = 0 dB)" },
            { "WIDTH",   "WIDTH: side change on L / R" },
            { "REVERB",  "REVERB: reverb tail added to L / R" },
            { "SHIMMER", "SHIMMER: octave-up shimmer inside the tail on L / R" },
        }};
        const float colW = (displayColsU1 - displayColsU0) / (float) displayColumns;
        for (int c = 0; c < displayColumns; ++c)
        {
            const float cu = displayColsU0 + ((float) c + 0.5f) * colW;
            label (columns[(size_t) c].first, cu, 0.865f, tiny, juce::Justification::centred, columns[(size_t) c].second);
            label ("L", cu - 0.24f * colW, 0.775f, tiny * 0.85f, juce::Justification::centred, "left channel");
            label ("R", cu + 0.24f * colW, 0.775f, tiny * 0.85f, juce::Justification::centred, "right channel");
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
    /*  The analyser graticule: the frequency scale along the bottom, the dB marks for the EQ
        curve at the left, what the two traces are, and the live readout along the top. It is
        re-rendered only when that text changes. */
    RawTexture renderDisplayOverlay (const DisplayText& t)
    {
        const int w = displayOverlayWidth, h = displayOverlayHeight;
        juce::Image img (juce::Image::SingleChannel, w, h, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);

        // Same plot rectangle the shader uses. The window is recessed, and this unit is below eye level:
        // the recess wall and bezel lip hide the top few pixels of the glass, so the header starts 14 px
        // down; the frequency scale sits under the plot, not on the spectrum.
        const float plotL = 0.055f * (float) w, plotR = 0.975f * (float) w;
        const float plotT = 0.20f * (float) h, plotB = 0.84f * (float) h;

        auto atHz = [&] (float hz) { return plotL + (plotR - plotL) * std::log (hz / 20.0f) / std::log (1000.0f); };

        // Graticule: decades bright, 2/5 steps faint, four horizontal dB lines
        for (float hz : { 100.0f, 1000.0f, 10000.0f })
        {
            g.setOpacity (0.55f);
            const float x = atHz (hz);
            g.fillRect (juce::Rectangle<float> (x - 0.6f, plotT, 1.2f, plotB - plotT));
        }
        for (float hz : { 50.0f, 200.0f, 500.0f, 2000.0f, 5000.0f, 20000.0f })
        {
            g.setOpacity (0.18f);
            const float x = atHz (hz);
            if (x > plotL && x < plotR)
                g.fillRect (juce::Rectangle<float> (x - 0.5f, plotT, 1.0f, plotB - plotT));
        }
        for (int i = 1; i < 4; ++i)
        {
            g.setOpacity (0.14f);
            const float y = plotT + (plotB - plotT) * (float) i / 4.0f;
            g.fillRect (juce::Rectangle<float> (plotL, y - 0.5f, plotR - plotL, 1.0f));
        }
        g.setOpacity (1.0f);

        // Frequency scale, under the plot
        g.setFont (makeFont (13.0f, true, 0.10f));
        struct Mark { float hz; const char* label; };
        for (auto& mark : { Mark { 50.0f, "50" }, Mark { 100.0f, "100" }, Mark { 200.0f, "200" }, Mark { 500.0f, "500" },
                            Mark { 1000.0f, "1k" }, Mark { 2000.0f, "2k" }, Mark { 5000.0f, "5k" },
                            Mark { 10000.0f, "10k" }, Mark { 16000.0f, "16k" } })
        {
            const float x = atHz (mark.hz);
            if (x < plotL + 6.0f || x > plotR - 14.0f)
                continue;
            g.drawText (mark.label, juce::Rectangle<float> (x - 22.0f, plotB + 3.0f, 44.0f, 15.0f),
                        juce::Justification::centred, false);
        }

        // dB marks for the EQ curve, at the left edge
        g.setFont (makeFont (12.0f, true, 0.08f));
        for (int i = -1; i <= 1; ++i)
        {
            const float v = 0.5f - (float) i * 0.5f * 0.92f;
            const float y = plotT + (plotB - plotT) * v;
            const auto label = i == 0 ? juce::String ("0") : juce::String (i > 0 ? "+12" : "-12");
            g.drawText (label, juce::Rectangle<float> (plotL + 3.0f, y - 8.0f, 34.0f, 16.0f),
                        juce::Justification::centredLeft, false);
        }

        // Header: what the plugin is doing, and the key to the traces
        constexpr float headerY = 14.0f, headerH = 18.0f;
        g.setFont (makeFont (15.0f, true, 0.06f, true));
        g.drawText (t.title, juce::Rectangle<float> (plotL + 2.0f, headerY, (float) w * 0.5f, headerH),
                    juce::Justification::centredLeft, false);
        g.drawText (t.focusLine.isNotEmpty() ? t.focusLine : t.limitLine.isNotEmpty() ? t.limitLine : t.lineLeft,
                    juce::Rectangle<float> ((float) w * 0.34f, headerY, (float) w * 0.44f, headerH),
                    juce::Justification::centred, false);
        g.drawText (t.tag, juce::Rectangle<float> (plotL + 150.0f, headerY, 70.0f, headerH),
                    juce::Justification::centredLeft, false);

        g.setFont (makeFont (12.0f, true, 0.14f));
        g.drawText ("IN / OUT / EQ / LIMIT", juce::Rectangle<float> ((float) w * 0.62f, headerY, plotR - (float) w * 0.62f - 2.0f, headerH),
                    juce::Justification::centredRight, false);

        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (img, tex, 0);
        return tex;
    }
}
