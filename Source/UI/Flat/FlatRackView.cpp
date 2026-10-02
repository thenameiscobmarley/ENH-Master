#include "FlatRackView.h"
#include "../UnitFace.h"
#include "../../PluginProcessor.h"
#include "../../Parameters/ParameterBridge.h"
#include "../Scene/CustomLayout.h"
#include "../Scene/RoomScreen.h"
#include "../Scene/SimScreens.h"
#include "../Scene/BackPanels.h"
#include "../Scene/PatchCables.h"

namespace pad
{
    using namespace layout;
    using namespace face;   // (the faceplate painter: UI/UnitFace.h)

    //==============================================================================
    FlatRackView::FlatRackView (PluginProcessor& p) : processor (p), bridge (p.getBridge())
    {
        setOpaque (true);
        patchOpen = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_PATCHVIEW", {}).isNotEmpty();   // (dev: open on the patch bay)
        // Dev-only (screenshots, as the 3D view's): PAD_UI_TEST_STORED=<lo[:hi]> sets THE GEAR LOCKER (signed
        // 64-bit halves), PAD_UI_TEST_PARAMS="id=normalised;..." sets parameters
        if (const auto t = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_STORED", {}); t.isNotEmpty())
            processor.setStoredUnits (layout::UnitMask { (std::uint64_t) t.upToFirstOccurrenceOf (":", false, false).getLargeIntValue(),
                                                         (std::uint64_t) t.fromFirstOccurrenceOf (":", false, false).getLargeIntValue() });
        for (auto& token : juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_PARAMS", {}), ";", {}))
            if (const int i = bridge.indexOf (token.upToFirstOccurrenceOf ("=", false, false).trim()); i >= 0)
                bridge.setValueWithSource (i, token.fromFirstOccurrenceOf ("=", false, false).getFloatValue(), ControlSource::hostAutomation);
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
        if (juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_SCROLL", {}) == "end") scrollY = maxScroll();   // (screenshots: the rack's last units)
        scrollTarget = scrollY = std::clamp (scrollY, 0.0f, maxScroll());
        repaint();
    }

    /** One faceplate, drawn once (UI/UnitFace.h). */
    juce::Image FlatRackView::renderFace (int unit, float scale) const { return face::render (unit, scale); }
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
        if (key != "rayroom" && simscreen::kindOf (key) == simscreen::Kind::none && colourscreen::kindOf (key) == colourscreen::Kind::none)
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
        if (const auto ck = colourscreen::kindOf (enh::dsp::units::info[r.unit - firstGenUnit].key); ck != colourscreen::Kind::none)
        { drawColourScreen (g, r, box, ck); return; }
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
        g.setColour (juce::Colour (0xff06070a));   // (not pure black: see drawColourScreen)
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

    /** CHROMA SPACE's and HYPERCUBE's screens, in colour (ColourScreens.h): the same light as the 3D rack's. */
    void FlatRackView::drawColourScreen (juce::Graphics& g, const Row& r, juce::Rectangle<float> box, colourscreen::Kind kind) const
    {
        namespace C = colourscreen;
        const int power = bridge.indexOf (C::powerId (kind)), gk = r.unit - firstGenUnit;
        auto& slot = colourSlots[r.unit];
        // (never pure black: on some Linux desktops the 2D window's pure black shows through as transparent)
        g.setColour (juce::Colour (0xff06070a));
        g.fillRoundedRectangle (box, 3.0f);
        if (power < 0 || bridge.getNormalised (power) < 0.5f) { slot.clock = -1.0; slot.cv.fade (0.0f); return; }
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (slot.img.isNull() || slot.clock < 0.0 || now - slot.clock >= 1.0 / 30.0)
        {
            const float dt = slot.clock < 0.0 ? 1.0f / 30.0f : (float) std::clamp (now - slot.clock, 0.0, 0.25);
            slot.clock = now;
            const int w = std::clamp ((int) std::lround (box.getWidth()), 64, 448), h = std::clamp ((int) std::lround (box.getHeight()), 36, 258);
            slot.cv.resize (w, h);
            std::array<float, 192> st {};
            bool any = false;
            if (gk >= 0 && gk < enh::dsp::EngineMeters::displayUnits)
                for (size_t i = 0; i < st.size(); ++i) { st[i] = processor.getMeters().unitDisplay[(size_t) gk][i].load (std::memory_order_relaxed); any = any || st[i] != 0.0f; }
            if (! any) C::demo (kind, (float) demoTime, st.data());
            if (kind == C::Kind::chroma) C::chroma (slot.cv, st.data(), (float) demoTime);
            else if (kind == C::Kind::tuner) C::tunerCloud (slot.cv, (float) demoTime);
            else C::hypercube (slot.cv, slot.cube, st.data(), (float) demoTime, dt);
            slot.cv.toRgba();
            if (slot.img.getWidth() != w || slot.img.getHeight() != h) slot.img = juce::Image (juce::Image::RGB, w, h, false, juce::SoftwareImageType());
            juce::Image::BitmapData px (slot.img, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const auto* c = slot.cv.rgba.data() + (size_t) ((y * w + x) * 4);
                    px.setPixelColour (x, y, juce::Colour (std::max (c[0], (std::uint8_t) 6), std::max (c[1], (std::uint8_t) 7), std::max (c[2], (std::uint8_t) 10)));
                }
        }
        juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (box.toNearestInt());
        g.drawImage (slot.img, box, juce::RectanglePlacement::stretchToFit);
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
        if (patchOpen)
        {
            paintPatch (g);
            return;
        }
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
        tickPatch();
        glassPanel->tickTuner (1.0f / 30.0f);   // RACK TUNER: its glide and its faceplate's buttons, open or not
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
        m.addItem (6, "Patch bay - the rack's back, its cords", true, patchOpen);
        m.addItem (3, "Back to the top", true, false);
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe = juce::Component::SafePointer<FlatRackView> (this), unit] (int chosen)
        {
            if (safe == nullptr) return;
            if (chosen == 1) safe->openPanel (unit);
            if (chosen == 2) safe->openPanel (glass::lockerPage);
            if (chosen == 3) safe->scrollTarget = 0.0f;
            if (chosen == 6) { safe->patchOpen = ! safe->patchOpen; safe->heldCord = -1; safe->repaint(); }
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
        if (patchOpen && ! welcomeOpen && ! glassPanel->isOpen())
        {
            patchMouseDown (e);
            return;
        }
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
        mousePos = e.position;
        if (patchOpen) { if (heldCord >= 0) repaint(); return; }
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

    //==============================================================================
    // THE PATCH BAY
    enh::patch::State FlatRackView::currentPatch() const
    {
        auto s = processor.getPatch();
        if (s.cords.empty())
            s = enh::patch::straightThrough (backs::bay::unitsInOrder());
        return s;
    }

    juce::Rectangle<float> FlatRackView::bayRect() const
    {
        const float w = (float) getWidth() - 32.0f, h = w * bayHalfH / backs::bay::halfW();
        return { 16.0f, (float) getHeight() * 0.30f, w, h };
    }

    juce::Point<float> FlatRackView::jackPos (int col, int row) const
    {
        const auto r = bayRect();
        const float k = r.getWidth() / (2.0f * backs::bay::halfW());
        return { r.getX() + (backs::bay::halfW() - backs::bay::jackX (col)) * k, r.getY() + (backs::bay::rowZ (row) + bayHalfH) * k };
    }

    bool FlatRackView::jackAtPoint (juce::Point<float> p, int& col, int& row) const
    {
        const float pitch = jackPos (1, 0).x - jackPos (0, 0).x;
        for (row = 0; row < 2; ++row)
            for (col = 0; col < backs::bay::columns; ++col)
                if (p.getDistanceFrom (jackPos (col, row)) < 0.5f * pitch)
                    return true;
        return false;
    }

    juce::Rectangle<float> FlatRackView::masterRect() const { return { (float) getWidth() - 300.0f, 16.0f, 284.0f, 54.0f }; }
    juce::Rectangle<float> FlatRackView::patchCloseRect() const { return { 16.0f, 16.0f, 180.0f, 34.0f }; }

    void FlatRackView::paintPatch (juce::Graphics& g)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1712), 0.0f, 0.0f, juce::Colour (0xff0b0a09), 0.0f, (float) getHeight(), false));
        g.fillAll();
        const auto r = bayRect();
        const auto chain = backs::bay::unitsInOrder();
        if (bayImageWidth != (int) r.getWidth() || chain != bayImageChain)
        {
            bayImage = backs::bay::renderFaceImage (juce::jmax (256, (int) r.getWidth() * 2));
            bayImageWidth = (int) r.getWidth();
            bayImageChain = chain;
        }
        // the shelf it sits over, the bay
        g.setColour (juce::Colour (0xffcfccc6));
        g.fillRect (0.0f, r.getBottom() + r.getHeight() * 1.6f, (float) getWidth(), (float) getHeight());
        g.drawImage (bayImage, r);

        // the lamp: red - the chain is open (silent); amber - a loop
        {
            const auto lp = juce::Point<float> (r.getX() + (backs::bay::halfW() - backs::bay::lampX) * r.getWidth() / (2.0f * backs::bay::halfW()),
                                                r.getY() + (backs::bay::lampZ + bayHalfH) * r.getWidth() / (2.0f * backs::bay::halfW()));
            const bool red = processor.isPatchMuted(), amber = ! red && processor.isPatchLoop();
            const float lr = r.getHeight() * 0.045f;
            g.setColour (red ? juce::Colour (0xffff3020) : amber ? juce::Colour (0xffffb020) : juce::Colour (0xff3a1210));
            g.fillEllipse (lp.x - lr, lp.y - lr, 2 * lr, 2 * lr);
        }

        // the cords: from jack to jack, hanging under the bay (a loose end lies below its jack, the one in the hand
        // follows the pointer)
        const auto s = currentPatch();
        const float pitch = jackPos (1, 0).x - jackPos (0, 0).x;
        const float t = (float) juce::Time::getMillisecondCounterHiRes() * 0.001f;
        for (int k = 0; k < (int) s.cords.size(); ++k)
        {
            const auto& c = s.cords[(size_t) k];
            juce::Point<float> ends[2];
            bool inJack[2] {};
            float depth[2] { 1.0f, 1.0f };
            for (int e = 0; e < 2; ++e)
            {
                int col = 0, row = 0;
                if (k == heldCord && e == heldEnd)
                {
                    if (plugMove.cord == k)
                    {
                        ends[e] = jackPos (plugMove.col, plugMove.row);
                        const float u = juce::jlimit (0.0f, 1.0f, (float) ((juce::Time::getMillisecondCounterHiRes() - plugMove.start) / (1000.0 * plugMove.seconds)));
                        depth[e] = plugMove.inserting ? u : 1.0f - u;
                        inJack[e] = true;
                    }
                    else
                        ends[e] = mousePos;
                }
                else if (backs::bay::jackOf (chain, e == 0 ? c.a : c.b, col, row))
                {
                    ends[e] = jackPos (col, row);
                    inJack[e] = true;
                }
                else
                    ends[e] = { -1.0f, -1.0f };
            }
            for (int e = 0; e < 2; ++e)   // a loose end, not in the hand: on the shelf below the other end
                if (ends[e].x < 0.0f)
                    ends[e] = ends[1 - e].translated (pitch * 0.6f, r.getHeight() * 1.9f);
            const auto lin = geo::patch::cordColour (k / 2);
            const auto col = juce::Colour::fromFloatRGBA (std::sqrt (lin.x) * 1.15f, std::sqrt (lin.y) * 1.15f, std::sqrt (lin.z) * 1.15f, 1.0f);
            const float span = ends[0].getDistanceFrom (ends[1]);
            const float sag = r.getHeight() * 0.55f + span * 0.22f;
            const float sway = (k == heldCord ? 6.0f * std::sin (t * 5.0f) : 0.0f);
            juce::Path cord;
            cord.startNewSubPath (ends[0]);
            cord.cubicTo (ends[0].translated (sway, sag * 0.8f), ends[1].translated (sway, sag * 0.8f), ends[1]);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.strokePath (cord, juce::PathStrokeType (pitch * 0.32f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (2.0f, 4.0f));
            g.setColour (col.darker (0.4f));
            g.strokePath (cord, juce::PathStrokeType (pitch * 0.30f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (col);
            g.strokePath (cord, juce::PathStrokeType (pitch * 0.16f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          juce::AffineTransform::translation (-pitch * 0.04f, -pitch * 0.04f));
            for (int e = 0; e < 2; ++e)   // the plug: a nickel ferrule in the jack, the black body (smaller as it goes in)
            {
                const float scale = inJack[e] ? 1.0f + 0.35f * (1.0f - depth[e]) : 1.3f;
                const float pw = pitch * 0.42f * scale;
                g.setColour (juce::Colour (0xff111113));
                g.fillRoundedRectangle (juce::Rectangle<float> (pw, pw * 1.6f).withCentre (ends[e].translated (0.0f, pw * 0.5f)), pw * 0.3f);
                g.setColour (juce::Colour (0xffc8c9cc));
                g.fillEllipse (juce::Rectangle<float> (pw * 0.6f, pw * 0.6f).withCentre (ends[e]));
            }
        }

        // where the plug in the hand may go: an outline round each jack
        if (heldCord >= 0 && plugMove.cord < 0)
        {
            g.setColour (juce::Colour (0xff8ce8ff));
            for (int row = 0; row < 2; ++row)
                for (int col = 0; col < backs::bay::columns; ++col)
                    if (backs::bay::canGoInto (s, chain, heldCord, heldEnd, col, row))
                        g.drawEllipse (juce::Rectangle<float> (pitch * 0.62f, pitch * 0.62f).withCentre (jackPos (col, row)), 1.6f);
        }

        // the MASTER switch, and the way back
        {
            const auto m = masterRect();
            g.setColour (juce::Colour (0xff121316));
            g.fillRoundedRectangle (m, 4.0f);
            g.setColour (juce::Colour (0xffeceae4));
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText ("MASTER: ANYTHING INTO ANYTHING", m.withTrimmedLeft (54.0f).withTrimmedBottom (22.0f), juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xff8f9196));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (s.anything ? "ON - any plug into any jack, loops feed back" : "OFF - outputs into inputs", m.withTrimmedLeft (54.0f).withTrimmedTop (28.0f),
                        juce::Justification::centredLeft);
            const auto lever = juce::Rectangle<float> (14.0f, 30.0f).withCentre ({ m.getX() + 28.0f, m.getCentreY() });
            g.setColour (juce::Colour (0xff3a3b40));
            g.fillEllipse (lever.withSizeKeepingCentre (26.0f, 26.0f));
            g.setColour (juce::Colour (0xffd0d0d4));
            g.fillRoundedRectangle (lever.withHeight (16.0f).withY (s.anything ? lever.getY() : lever.getBottom() - 16.0f), 6.0f);
            const auto b = patchCloseRect();
            g.setColour (juce::Colour (0xff2a2b2f));
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            g.drawText ("< BACK TO THE FRONT", b, juce::Justification::centred);
            g.setColour (juce::Colour (0xff3a3b3e));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText ("Click a plug to take it out, then a jack to put it in. Click empty space to put it down.",
                        juce::Rectangle<float> (16.0f, (float) getHeight() - 30.0f, (float) getWidth() - 32.0f, 20.0f), juce::Justification::centredLeft);
        }
    }

    void FlatRackView::patchMouseDown (const juce::MouseEvent& e)
    {
        const auto p = e.position;
        mousePos = p;
        if (e.mods.isPopupMenu()) { showMenu (-1); return; }
        if (patchCloseRect().contains (p)) { patchOpen = false; heldCord = -1; repaint(); return; }
        if (plugMove.cord >= 0) return;
        auto s = currentPatch();
        if (masterRect().contains (p))
        {
            s.anything = ! s.anything;
            processor.setPatch (s);
            processor.patchClick();
            repaint();
            return;
        }
        const auto chain = backs::bay::unitsInOrder();
        int col = 0, row = 0;
        const bool onJack = jackAtPoint (p, col, row);
        auto start = [&] (int cord, int end, bool inserting)
        {
            plugMove = { cord, end, col, row, inserting, juce::Time::getMillisecondCounterHiRes(),
                         inserting ? 0.38f + 0.3f * plugRandom.nextFloat() : 0.22f };
            heldCord = cord;
            heldEnd = end;
            if (! inserting) processor.patchClick();
        };
        if (heldCord < 0)
        {
            if (! onJack) return;
            const auto port = backs::bay::portAt (chain, col, row);
            for (int k = 0; k < (int) s.cords.size(); ++k)
                for (int en = 0; en < 2; ++en)
                    if (const auto& end = en == 0 ? s.cords[(size_t) k].a : s.cords[(size_t) k].b; end.plugged() && end == port)
                    {
                        start (k, en, false);
                        return;
                    }
            for (int k = 0; k < (int) s.cords.size(); ++k)   // an empty jack: a loose plug goes in
                for (int en = 0; en < 2; ++en)
                    if (! (en == 0 ? s.cords[(size_t) k].a : s.cords[(size_t) k].b).plugged() && backs::bay::canGoInto (s, chain, k, en, col, row))
                    {
                        start (k, en, true);
                        return;
                    }
            return;
        }
        if (onJack)
        {
            if (backs::bay::canGoInto (s, chain, heldCord, heldEnd, col, row))
                start (heldCord, heldEnd, true);
            return;
        }
        heldCord = -1;   // put down: it lies below
        repaint();
    }

    void FlatRackView::tickPatch()
    {
        if (! patchOpen)
            return;
        // a unit put in or taken out: the cords follow
        if (heldCord < 0)
            if (auto s = processor.getPatch(); ! s.cords.empty() && enh::patch::reconcile (s, backs::bay::unitsInOrder()))
                processor.setPatch (s);
        repaint();
        if (plugMove.cord < 0)
            return;
        const float u = (float) juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - plugMove.start) / (1000.0 * plugMove.seconds));
        processor.setPatchNoise (u < 0.95f ? 0.9f * std::sin (juce::MathConstants<float>::pi * u) : 0.0f);   // (half in: a crackle)
        if (u < 1.0f)
            return;
        auto s = currentPatch();
        if (plugMove.cord < (int) s.cords.size())
        {
            auto& c = s.cords[(size_t) plugMove.cord];
            (plugMove.end == 0 ? c.a : c.b) = plugMove.inserting ? backs::bay::portAt (backs::bay::unitsInOrder(), plugMove.col, plugMove.row) : enh::patch::End {};
            processor.setPatch (s);
            if (plugMove.inserting) { heldCord = -1; processor.patchClick(); }
        }
        processor.setPatchNoise (0.0f);
        plugMove.cord = -1;
    }
}
