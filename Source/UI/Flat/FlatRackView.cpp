#include "FlatRackView.h"
#include "../../PluginProcessor.h"
#include "../../Parameters/ParameterBridge.h"
#include "../Scene/CustomLayout.h"
#include "../Scene/RoomScreen.h"
#include "../Scene/SimScreens.h"

namespace pad
{
    using namespace layout;
    namespace
    {
        /** The renderer's plate colours are linear light; the 2D view paints in sRGB. */
        juce::Colour fromLinear (float r, float g, float b)
        {
            auto enc = [] (float v) { v = std::clamp (v, 0.0f, 1.0f); return (juce::uint8) std::lround (255.0f * (v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow (v, 1.0f / 2.4f) - 0.055f)); };
            return juce::Colour (enc (r), enc (g), enc (b));
        }

        struct Plate { float r, g, b; };
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
    }

    //==============================================================================
    FlatRackView::FlatRackView (PluginProcessor& p) : processor (p), bridge (p.getBridge())
    {
        setOpaque (true);
        storedUnits = processor.getStoredUnits();
        placeLunchbox (processor.getStoredModules());
        glassPanel = std::make_unique<GlassPanel> (bridge, processor);
        shownValues.assign ((size_t) numControls, -1.0f);
        glassPanel->setHolo (config.holoPanel);
        welcome.version = JucePlugin_VersionString;
        welcome.showAtStart = config.showWelcome;
        welcome.holoPanel = config.holoPanel;
        {
            const bool testing = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_SIZE", {}).isNotEmpty();
            const bool asked = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_WELCOME", {}).isNotEmpty();
            welcomeOpen = asked || (! testing && (config.showWelcome || config.welcomeSeen != welcome.version));
            if (welcomeOpen) renderWelcome();
        }
        startTimerHz (30);
    }

    FlatRackView::~FlatRackView() = default;

    float FlatRackView::maxScroll() const noexcept { return std::max (0.0f, contentH - (float) getHeight()); }

    //==============================================================================
    /** The rack as the 3D one stacks it (top to bottom), then the LUNCHBOX; each face drawn for this width. */
    void FlatRackView::rebuildRows()
    {
        builtFor = storedUnits.load(); builtModules = processor.getStoredModules(); builtWidth = getWidth();
        rows.clear();
        const float W = (float) juce::jlimit (200, 1000, getWidth() - 24);   // (a wide window shows more units, not bigger ones)
        const float left = 0.5f * ((float) getWidth() - W);
        float y = 12.0f;
        std::vector<int> order;
        for (auto it = rackOrder.rbegin(); it != rackOrder.rend(); ++it)
            if (isShown (*it)) order.push_back (*it);
        if (isShown (lunchboxUnit)) order.push_back (lunchboxUnit);
        for (int u : order)
        {
            Row r;
            r.unit = u;
            r.scale = W / (2.0f * unitHalfW (u));
            r.left = left;
            r.height = 2.0f * unitHalfH (u) * r.scale;
            r.top = y;
            r.face = renderFace (u, r.scale);
            rows.push_back (std::move (r));
            y += rows.back().height + (u == lunchboxUnit ? 12.0f : 2.0f);
        }
        contentH = y + 12.0f;
        scrollTarget = scrollY = std::clamp (scrollY, 0.0f, maxScroll());
        repaint();
    }

    /** One faceplate, drawn once: its paint, its print, its windows and meters' faces, its ears' screws. */
    juce::Image FlatRackView::renderFace (int unit, float scale) const
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
        return img;
    }

    //==============================================================================
    void FlatRackView::resized()
    {
        glassPanel->setViewSize (getLocalBounds().toFloat());
        if (getWidth() != builtWidth) rebuildRows();
    }

    const FlatRackView::Row* FlatRackView::rowAt (float cy) const
    {
        for (const auto& r : rows) if (cy >= r.top && cy < r.top + r.height) return &r;
        return nullptr;
    }

    juce::Point<float> FlatRackView::controlCentre (const Row& r, const ControlDef& c) const
    {
        return { r.left + (c.x + unitHalfW (r.unit)) * r.scale, r.top + (c.z + unitHalfH (r.unit)) * r.scale - scrollY };
    }

    int FlatRackView::paramFor (const ControlDef& c) const
    {
        if (c.altParamId != nullptr && c.modeParamId != nullptr)
            if (const int mode = bridge.indexOf (c.modeParamId); mode >= 0 && bridge.getNormalised (mode) > 0.5f)
                return bridge.indexOf (c.altParamId);
        return c.paramId != nullptr ? bridge.indexOf (c.paramId) : -1;
    }

    int FlatRackView::controlAt (juce::Point<float> p) const
    {
        const auto* r = rowAt (p.y + scrollY);
        if (r == nullptr) return -1;
        int best = -1; float bestD = 1.0e9f;
        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            if (c.unit != r->unit || isParked (c)) continue;
            const float rad = std::max (10.0f, (c.kind == ControlKind::knob || c.kind == ControlKind::selector ? knobBodyRadius (c) : 0.06f) * r->scale * 1.2f);
            const float d = controlCentre (*r, c).getDistanceFrom (p);
            if (d < rad && d < bestD) { best = i; bestD = d; }
        }
        return best;
    }

    //==============================================================================
    void FlatRackView::drawControl (juce::Graphics& g, const Row& r, const ControlDef& c) const
    {
        const auto at = controlCentre (r, c);
        const int pi = paramFor (c);
        const float v = pi >= 0 ? bridge.getNormalised (pi) : 0.0f;
        const bool hot = &c == &controls[(size_t) std::max (0, hoverControl)] && hoverControl >= 0;
        if (c.kind == ControlKind::knob || c.kind == ControlKind::selector)
        {
            const float rad = knobBodyRadius (c) * r.scale;
            const auto cap = knobColour (c.style);
            // skirt, cap with a light from above, and its pointer
            g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (at.x - rad * 1.08f, at.y - rad * 1.0f, rad * 2.16f, rad * 2.2f);
            g.setGradientFill (juce::ColourGradient (cap.brighter (0.35f), at.x, at.y - rad, cap.darker (0.45f), at.x, at.y + rad, false));
            g.fillEllipse (at.x - rad, at.y - rad, 2.0f * rad, 2.0f * rad);
            g.setColour (juce::Colours::white.withAlpha (hot ? 0.35f : 0.12f)); g.drawEllipse (at.x - rad, at.y - rad, 2.0f * rad, 2.0f * rad, 1.0f);
            const float a = c.kind == ControlKind::selector ? characterSelectorAngle (v) : knobAngleForValue (v);
            const bool lightCap = cap.getPerceivedBrightness() > 0.55f;
            g.setColour (lightCap ? juce::Colour (0xff111111) : juce::Colour (0xfff4f1ea));
            g.drawLine (at.x + std::sin (a) * rad * 0.15f, at.y - std::cos (a) * rad * 0.15f, at.x + std::sin (a) * rad * 0.85f, at.y - std::cos (a) * rad * 0.85f, std::max (1.5f, rad * 0.14f));
        }
        else if (c.kind == ControlKind::toggle)
        {
            const bool on = v > 0.5f;
            const float s = std::max (6.0f, 0.045f * r.scale);
            if (c.switchStyle == SwitchStyle::rocker || c.switchStyle == SwitchStyle::rockerRed)
            {
                const juce::Rectangle<float> b (at.x - s * 0.8f, at.y - s * 1.4f, s * 1.6f, s * 2.8f);
                g.setColour (juce::Colour (0xff111113)); g.fillRoundedRectangle (b.expanded (1.5f), 2.0f);
                const auto body = c.switchStyle == SwitchStyle::rockerRed ? juce::Colour (on ? 0xffe2372b : 0xff7a1712) : juce::Colour (on ? 0xff5a5c61 : 0xff2a2b2e);
                g.setColour (body); g.fillRoundedRectangle (b, 2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.drawText (on ? "I" : "O", b.withHeight (b.getHeight() * 0.5f).translated (0.0f, on ? 0.0f : b.getHeight() * 0.5f), juce::Justification::centred);
            }
            else
            {
                // a bat toggle: its nut, and the lever up (on) or down (off)
                g.setColour (juce::Colour (0xffc9cacd)); g.fillEllipse (at.x - s * 0.7f, at.y - s * 0.7f, s * 1.4f, s * 1.4f);
                g.setColour (juce::Colour (0xff6b6c70)); g.drawEllipse (at.x - s * 0.7f, at.y - s * 0.7f, s * 1.4f, s * 1.4f, 1.0f);
                const float ty = at.y + (on ? -1.0f : 1.0f) * s * 1.6f;
                g.setColour (juce::Colour (0xffe4e5e8)); g.drawLine (at.x, at.y, at.x, ty, std::max (2.0f, s * 0.35f));
                g.fillEllipse (at.x - s * 0.32f, ty - s * 0.32f, s * 0.64f, s * 0.64f);
            }
        }
        else
        {
            const float s = std::max (7.0f, 0.06f * r.scale);
            const bool on = v > 0.5f;
            g.setColour (juce::Colour (on ? 0xffe0dccf : 0xff4b4d52)); g.fillRoundedRectangle (at.x - s, at.y - s, 2.0f * s, 2.0f * s, 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (at.x - s, at.y - s, 2.0f * s, 2.0f * s, 2.0f, 1.0f);
        }
    }

    float FlatRackView::needleReading (int i) const
    {
        const auto& m = processor.getMeters();
        auto sat = [] (float x) { return std::clamp (x, 0.0f, 1.05f); };
        if (demo) return 0.45f + 0.3f * (float) std::sin (demoTime * (0.7 + 0.13 * i) + i);
        switch (i)
        {
            case 0:  return sat (m.tideGrDb.load() / 12.0f);
            case 1: case 2: case 3: return sat (m.lumenGainDb[(size_t) (i - 1)].load() / 18.0f);
            case 4:  { float d = 0.0f; for (auto& x : m.limitDepthDb) d = std::max (d, x.load()); return sat (d / 18.0f); }
            case 5:  return sat (m.limitBroadbandDb.load() / 12.0f);
            case 6:  return sat (m.momentaryLufs.load() / 40.0f + 1.0f);   // (the 3D reads the input's RMS here; the 2D its loudness)
            case 7:  return sat (m.momentaryLufs.load() / 40.0f + 1.0f);
            case 8:  return sat (m.shortTermLufs.load() / 40.0f + 1.0f);
            case 9:  return sat (m.deepGeneratedDb.load() / 40.0f + 1.0f);
            case 10: return sat (m.charHarmonicsDb.load() / 60.0f + 1.0f);
            case 11: return sat (m.radarLiftDb.load() / 36.0f);
            case 12: return sat (20.0f * std::log10 (m.lunchboxPeak.load() + 1.0e-6f) / 40.0f + 1.0f);
            case 13: return sat (m.x4DensityDb.load() / 60.0f + 1.0f);
            case 14: return sat (m.velvetDb.load() / 18.0f);
            default: return sat (m.takebackDb[(size_t) std::clamp (i - 15, 0, 3)].load() / 12.0f);
        }
    }

    /** RAY ROOM's screen, when it is powered: white on black, drawn live (RoomScreen.h). */
    juce::Rectangle<float> FlatRackView::roomScreenRect (const Row& r) const
    {
        if (r.unit < firstGenUnit || r.unit >= firstGenUnit + gen::count)
            return {};
        const std::string_view key = enh::dsp::units::info[r.unit - firstGenUnit].key;
        if (key != "rayroom" && simscreen::kindOf (key) == simscreen::Kind::none)
            return {};
        const auto [pr, np] = gen::printOf (r.unit - firstGenUnit);
        for (int i = 0; i < np; ++i)
            if (pr[i].kind == 'D')
                return { r.left + (pr[i].x - 0.5f * pr[i].w + unitHalfW (r.unit)) * r.scale, r.top + (pr[i].z - 0.5f * pr[i].h + unitHalfH (r.unit)) * r.scale - scrollY,
                         pr[i].w * r.scale, pr[i].h * r.scale };
        return {};
    }

    void FlatRackView::drawRoomScreen (juce::Graphics& g, const Row& r) const
    {
        const auto box = roomScreenRect (r);
        if (box.isEmpty()) return;
        const auto kind = simscreen::kindOf (enh::dsp::units::info[r.unit - firstGenUnit].key);
        const int power = bridge.indexOf (kind == simscreen::Kind::none ? "rrPower" : simscreen::powerId (kind));
        if (power < 0 || bridge.getNormalised (power) < 0.5f) return;
        const auto& m = processor.getMeters();
        std::array<float, 192> raw {};
        bool any = false;
        for (size_t i = 0; i < raw.size(); ++i) { raw[i] = m.unitDisplay[(size_t) (r.unit - firstGenUnit)][i].load (std::memory_order_relaxed); any = any || raw[i] != 0.0f; }
        std::vector<roomscreen::Line> lines; std::vector<roomscreen::Blob> blobs;
        if (kind != simscreen::Kind::none)
        {
            if (! any) simscreen::demo (kind, (float) demoTime, raw.data());
            simscreen::build (kind, raw.data(), box.getWidth(), box.getHeight(), (float) demoTime, lines, blobs);
        }
        else
        {
            namespace R = enh::dsp::units::room;
            R::State s;
            s.read (raw.data());
            if (s.W < 1.0f) { s.W = R::halfWidth (4.0f); s.D = R::halfDepth (4.0f); }
            roomscreen::build (s, (float) demoTime, box.getWidth(), box.getHeight(), lines, blobs);
        }
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (box, 3.0f);
        juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (box.toNearestInt());
        for (const auto& l : lines)
        {
            g.setColour (juce::Colours::white.withAlpha (std::clamp (l.bright, 0.0f, 1.0f)));
            g.drawLine (box.getX() + l.x0, box.getY() + l.y0, box.getX() + l.x1, box.getY() + l.y1, l.thick);
        }
        for (const auto& b : blobs)
        {
            g.setColour (juce::Colours::white.withAlpha (std::clamp (b.bright, 0.0f, 1.0f)));
            const juce::Rectangle<float> c (box.getX() + b.x - b.r, box.getY() + b.y - b.r, 2.0f * b.r, 2.0f * b.r);
            if (b.ring) g.drawEllipse (c, 1.2f); else g.fillEllipse (c);
        }
    }

    void FlatRackView::drawNeedles (juce::Graphics& g, const Row& r) const
    {
        for (int i = 0; i < numVus (r.unit); ++i)
        {
            const float hh = vuHalfHFor (r.unit), cx = r.left + (vuX (r.unit, i) + unitHalfW (r.unit)) * r.scale;
            const float cz = r.top + (vuZ (r.unit, i) + unitHalfH (r.unit)) * r.scale - scrollY;
            const float pivotY = cz + hh * hwk::models::vuPivotDrop * r.scale, len = hh * hwk::models::vuNeedleTip * r.scale * 0.92f;
            const float a = hwk::models::vuAngleFor (needleNow[(size_t) std::clamp (firstNeedle (r.unit) + i, 0, numNeedles - 1)]);
            juce::Graphics::ScopedSaveState keep (g);
            const float vw = vuHalfW (r.unit) * r.scale;
            g.reduceClipRegion (juce::Rectangle<float> (cx - vw, cz - hh * r.scale, 2.0f * vw, 2.0f * hh * r.scale).toNearestInt());
            g.setColour (juce::Colour (0xff1a1614));
            g.drawLine (cx, pivotY, cx + std::sin (a) * len, pivotY - std::cos (a) * len, 1.4f);
        }
    }

    //==============================================================================
    void FlatRackView::paint (juce::Graphics& g)
    {
        // Behind the rack: a dark room, a warm light from above
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1712), 0.0f, 0.0f, juce::Colour (0xff0b0a09), 0.0f, (float) getHeight(), false));
        g.fillAll();
        const auto clip = g.getClipBounds().toFloat();
        for (const auto& r : rows)
        {
            const float y = r.top - scrollY;
            if (y > clip.getBottom() || y + r.height < clip.getY()) continue;
            g.drawImageAt (r.face, juce::roundToInt (r.left), juce::roundToInt (y));
            for (int i = 0; i < numControls; ++i)
            {
                const auto& c = controls[(size_t) i];
                if (c.unit == r.unit && ! isParked (c)) drawControl (g, r, c);
            }
            drawNeedles (g, r);
            drawRoomScreen (g, r);
        }
        // a thin scroll bar
        if (contentH > (float) getHeight() + 1.0f)
        {
            const float h = (float) getHeight(), barH = std::max (30.0f, h * h / contentH), barY = (h - barH) * scrollY / std::max (1.0f, maxScroll());
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRoundedRectangle ((float) getWidth() - 7.0f, barY + 2.0f, 4.0f, barH - 4.0f, 2.0f);
        }
        // The settings panel / gear locker
        if (glassPanel->isOpen() && panelImage.isValid())
        {
            const auto b = glassPanel->getBounds();
            g.setColour (config.holoPanel ? juce::Colour (0xf2030b07) : juce::Colour (0xf0141518));
            g.fillRoundedRectangle (b, 7.0f);
            g.drawImage (panelImage, b, juce::RectanglePlacement::stretchToFit);
        }
        // The welcome screen: the rack dimmed, the card, the trace in its scope window
        if (welcomeOpen && welcomeImage.isValid())
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillAll();
            const auto r = holo::Welcome::placeIn ((float) getWidth(), (float) getHeight());
            g.setColour (juce::Colour (0xff030b07));
            g.fillRoundedRectangle (r.reduced (1.0f), 10.0f * r.getWidth() / holo::Welcome::cardW);
            g.drawImage (welcomeImage, r, juce::RectanglePlacement::stretchToFit);
            const float s = r.getWidth() / holo::Welcome::cardW;
            const auto sc = welcome.scope();
            const float cx = r.getX() + sc.getCentreX() * s, cy = r.getY() + sc.getCentreY() * s, hw = 0.45f * sc.getWidth() * s, hh = 0.45f * sc.getHeight() * s;
            holo::Welcome::lissajous (demoTime, welcomePts);
            juce::Path trace;
            for (size_t i = 0; i < welcomePts.size(); ++i)
            {
                const juce::Point<float> p (cx + welcomePts[i].x * hw, cy - welcomePts[i].y * hh);
                if (i == 0) trace.startNewSubPath (p); else trace.lineTo (p);
            }
            g.setColour (holo::phosphor.withAlpha (0.18f)); g.strokePath (trace, juce::PathStrokeType (5.0f));
            g.setColour (holo::phosphor.withAlpha (0.9f)); g.strokePath (trace, juce::PathStrokeType (1.6f));
        }
    }

    //==============================================================================
    void FlatRackView::timerCallback()
    {
        if (storedUnits.load() != processor.getStoredUnits()) storedUnits = processor.getStoredUnits();
        if (const auto lbs = processor.getStoredModules(); lbs != builtModules) placeLunchbox (lbs);
        if (storedUnits.load() != builtFor || processor.getStoredModules() != builtModules) rebuildRows();

        demoTime += 1.0 / 30.0;
        bool dirty = welcomeOpen;   // (the welcome screen's trace moves, its beam jitters)
        for (const auto& r : rows)   // RAY ROOM's screen moves while it is on
            if (const auto box = roomScreenRect (r); ! box.isEmpty() && box.getBottom() > 0.0f && box.getY() < (float) getHeight())
                repaint (box.toNearestInt().expanded (2));
        if (welcomeOpen && welcomeFrames[0].isValid())
            welcomeImage = welcomeFrames[(size_t) ((int) (demoTime * 12.0) * 7 % holo::Welcome::frames)];
        // the rack eases to where the wheel sent it
        if (std::abs (scrollTarget - scrollY) > 0.5f) { scrollY += (scrollTarget - scrollY) * 0.35f; dirty = true; }
        else if (scrollY != scrollTarget) { scrollY = scrollTarget; dirty = true; }
        // needles: a light ballistic, and only the meters' windows repainted
        for (int i = 0; i < numNeedles; ++i)
        {
            const float want = needleReading (i), before = needleNow[(size_t) i];
            needleNow[(size_t) i] += (want - needleNow[(size_t) i]) * 0.3f;
            if (std::abs (needleNow[(size_t) i] - before) > 0.002f && ! dirty)
                for (const auto& r : rows)
                    for (int k = 0; k < numVus (r.unit); ++k)
                        if (firstNeedle (r.unit) + k == i)
                        {
                            const float vw = vuHalfW (r.unit) * r.scale, vh = vuHalfHFor (r.unit) * r.scale;
                            const float cx = r.left + (vuX (r.unit, k) + unitHalfW (r.unit)) * r.scale, cz = r.top + (vuZ (r.unit, k) + unitHalfH (r.unit)) * r.scale - scrollY;
                            repaint (juce::Rectangle<float> (cx - vw, cz - vh, 2.0f * vw, 2.0f * vh).toNearestInt().expanded (2));
                        }
        }
        // a control moved (the host, a preset, automation): redraw
        for (int i = 0; i < numControls && ! dirty; ++i)
        {
            const auto& c = controls[(size_t) i];
            const int pi = paramFor (c);
            const float v = pi >= 0 ? bridge.getNormalised (pi) : 0.0f;
            if (std::abs (v - shownValues[(size_t) i]) > 1.0e-4f) dirty = true;
        }
        if (dirty)
        {
            for (int i = 0; i < numControls; ++i) { const int pi = paramFor (controls[(size_t) i]); shownValues[(size_t) i] = pi >= 0 ? bridge.getNormalised (pi) : 0.0f; }
            repaint();
        }
        // momentary buttons (PREV / NEXT preset, loudness RESET): they act here and spring back
        for (auto [pid, delta] : { std::pair { pad::params::id::presetPrev, -1 }, std::pair { pad::params::id::presetNext, 1 } })
            if (const int index = bridge.indexOf (pid); index >= 0 && bridge.getNormalised (index) > 0.5f)
            { processor.stepPreset (delta); bridge.setValueWithSource (index, 0.0f, ControlSource::user); }
        if (const int reset = bridge.indexOf (pad::params::id::loudnessReset); reset >= 0 && bridge.getNormalised (reset) > 0.5f)
        { processor.resetLoudness(); bridge.setValueWithSource (reset, 0.0f, ControlSource::user); }
        // the panel's animations
        if (glassPanel->isOpen())
        {
            glassPanel->pollValues();
            const bool moving = glassPanel->tick (1.0f / 30.0f);
            if (moving || glassPanel->needsRedraw())
            {
                const auto t = glassPanel->render ((float) juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale);
                panelImage = toImage (t, juce::Colours::white);
                repaint (glassPanel->getBounds().toNearestInt().expanded (4));
            }
        }
    }

    //==============================================================================
    void FlatRackView::openPanel (int unit)
    {
        glassPanel->open (unit, 0.5f * (float) getHeight(), getLocalBounds().toFloat());
        setWantsKeyboardFocus (glassPanel->wantsKeys());
        if (glassPanel->wantsKeys()) grabKeyboardFocus();
        panelImage = {};
        const auto t = glassPanel->render ((float) juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale);
        panelImage = toImage (t, juce::Colours::white);
        repaint();
    }

    void FlatRackView::showMenu (int unit)
    {
        juce::PopupMenu m;
        if (unit >= 0 && unit < numUnits)
            m.addItem (1, juce::String (unitInfo[(size_t) unit].name) + " settings...");
        m.addItem (2, "Gear locker...  (swap units in and out of the rack)");
        m.addItem (4, "Welcome screen...");
        m.addItem (5, "Hologram settings panel", true, config.holoPanel);
        m.addSeparator();
        m.addItem (3, "Back to the top", true, false);
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe = juce::Component::SafePointer<FlatRackView> (this), unit] (int chosen)
        {
            if (safe == nullptr) return;
            if (chosen == 1) safe->openPanel (unit);
            if (chosen == 2) safe->openPanel (glass::lockerPage);
            if (chosen == 3) safe->scrollTarget = 0.0f;
            if (chosen == 4) { safe->welcomeOpen = true; safe->renderWelcome(); safe->repaint(); }
            if (chosen == 5)
            {
                safe->config.holoPanel = ! safe->config.holoPanel; safe->welcome.holoPanel = safe->config.holoPanel;
                UIConfig::saveKey ("holoPanel", safe->config.holoPanel); safe->glassPanel->setHolo (safe->config.holoPanel);
            }
        });
    }

    void FlatRackView::mouseDown (const juce::MouseEvent& e)
    {
        const auto p = e.position;
        if (welcomeOpen)
        {
            const auto r = holo::Welcome::placeIn ((float) getWidth(), (float) getHeight());
            const int hit = r.contains (p) ? welcome.hit ((p - r.getPosition()) / (r.getWidth() / holo::Welcome::cardW)) : 1;
            if (hit == 1)
            {
                welcomeOpen = false;
                if (config.welcomeSeen != welcome.version) { config.welcomeSeen = welcome.version; UIConfig::saveKey ("welcomeSeen", welcome.version); }
            }
            else if (hit == 2) { welcome.showAtStart = config.showWelcome = ! config.showWelcome; UIConfig::saveKey ("showWelcome", config.showWelcome); renderWelcome(); }
            else if (hit == 3) { welcome.holoPanel = config.holoPanel = ! config.holoPanel; UIConfig::saveKey ("holoPanel", config.holoPanel); glassPanel->setHolo (config.holoPanel); renderWelcome(); }
            repaint();
            return;
        }
        if (glassPanel->isOpen())
        {
            if (glassPanel->getBounds().contains (p)) { glassPanel->click (p); return; }
            glassPanel->close(); panelImage = {}; repaint();
            return;
        }
        const auto* r = rowAt (p.y + scrollY);
        if (e.mods.isPopupMenu()) { showMenu (r != nullptr ? r->unit : -1); return; }
        dragControl = controlAt (p);
        draggingRack = dragControl < 0;
        dragStartScroll = scrollTarget;
        if (dragControl >= 0)
        {
            const auto& c = controls[(size_t) dragControl];
            dragParam = paramFor (c);
            if (dragParam < 0) { dragControl = -1; return; }
            if (c.kind == ControlKind::toggle || c.kind == ControlKind::button)
            {
                const float v = bridge.getNormalised (dragParam) > 0.5f ? 0.0f : 1.0f;
                bridge.beginGesture (dragParam, ControlSource::user);
                bridge.setValueWithSource (dragParam, v, ControlSource::user);
                bridge.endGesture (dragParam);
                dragControl = -1; repaint();
                return;
            }
            dragStartValue = bridge.getNormalised (dragParam);
            bridge.beginGesture (dragParam, ControlSource::user);
        }
    }

    void FlatRackView::mouseDrag (const juce::MouseEvent& e)
    {
        if (draggingRack)
        {
            scrollTarget = scrollY = std::clamp (dragStartScroll - (float) e.getDistanceFromDragStartY(), 0.0f, maxScroll());
            repaint();
            return;
        }
        if (dragControl < 0 || dragParam < 0) return;
        const auto& c = controls[(size_t) dragControl];
        const float range = e.mods.isShiftDown() ? 900.0f : 220.0f;
        float v = std::clamp (dragStartValue - (float) e.getDistanceFromDragStartY() / range, 0.0f, 1.0f);
        if (c.kind == ControlKind::selector)
            if (auto* prm = bridge.getParameter (dragParam); prm != nullptr && prm->getNumSteps() > 1 && prm->getNumSteps() < 64)
            {
                const float steps = (float) (prm->getNumSteps() - 1);
                v = std::round (v * steps) / steps;
            }
        bridge.setValueWithSource (dragParam, v, ControlSource::user);
        repaint();
    }

    void FlatRackView::mouseUp (const juce::MouseEvent& e)
    {
        if (dragControl >= 0 && dragParam >= 0)
        {
            const auto& c = controls[(size_t) dragControl];
            // a click on a selector (no drag): its next position
            if (c.kind == ControlKind::selector && e.getDistanceFromDragStart() < 3)
                if (auto* prm = bridge.getParameter (dragParam); prm != nullptr && prm->getNumSteps() > 1)
                {
                    const float steps = (float) (prm->getNumSteps() - 1);
                    float v = std::round (bridge.getNormalised (dragParam) * steps) + 1.0f;
                    if (v > steps) v = 0.0f;
                    bridge.setValueWithSource (dragParam, v / steps, ControlSource::user);
                }
            bridge.endGesture (dragParam);
        }
        dragControl = dragParam = -1;
        draggingRack = false;
        repaint();
    }

    void FlatRackView::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const int ci = controlAt (e.position);
        if (ci < 0) return;
        const int pi = paramFor (controls[(size_t) ci]);
        if (pi < 0 || controls[(size_t) ci].kind != ControlKind::knob) return;
        bridge.beginGesture (pi, ControlSource::user);
        bridge.setValueWithSource (pi, bridge.getDefaultNormalised (pi), ControlSource::user);
        bridge.endGesture (pi);
        repaint();
    }

    void FlatRackView::mouseMove (const juce::MouseEvent& e)
    {
        if (glassPanel->isOpen() && glassPanel->hover (e.position)) return;
        const int h = controlAt (e.position);
        if (h != hoverControl)
        {
            hoverControl = h;
            setMouseCursor (h >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
            if (h >= 0)
            {
                const auto& c = controls[(size_t) h];
                const int pi = paramFor (c);
                juce::String tip = juce::String (c.label);
                if (auto* prm = pi >= 0 ? bridge.getParameter (pi) : nullptr) tip << ": " << prm->getCurrentValueAsText();
                setTooltip (tip);
            }
            repaint();
        }
    }

    void FlatRackView::mouseExit (const juce::MouseEvent&)
    {
        glassPanel->unhover();
        if (hoverControl >= 0) { hoverControl = -1; repaint(); }
    }

    void FlatRackView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        if (glassPanel->isOpen() && glassPanel->getBounds().contains (e.position)) { glassPanel->scroll (w.deltaY * 220.0f); return; }
        scrollTarget = std::clamp (scrollTarget - w.deltaY * 260.0f, 0.0f, maxScroll());
    }

    bool FlatRackView::keyPressed (const juce::KeyPress& k)
    {
        if (glassPanel->isOpen() && glassPanel->keyPressed (k))
        {
            panelImage = toImage (glassPanel->render ((float) juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale), juce::Colours::white);
            repaint();
            return true;
        }
        return false;
    }
}
