#include "PanelArtwork.h"
#include "DeviceLayout.h"

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
            float sx, sz;
            float px (float x) const { return (x + faceHalfW) * sx; }
            float pz (float z) const { return (z + faceHalfH) * sz; }
            float len (float w) const { return w * sx; }

            juce::Rectangle<float> rect (float x0, float z0, float x1, float z1) const
            {
                return juce::Rectangle<float>::leftTopRightBottom (px (x0), pz (z0), px (x1), pz (z1));
            }
        };

        void text (juce::Graphics& g, const PanelMapper& m, const juce::String& s, float x, float z, float height,
                   juce::Justification just, bool bold, float tracking, float boxWidth)
        {
            g.setFont (makeFont (m.len (height), bold, tracking));

            float x0 = x;
            if (just.testFlags (juce::Justification::horizontallyCentred)) x0 = x - boxWidth * 0.5f;
            else if (just.testFlags (juce::Justification::right))          x0 = x - boxWidth;

            g.drawText (s, m.rect (x0, z - height, x0 + boxWidth, z + height),
                        juce::Justification (just.getOnlyHorizontalFlags() | juce::Justification::verticallyCentred), false);
        }

        juce::Path footprint (bool left)
        {
            juce::Path p;
            p.addEllipse (-0.50f, -1.00f, 1.00f, 1.25f);
            p.addEllipse (-0.38f, 0.40f, 0.76f, 0.72f);
            p.addEllipse (left ? -0.62f : 0.22f, -1.32f, 0.40f, 0.36f);
            return p;
        }

        juce::AffineTransform footTransform (const PanelMapper& m, bool left)
        {
            return juce::AffineTransform::rotation (left ? -0.20f : 0.20f)
                     .scaled (m.len (0.048f))
                     .translated (m.px (glyphX + (left ? -0.065f : 0.065f)), m.pz (glyphZ + (left ? 0.035f : -0.035f)));
        }
    }

    RawTexture renderFaceplateDecal (int textureWidth)
    {
        const int w = textureWidth;
        const int h = juce::roundToInt ((float) textureWidth * faceHalfH / faceHalfW);
        const PanelMapper m { (float) w / (2.0f * faceHalfW), (float) h / (2.0f * faceHalfH) };

        juce::Image ink (juce::Image::SingleChannel, w, h, true);
        juce::Image accent (juce::Image::SingleChannel, w, h, true);
        juce::Image glyphL (juce::Image::SingleChannel, w, h, true);
        juce::Image glyphR (juce::Image::SingleChannel, w, h, true);

        const auto solid = juce::Colours::white;
        const auto centred = juce::Justification::horizontallyCentred;
        const auto left = juce::Justification::left;

        {
            juce::Graphics g (ink);
            g.setColour (solid);

            // Brand
            text (g, m, "ADAPTIVE DYNAMICS", displayRect.minX() - 0.02f, -0.575f, 0.075f, left, true, 0.12f, 2.0f);
            text (g, m, "MODEL AD-1", 0.18f, -0.60f, 0.036f, centred, true, 0.25f, 0.6f);

            // Knobs: label, sub label, fixed indicator line
            for (auto& c : controls)
            {
                if (c.kind == ControlKind::knob)
                {
                    text (g, m, c.label, c.x, c.z + 0.465f, 0.060f, centred, true, 0.14f, 0.95f);
                    text (g, m, c.subLabel, c.x, c.z + 0.575f, 0.030f, centred, false, 0.22f, 0.95f);

                    g.fillRoundedRectangle (m.rect (c.x - 0.009f, c.z - indicatorFar - 0.005f, c.x + 0.009f, c.z - indicatorNear + 0.005f), m.len (0.006f));

                    juce::Path arrow; // arrowhead pointing down at the rotating scale
                    arrow.addTriangle (m.px (c.x - 0.03f), m.pz (c.z - indicatorNear + 0.005f),
                                       m.px (c.x + 0.03f), m.pz (c.z - indicatorNear + 0.005f),
                                       m.px (c.x), m.pz (c.z - indicatorNear + 0.035f));
                    g.fillPath (arrow);
                }
                else
                {
                    text (g, m, c.label, c.x, c.z - 0.30f, 0.046f, centred, true, 0.14f, 0.6f);
                    text (g, m, c.subLabel, c.x, c.z + 0.43f, 0.030f, centred, false, 0.22f, 0.6f);
                    text (g, m, "ON",  c.x + switchPlateHalfW + 0.025f, c.z - 0.15f, 0.030f, left, true, 0.05f, 0.2f);
                    text (g, m, "OFF", c.x + switchPlateHalfW + 0.025f, c.z + 0.15f, 0.030f, left, true, 0.05f, 0.2f);
                }
            }

            // Display caption + legend
            const char* legend[3] { "USER", "AUTO", "SELF-TUNE" };
            for (int i = 0; i < 3; ++i)
                text (g, m, legend[i], legendX[i] + 0.05f, legendZ, 0.030f, left, true, 0.08f, 0.5f);

            text (g, m, "POWER", powerLampX, powerLampZ + 0.085f, 0.022f, centred, true, 0.18f, 0.4f);
            text (g, m, "NO DSP  /  UI PROTOTYPE", 0.18f, 0.62f, 0.024f, centred, false, 0.22f, 1.2f);

            // Faint footprint outline, visible when the mode is off
            g.setColour (solid.withAlpha (0.45f));
            for (bool isLeft : { true, false })
                g.strokePath (footprint (isLeft), juce::PathStrokeType (m.len (0.005f)), footTransform (m, isLeft));
        }

        {
            juce::Graphics g (accent);
            g.setColour (solid);

            // Pink pinstripes
            g.fillRect (m.rect (displayRect.minX() - 0.02f, -0.505f, displayRect.maxX(), -0.497f));
            g.fillRect (m.rect (-0.70f, 0.660f, 1.20f, 0.668f));
            g.fillRect (m.rect (-0.70f, -0.668f, 1.20f, -0.660f));

            // Short pink arc behind each indicator
            for (auto& c : controls)
                if (c.kind == ControlKind::knob)
                {
                    juce::Path arc;
                    const float r = m.len (bezelRadius + 0.03f);
                    arc.addCentredArc (m.px (c.x), m.pz (c.z), r, r, 0.0f, -0.35f, 0.35f, true);
                    g.strokePath (arc, juce::PathStrokeType (m.len (0.008f)));
                }
        }

        for (bool isLeft : { true, false })
        {
            juce::Graphics g (isLeft ? glyphL : glyphR);
            g.setColour (solid);
            g.fillPath (footprint (isLeft), footTransform (m, isLeft));
        }

        RawTexture tex { w, h, 4, {} };
        tex.pixels.assign ((size_t) (w * h * 4), 0);
        copyChannel (ink, tex, 0);
        copyChannel (accent, tex, 1);
        copyChannel (glyphL, tex, 2);
        copyChannel (glyphR, tex, 3);
        return tex;
    }

    //==============================================================================
    RawTexture renderKnobDial (int size)
    {
        juce::Image img (juce::Image::SingleChannel, size, size, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);

        const float scale = (float) size / (2.0f * dialRadius);
        const juce::Point<float> centre ((float) size * 0.5f, (float) size * 0.5f);
        const float numberRadius = 0.252f * scale;

        g.setFont (makeFont (0.066f * scale, true));

        // Laid out so the number under the fixed indicator equals the value.
        for (int i = 0; i <= 10; ++i)
        {
            const float angle = scaleAngleForValue ((float) i / 10.0f);
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

            const auto box = juce::Rectangle<float> (0.10f * scale, 0.075f * scale).withCentre ({ centre.x, centre.y - numberRadius });
            g.drawText (juce::String (i), box, juce::Justification::centred, false);
        }

        for (int i = 0; i <= 50; ++i)
        {
            const float angle = scaleAngleForValue ((float) i / 50.0f);
            const bool major = (i % 5) == 0;
            const float r0 = (major ? 0.178f : 0.192f) * scale, r1 = 0.212f * scale;
            const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
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
        g.setFont (makeFont (38.0f, true, 0.0f, true));

        if (t.focusLine.isNotEmpty())
        {
            g.drawText (t.focusLine, juce::Rectangle<float> (pad, y, (float) w - 2.0f * pad, 48.0f), juce::Justification::centredLeft, false);
        }
        else
        {
            const float half = ((float) w - 2.0f * pad) * 0.5f;
            g.drawText (t.lineLeft,  juce::Rectangle<float> (pad, y, half, 48.0f), juce::Justification::centredLeft, false);
            g.drawText (t.lineRight, juce::Rectangle<float> (pad + half, y, half, 48.0f), juce::Justification::centredRight, false);
        }

        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (img, tex, 0);
        return tex;
    }
}
