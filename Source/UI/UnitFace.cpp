#include "UnitFace.h"
#include "Scene/CustomLayout.h"

namespace pad::face
{
    using namespace layout;

    /** The renderer's plate colours are linear light; the 2D view paints in sRGB. */
    juce::Colour fromLinear (float r, float g, float b)
    {
        auto enc = [] (float v) { v = std::clamp (v, 0.0f, 1.0f); return (juce::uint8) std::lround (255.0f * (v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow (v, 1.0f / 2.4f) - 0.055f)); };
        return juce::Colour (enc (r), enc (g), enc (b));
    }

    Plate plateOf (int unit)
    {
        switch (unit)
        {
            case enhUnit:       return { 0.050f, 0.052f, 0.058f };
            case tubeUnit:      return { 0.16f, 0.30f, 0.40f };
            case tideUnit:      return { 0.035f, 0.035f, 0.04f };
            case lumenUnit:     return { 0.70f, 0.70f, 0.71f };
            case limiterUnit:   return { 0.80f, 0.76f, 0.62f };
            case deepUnit:      return { 0.030f, 0.030f, 0.034f };
            case characterUnit: return { 0.90f, 0.90f, 0.88f };
            case radarUnit:     return { 0.055f, 0.070f, 0.15f };
            case levelUnit:     return { 0.045f, 0.045f, 0.05f };
            case balancerUnit:  return { 0.13f, 0.15f, 0.27f };
            case monitorUnit:   return { 0.16f, 0.165f, 0.175f };
            case powerUnit:     return { 0.035f, 0.035f, 0.04f };
            case lunchboxUnit:  return { 0.045f, 0.045f, 0.05f };
            case x4Unit:        return { 0.061f, 0.064f, 0.068f };
            case velvetUnit:    return { 0.0012f, 0.098f, 0.012f };
            case takebackUnit:  return { 0.0194f, 0.063f, 0.242f };
            case scopeUnit:     return { 0.028f, 0.036f, 0.048f };
            default: break;
        }
        if (unit == customUnit) { const auto l = layout::custom::get(); return { l->plate[0], l->plate[1], l->plate[2] }; }
        if (unit >= firstGenUnit && unit < firstGenUnit + gen::count) { const auto& l = gen::looks[unit - firstGenUnit]; return { l.plate[0], l.plate[1], l.plate[2] }; }
        return { 0.05f, 0.05f, 0.055f };
    }

    juce::Colour knobColour (KnobStyle s)
    {
        switch (s)
        {
            case KnobStyle::apiRed:        return juce::Colour (0xff9a2a22);
            case KnobStyle::apiBlue:       return juce::Colour (0xff2c4c86);
            case KnobStyle::apiWhite:      return juce::Colour (0xffd8d6cf);
            case KnobStyle::neveMaroon:    return juce::Colour (0xff6a1c1a);
            case KnobStyle::neveGrey:      return juce::Colour (0xff76787c);
            case KnobStyle::neveSmallGrey: return juce::Colour (0xff8a8c90);
            case KnobStyle::consoleWhite:  return juce::Colour (0xffe6e4de);
            default:                       return juce::Colour (0xff1c1d20);
        }
    }

    /** A texture from the artwork as a JUCE image: one channel (print) becomes `ink` with that alpha,
        three or four channels are copied (straight alpha). */
    /** A meter's face: the cream card, its print (channel 0) and its red zone (channel 1). */
    juce::Image meterFace (const artwork::RawTexture& t)
    {
        if (t.width <= 0 || t.height <= 0 || t.channels < 2) return {};
        juce::Image img (juce::Image::ARGB, t.width, t.height, false);
        juce::Image::BitmapData d (img, juce::Image::BitmapData::writeOnly);
        const juce::Colour cream (0xfff0e6cb), ink (0xff1c1712), red (0xffc0392b);
        for (int y = 0; y < t.height; ++y)
        {
            const auto card = cream.interpolatedWith (juce::Colour (0xffd8caa4), (float) y / (float) t.height);
            for (int x = 0; x < t.width; ++x)
            {
                const auto* p = t.pixels.data() + ((size_t) y * (size_t) t.width + (size_t) x) * (size_t) t.channels;
                d.setPixelColour (x, y, card.interpolatedWith (red, p[1] / 255.0f).interpolatedWith (ink, p[0] / 255.0f));
            }
        }
        return img;
    }

    juce::Image toImage (const artwork::RawTexture& t, juce::Colour ink)
    {
        if (t.width <= 0 || t.height <= 0 || t.pixels.empty()) return {};
        // (print drawn faint for the 3D rack's engraving is brought up to full strength)
        int most = 1;
        if (t.channels <= 2)
            for (size_t i = 0; i < t.pixels.size(); i += (size_t) t.channels) most = std::max (most, (int) t.pixels[i]);
        const float lift = 255.0f / (float) most;
        juce::Image img (juce::Image::ARGB, t.width, t.height, true);
        juce::Image::BitmapData d (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < t.height; ++y)
            for (int x = 0; x < t.width; ++x)
            {
                const auto* p = t.pixels.data() + ((size_t) y * (size_t) t.width + (size_t) x) * (size_t) t.channels;
                // (print: its coverage lifted a little - the 3D rack's lit paint makes thin print read stronger)
                const auto a = (juce::uint8) std::lround (255.0f * std::min (1.0f, 1.6f * std::sqrt (std::min (1.0f, p[0] * lift / 255.0f))));
                juce::Colour c = t.channels == 1 ? ink.withAlpha (a)
                               : t.channels == 3 ? juce::Colour (p[0], p[1], p[2])
                               : t.channels == 2 ? ink.withAlpha (a)
                                                 : juce::Colour (p[0], p[1], p[2], p[3]);
                d.setPixelColour (x, y, c);
            }
        return img;
    }

    /** One faceplate, drawn once: its paint, its print, its windows and meters' faces, its ears' screws. */
    juce::Image render (int unit, float scale, bool withControls)
    {
        const float hw = unitHalfW (unit), hh = unitHalfH (unit);
        const int w = std::max (1, juce::roundToInt (2.0f * hw * scale)), h = std::max (1, juce::roundToInt (2.0f * hh * scale));
        juce::Image img (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        auto X = [&] (float x) { return (x + hw) * scale; };
        auto Z = [&] (float z) { return (z + hh) * scale; };
        const auto pl = plateOf (unit);
        const bool light = pl.r + pl.g + pl.b > 1.05f;
        // (as the 3D rack's light leaves it: the paint a little darker than its linear colour reads flat)
        const float k = light ? 0.85f : 0.55f;
        const auto plate = fromLinear (pl.r * k, pl.g * k, pl.b * k);

        // The plate: its paint with a soft light from above, a bevel, the ears' slots and screws
        g.setGradientFill (juce::ColourGradient (plate.brighter (0.10f), 0.0f, 0.0f, plate.darker (0.18f), 0.0f, (float) h, false));
        g.fillRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (1.0f, 1.0f, (float) w - 2.0f, (float) h - 2.0f, 2.0f, 1.0f);
        if (unit == lunchboxUnit)
        {
            // The LUNCHBOX: each installed module's own plate on the black frame, the empty slots dark
            for (int m = 0; m < lb::numModules; ++m)
            {
                if (! lb::installed (m)) continue;
                static constexpr Plate first[4] { { 0.21f, 0.30f, 0.43f }, { 0.07f, 0.072f, 0.078f }, { 0.10f, 0.105f, 0.115f }, { 0.07f, 0.24f, 0.52f } };
                const auto c = m < 4 ? first[m] : Plate { enh::dsp::lbmods::info[m - 4].plate[0], enh::dsp::lbmods::info[m - 4].plate[1], enh::dsp::lbmods::info[m - 4].plate[2] };
                const float cx = lbModuleX (m), mhw = 0.5f * lbSlotW * (float) lb::widthOf (m) - 0.012f;
                const auto mc = fromLinear (c.r * 0.55f, c.g * 0.55f, c.b * 0.55f);
                g.setGradientFill (juce::ColourGradient (mc.brighter (0.08f), 0.0f, Z (-lbModuleHalfH), mc.darker (0.15f), 0.0f, Z (lbModuleHalfH), false));
                g.fillRoundedRectangle (X (cx - mhw), Z (-lbModuleHalfH), 2.0f * mhw * scale, 2.0f * lbModuleHalfH * scale, 2.0f);
            }
            for (int k : lb::emptySlots())
            {
                g.setColour (juce::Colour (0xff050506));
                g.fillRect (X (slotX (k) - 0.5f * lbSlotW + 0.02f), Z (-lbModuleHalfH), (lbSlotW - 0.04f) * scale, 2.0f * lbModuleHalfH * scale);
            }
        }
        else
            for (float sx : { -1.0f, 1.0f })
                for (float sz : { -1.0f, 1.0f })
                {
                    if (hh < 0.4f && sz > 0.0f) continue;   // (a 1U: one screw each side)
                    const float cx = X (sx * (hw - 0.11f)), cz = hh < 0.4f ? Z (0.0f) : Z (sz * (hh - 0.12f));
                    g.setColour (juce::Colour (0xffb8b9bd)); g.fillEllipse (cx - 4.0f, cz - 4.0f, 8.0f, 8.0f);
                    g.setColour (juce::Colour (0xff3a3b3f)); g.drawLine (cx - 2.5f, cz, cx + 2.5f, cz, 1.2f);
                }

        // Its windows: the displays as dark glass (the 2D rack draws no live screens)
        std::vector<Rect> windows;
        if (unit == enhUnit) windows.push_back (displayRect);
        if (unit == tubeUnit) windows.push_back (seraphDisplayRect);
        if (unit == monitorUnit) windows.push_back (monitorDisplayRect);
        if (unit == balancerUnit) windows.push_back (balancerDisplayRect);
        for (const auto& r : windows)
        {
            g.setColour (juce::Colour (0xff0b0d10));
            g.fillRoundedRectangle (X (r.cx - r.hw), Z (r.cz - r.hd), 2.0f * r.hw * scale, 2.0f * r.hd * scale, 3.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawRoundedRectangle (X (r.cx - r.hw), Z (r.cz - r.hd), 2.0f * r.hw * scale, 2.0f * r.hd * scale, 3.0f, 1.0f);
        }

        // The print
        const int tw = juce::jlimit (512, 2048, juce::nextPowerOfTwo (w));
        artwork::RawTexture decal;
        if (unit == enhUnit) decal = artwork::renderFaceplateDecal (tw);
        else if (unit == tubeUnit) decal = artwork::renderTubeDecal (tw);
        else if (unit == lunchboxUnit) decal = artwork::renderLunchboxDecal (tw);
        else if (unit == x4Unit || unit == velvetUnit || unit == takebackUnit || unit == scopeUnit || unit == customUnit || (unit >= firstGenUnit && unit < firstGenUnit + gen::count))
            decal = artwork::renderDesignedDecal (unit, tw);
        else
            decal = artwork::renderOneUDecal (unit, tw);
        if (decal.channels >= 3)   // (the ENHANCER's and TONE & SPACE's print: ink, lines, fills, stripes - the print is ink and lines)
        {
            artwork::RawTexture one { decal.width, decal.height, 1, {} };
            one.pixels.resize ((size_t) decal.width * (size_t) decal.height);
            // (each brought up to full strength on its own: the ink is stored far fainter than the lines)
            int most[2] { 1, 1 };
            for (size_t i = 0; i < one.pixels.size(); ++i)
                for (int c = 0; c < 2; ++c) most[c] = std::max (most[c], (int) decal.pixels[i * (size_t) decal.channels + (size_t) c]);
            for (size_t i = 0; i < one.pixels.size(); ++i)
                one.pixels[i] = (juce::uint8) std::max (255 * decal.pixels[i * (size_t) decal.channels] / most[0], 255 * decal.pixels[i * (size_t) decal.channels + 1] / most[1]);
            decal = std::move (one);
        }
        const auto ink = toImage (decal, light ? juce::Colour (0xff1a1a1c) : juce::Colour (0xfff2efe8));
        if (ink.isValid())
            g.drawImage (ink, juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h), juce::RectanglePlacement::stretchToFit);

        // The meters' faces (their needles are drawn live)
        for (int i = 0; i < numVus (unit); ++i)
        {
            const float cx = vuX (unit, i), cz = vuZ (unit, i), vw = vuHalfW (unit), vh = vuHalfHFor (unit);
            const auto faceTex = artwork::renderVuFace (unit, 256, nullptr, i);
            // (a meter's print is ink on the cream card the 3D rack's shader paints under it)
            const auto face = faceTex.channels >= 2 ? meterFace (faceTex) : toImage (faceTex, juce::Colour (0xff1c1712));
            const juce::Rectangle<float> r (X (cx - vw), Z (cz - vh), 2.0f * vw * scale, 2.0f * vh * scale);
            g.setColour (juce::Colour (0xff0d0d0f)); g.fillRoundedRectangle (r.expanded (2.0f), 3.0f);
            if (faceTex.channels < 2)
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff1e7cc), r.getCentreX(), r.getY(), juce::Colour (0xffd9cba5), r.getCentreX(), r.getBottom(), false));
                g.fillRoundedRectangle (r, 2.0f);
            }
            if (face.isValid()) g.drawImage (face, r, juce::RectanglePlacement::stretchToFit);
        }
        // The controls, simply (the locker's previews: the 2D rack draws its own, live)
        if (withControls)
            for (const auto& c : controls)
            {
                if (c.unit != unit || isParked (c)) continue;
                const float cx = X (c.x), cz = Z (c.z);
                if (c.kind == ControlKind::knob || c.kind == ControlKind::selector)
                {
                    const float r = std::max (1.5f, knobBodyRadius (c) * scale);
                    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (cx - r + 0.6f, cz - r + 1.0f, 2.0f * r, 2.0f * r);
                    g.setColour (knobColour (c.style)); g.fillEllipse (cx - r, cz - r, 2.0f * r, 2.0f * r);
                    g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawEllipse (cx - r, cz - r, 2.0f * r, 2.0f * r, 0.8f);
                    g.setColour (juce::Colours::white.withAlpha (0.85f)); g.drawLine (cx, cz, cx - 0.6f * r, cz - 0.7f * r, std::max (1.0f, 0.18f * r));
                }
                else
                {
                    const float hw2 = std::max (1.5f, 0.035f * scale), hd = std::max (2.0f, 0.06f * scale);
                    g.setColour (juce::Colour (0xff2a2b2f)); g.fillRoundedRectangle (cx - hw2, cz - hd, 2.0f * hw2, 2.0f * hd, 1.5f);
                    g.setColour (juce::Colour (0xffc8c9cc)); g.fillRect (cx - 0.4f * hw2, cz - hd * 0.9f, 0.8f * hw2, hd * 0.9f);
                }
            }
        return img;
    }

}
