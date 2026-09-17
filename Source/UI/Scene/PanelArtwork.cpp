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
        /** Maps panel world coordinates (x, z) to decal pixels. */
        struct PanelMapper
        {
            float sx, sz;
            float px (float x) const { return (x + panelHalfW) * sx; }
            float pz (float z) const { return (z + panelHalfD) * sz; }
            float len (float w) const { return w * sx; }

            juce::Rectangle<float> rect (float x0, float z0, float x1, float z1) const
            {
                return juce::Rectangle<float>::leftTopRightBottom (px (x0), pz (z0), px (x1), pz (z1));
            }
        };

        void drawTextWorld (juce::Graphics& g, const PanelMapper& m, const juce::String& text,
                            float x, float z, float height, juce::Justification just,
                            bool bold, float tracking = 0.06f, float boxWidth = 1.2f)
        {
            g.setFont (makeFont (m.len (height), bold, tracking));

            float x0 = x;
            if (just.testFlags (juce::Justification::horizontallyCentred)) x0 = x - boxWidth * 0.5f;
            else if (just.testFlags (juce::Justification::right))          x0 = x - boxWidth;

            g.drawText (text, m.rect (x0, z - height, x0 + boxWidth, z + height),
                        juce::Justification (just.getOnlyHorizontalFlags() | juce::Justification::verticallyCentred), false);
        }

        juce::Path footprint (bool left)
        {
            // Unit shoe print, toes towards -y, roughly 1 x 2 units.
            juce::Path p;
            p.addEllipse (-0.50f, -1.00f, 1.00f, 1.20f);   // forefoot
            p.addEllipse (-0.36f, 0.35f, 0.72f, 0.70f);    // heel
            p.addEllipse (left ? -0.62f : 0.22f, -1.28f, 0.40f, 0.34f); // big toe
            return p;
        }
    }

    RawTexture renderPanelDecal (int textureWidth)
    {
        const int w = textureWidth, h = textureWidth / 2;
        const PanelMapper m { (float) w / (2.0f * panelHalfW), (float) h / (2.0f * panelHalfD) };

        juce::Image ink (juce::Image::SingleChannel, w, h, true);
        juce::Image accent (juce::Image::SingleChannel, w, h, true);
        juce::Image glyphL (juce::Image::SingleChannel, w, h, true);
        juce::Image glyphR (juce::Image::SingleChannel, w, h, true);

        const auto solid = juce::Colours::white;

        {
            juce::Graphics g (ink);
            g.setColour (solid);

            // Brand
            drawTextWorld (g, m, "ADAPTIVE DYNAMICS", -2.22f, -1.02f, 0.085f, juce::Justification::left, true, 0.10f, 2.0f);
            drawTextWorld (g, m, "PvP ANALOG CONTROL UNIT   /   MODEL AD-1", -2.22f, -0.905f, 0.030f, juce::Justification::left, false, 0.18f, 2.0f);

            // Scope caption
            drawTextWorld (g, m, "RESPONSE MONITOR", scopeRect.minX(), scopeRect.maxZ() + 0.095f, 0.028f, juce::Justification::left, false, 0.2f, 1.0f);

            // Knob labels
            for (auto& c : controls)
            {
                if (c.kind == ControlKind::knob)
                    drawTextWorld (g, m, c.label, c.x, c.z + 0.375f, 0.040f, juce::Justification::horizontallyCentred, true, 0.12f, 0.66f);
                else if (c.kind == ControlKind::auxKnob)
                    drawTextWorld (g, m, c.label, c.x, c.z + 0.25f, 0.030f, juce::Justification::horizontallyCentred, true, 0.08f, 0.40f);
                else
                {
                    drawTextWorld (g, m, c.label, c.x, c.z - 0.245f, 0.028f, juce::Justification::horizontallyCentred, true, 0.08f, 0.40f);
                    drawTextWorld (g, m, "ON",  c.x + 0.112f, c.z - 0.15f, 0.020f, juce::Justification::left, false, 0.05f, 0.2f);
                    drawTextWorld (g, m, "OFF", c.x + 0.112f, c.z + 0.15f, 0.020f, juce::Justification::left, false, 0.05f, 0.2f);
                }
            }

            // Section frames
            const auto stroke = m.len (0.006f);
            g.drawRoundedRectangle (m.rect (-0.88f, -0.63f, 1.88f, 0.985f), m.len (0.04f), stroke);
            g.drawRoundedRectangle (m.rect (1.935f, -0.63f, 2.27f, 0.985f), m.len (0.04f), stroke);
            g.drawRoundedRectangle (m.rect (-2.27f, 0.365f, -1.00f, 0.985f), m.len (0.04f), stroke);
            g.drawRoundedRectangle (m.rect (ventBlock.minX() - 0.05f, ventBlock.minZ() - 0.05f,
                                            ventBlock.maxX() + 0.05f, ventBlock.maxZ() + 0.05f), m.len (0.03f), stroke);

            // Scope bezel print
            g.drawRect (m.rect (scopeRect.minX() - 0.03f, scopeRect.minZ() - 0.03f, scopeRect.maxX() + 0.03f, scopeRect.maxZ() + 0.03f), stroke * 1.4f);

            // Legend
            const char* legend[3] { "USER", "AUTOMATION", "SELF-TUNE" };
            for (int i = 0; i < 3; ++i)
                drawTextWorld (g, m, legend[i], legendX[i] + 0.055f, legendZ, 0.024f, juce::Justification::left, true, 0.08f, 0.5f);

            drawTextWorld (g, m, "PHASE 1  /  UI PROTOTYPE  /  NO DSP", 2.27f, 1.075f, 0.022f, juce::Justification::right, false, 0.16f, 1.6f);

            // Faint glyph outline (visible when the mode is off)
            g.setColour (solid.withAlpha (0.35f));
            for (bool left : { true, false })
            {
                auto t = juce::AffineTransform::rotation (left ? -0.18f : 0.18f)
                           .scaled (m.len (0.05f))
                           .translated (m.px (glyphX + (left ? -0.07f : 0.07f)), m.pz (glyphZ + (left ? 0.045f : -0.045f)));
                g.strokePath (footprint (left), juce::PathStrokeType (m.len (0.004f)), t);
            }
        }

        {
            juce::Graphics g (accent);
            g.setColour (solid);
            g.fillRect (m.rect (-2.22f, -0.865f, -1.00f, -0.855f));

            drawTextWorld (g, m, "ADAPTIVE CORE", -0.86f, -0.675f, 0.026f, juce::Justification::left, true, 0.22f, 1.0f);
            drawTextWorld (g, m, "MODES", 1.95f, -0.675f, 0.026f, juce::Justification::left, true, 0.22f, 0.4f);
            drawTextWorld (g, m, "PD CONTROL", -2.25f, 0.325f, 0.026f, juce::Justification::left, true, 0.22f, 1.0f);
            drawTextWorld (g, m, "THERMAL / ACTIVITY", ventBlock.maxX() + 0.05f, ventBlock.minZ() - 0.085f, 0.022f, juce::Justification::right, true, 0.22f, 1.0f);
        }

        for (bool left : { true, false })
        {
            juce::Graphics g (left ? glyphL : glyphR);
            g.setColour (solid);
            auto t = juce::AffineTransform::rotation (left ? -0.18f : 0.18f)
                       .scaled (m.len (0.05f))
                       .translated (m.px (glyphX + (left ? -0.07f : 0.07f)), m.pz (glyphZ + (left ? 0.045f : -0.045f)));
            g.fillPath (footprint (left), t);
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

        const float scale = (float) size / (2.0f * flangeRadius);
        const juce::Point<float> centre ((float) size * 0.5f, (float) size * 0.5f);
        const float numberRadius = 0.214f * scale;

        g.setFont (makeFont (0.042f * scale, true));

        for (int i = 0; i <= 10; ++i)
        {
            const float angle = knobAngleForValue ((float) i / 10.0f);
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::rotation (angle, centre.x, centre.y));

            const auto box = juce::Rectangle<float> (0.08f * scale, 0.06f * scale).withCentre ({ centre.x, centre.y - numberRadius });
            g.drawText (juce::String (i), box, juce::Justification::centred, false);
        }

        for (int i = 0; i <= 50; ++i)
        {
            const float angle = knobAngleForValue ((float) i / 50.0f);
            const bool major = (i % 5) == 0;
            const float r0 = (major ? 0.247f : 0.256f) * scale, r1 = 0.270f * scale;
            const juce::Point<float> dir (std::sin (angle), -std::cos (angle));
            g.drawLine ({ centre + dir * r0, centre + dir * r1 }, (major ? 0.006f : 0.0035f) * scale);
        }

        RawTexture tex { size, size, 1, {} };
        tex.pixels.assign ((size_t) (size * size), 0);
        copyChannel (img, tex, 0);
        return tex;
    }

    //==============================================================================
    RawTexture renderScopeOverlay (const ScopeText& text)
    {
        const int w = scopeOverlayWidth, h = scopeOverlayHeight;
        juce::Image img (juce::Image::SingleChannel, w, h, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::white);

        const float pad = 16.0f;

        g.setFont (makeFont (38.0f, true, 0.0f, true));
        g.drawText (text.target, juce::Rectangle<float> (pad, 6.0f, (float) w - 2.0f * pad, 44.0f), juce::Justification::centredLeft, false);

        g.setFont (makeFont (24.0f, true, 0.0f, true));
        g.drawText (text.lineRight, juce::Rectangle<float> (pad, 12.0f, (float) w - 2.0f * pad, 34.0f), juce::Justification::centredRight, false);

        g.setFont (makeFont (18.0f, false, 0.05f, true));
        g.drawText (text.title + "  " + text.footer, juce::Rectangle<float> (pad, 50.0f, (float) w - 2.0f * pad, 22.0f), juce::Justification::centredLeft, false);

        g.setFont (makeFont (36.0f, true, 0.0f, true));
        const float y = (float) h - 52.0f;

        if (text.focusLine.isNotEmpty())
        {
            g.drawText (text.focusLine, juce::Rectangle<float> (pad, y, (float) w - 2.0f * pad, 44.0f), juce::Justification::centredLeft, false);
        }
        else
        {
            const float half = ((float) w - 2.0f * pad) * 0.5f;
            g.drawText (text.lineLeft, juce::Rectangle<float> (pad, y, half, 44.0f), juce::Justification::centredLeft, false);
            g.drawText (text.lineMid,  juce::Rectangle<float> (pad + half, y, half, 44.0f), juce::Justification::centredRight, false);
        }

        RawTexture tex { w, h, 1, {} };
        tex.pixels.assign ((size_t) (w * h), 0);
        copyChannel (img, tex, 0);
        return tex;
    }
}
