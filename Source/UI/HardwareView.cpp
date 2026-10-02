#include "SiteExport.h"
#include "HardwareView.h"
#include "Scene/CustomLayout.h"
#include "Scene/HardwareRenderer.h"
#include "Scene/CameraRig.h"
#include "Scene/DeviceLayout.h"
#include "Scene/Picking.h"
#include "Scene/BackPanels.h"
#include "Scene/UnitDescriptions.h"
#include "Scene/LayoutAudit.h"
#include "Scene/LimiterDemo.h"
#include "Controls/ControlBinding.h"
#include "../PluginProcessor.h"
#include "../DSP/MixBalancer.h"
#include "../DSP/FinalLimiter.h"

namespace pad
{
    using namespace layout;

    /** Buttons and bat toggles flip on click; knobs and the selector are dragged. */
    // THE GEAR LOCKER: the engine and the rack number the units alike
    static_assert (enhUnit == enh::dsp::rack::enhancer && tubeUnit == enh::dsp::rack::toneSpace && tideUnit == enh::dsp::rack::compressor
                   && lumenUnit == enh::dsp::rack::leveler && limiterUnit == enh::dsp::rack::limiter && levelUnit == enh::dsp::rack::level
                   && balancerUnit == enh::dsp::rack::balancer && deepUnit == enh::dsp::rack::deepSub && characterUnit == enh::dsp::rack::character
                   && radarUnit == enh::dsp::rack::radar && x4Unit == enh::dsp::rack::x4 && velvetUnit == enh::dsp::rack::velvet
                   && takebackUnit == enh::dsp::rack::takeback && scopeUnit == enh::dsp::rack::scope && customUnit == enh::dsp::rack::custom && numUnits == enh::dsp::rack::numUnits);

    static bool isSwitchLike (ControlKind k) noexcept { return k == ControlKind::button || k == ControlKind::toggle; }

    HardwareView::HardwareView (PluginProcessor& p)
        : processor (p),
          bridge (p.getBridge()),
          meters (p.getMeters()),
          config (UIConfig::loadOrCreate())
    {
        setOpaque (true);

        // SIMPLE or FULL (PAD_UI_TEST_VIEW=simple|full overrides it, for screenshots)
        {
            const auto viewTest = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_VIEW", {});
            const bool simple = viewTest.isNotEmpty() ? viewTest == "simple" : config.simpleView;
            hiddenUnits = simple ? simpleViewHidden : 0u;
        }
        // Dev-only: PAD_UI_TEST_STORED=<mask> sets THE GEAR LOCKER (screenshots of other rack line-ups)
        if (const auto t = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_STORED", {}); t.isNotEmpty())
            processor.setStoredUnits (layout::UnitMask { (std::uint64_t) t.upToFirstOccurrenceOf (":", false, false).getLargeIntValue(),
                                                         (std::uint64_t) t.fromFirstOccurrenceOf (":", false, false).getLargeIntValue() });   // (lo[:hi])
        storedUnits = processor.getStoredUnits();   // THE GEAR LOCKER, as the session left it
        // Dev-only: PAD_UI_TEST_LBSTORED=<mask> sets the LUNCHBOX's locker (screenshots of other line-ups)
        if (const auto t = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_LBSTORED", {}); t.isNotEmpty())
            processor.setStoredModules ((std::uint32_t) t.getLargeIntValue());
        seenStoredModules = processor.getStoredModules();
        layout::placeLunchbox (seenStoredModules);   // the LUNCHBOX's modules in their slots (before its print is drawn)
        // Dev-only: PAD_UI_TEST_CUSTOM=<share code> loads a design into the CUSTOM slot
        if (const auto t = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_CUSTOM", {}); t.isNotEmpty())
        {
            processor.setCustomCode (t, true);
            applyCustomDesign();   // (now, so the artwork dump below prints it)
        }

        artwork::TextureSet textures;
        textures.faceplateDecal = artwork::renderFaceplateDecal (config.panelTextureWidth, &textItems);
        textures.scale10 = artwork::renderKnobScale (1024, 10);
        textures.scale30 = artwork::renderKnobScale (1024, 30);
        textures.scale3 = artwork::renderKnobScale (1024, 3);
        textures.scale5 = artwork::renderKnobScale (1024, 5);
        textures.tubeDecal = artwork::renderTubeDecal (config.panelTextureWidth, &textItems);
        textures.seraphLabels = artwork::renderSeraphDisplayLabels (3072, &textItems);
        textures.tideDecal = artwork::renderOneUDecal (tideUnit, config.panelTextureWidth, &textItems);
        textures.lumenDecal = artwork::renderOneUDecal (lumenUnit, config.panelTextureWidth, &textItems);
        textures.tideVuFace = artwork::renderVuFace (tideUnit, 1536, &textItems);
        textures.lumenVuFace = artwork::renderVuFace (lumenUnit, 1024, &textItems);
        textures.limiterDecal = artwork::renderOneUDecal (limiterUnit, config.panelTextureWidth, &textItems);
        textures.limiterVuFace[0] = artwork::renderVuFace (limiterUnit, 1024, &textItems, 0);
        textures.limiterVuFace[1] = artwork::renderVuFace (limiterUnit, 1024, &textItems, 1);
        textures.deepDecal = artwork::renderOneUDecal (deepUnit, config.panelTextureWidth, &textItems);
        textures.deepVuFace = artwork::renderVuFace (deepUnit, 1536, &textItems);
        textures.characterDecal = artwork::renderOneUDecal (characterUnit, config.panelTextureWidth, &textItems);
        textures.characterVuFace = artwork::renderVuFace (characterUnit, 1024, &textItems);
        textures.radarDecal = artwork::renderOneUDecal (radarUnit, config.panelTextureWidth, &textItems);
        textures.radarVuFace = artwork::renderVuFace (radarUnit, 1024, &textItems);
        textures.powerDecal = artwork::renderOneUDecal (powerUnit, config.panelTextureWidth, &textItems);
        textures.lunchboxDecal = artwork::renderLunchboxDecal (config.panelTextureWidth / 2, &textItems);
        textures.lunchboxVuFace = artwork::renderVuFace (lunchboxUnit, 512, &textItems);
        textures.x4Decal = artwork::renderDesignedDecal (x4Unit, config.panelTextureWidth, &textItems);
        textures.x4VuFace = artwork::renderVuFace (x4Unit, 512, &textItems);
        textures.x4Screens = artwork::renderDesignedScreens (x4Unit, config.panelTextureWidth / 2);
        textures.velvetDecal = artwork::renderDesignedDecal (velvetUnit, config.panelTextureWidth, &textItems);
        textures.velvetVuFace = artwork::renderVuFace (velvetUnit, 1024, &textItems);
        textures.velvetScreens = artwork::renderDesignedScreens (velvetUnit, config.panelTextureWidth / 2);
        textures.takebackDecal = artwork::renderDesignedDecal (takebackUnit, config.panelTextureWidth, &textItems);
        for (int m = 0; m < 4; ++m)
            textures.takebackVuFace[(size_t) m] = artwork::renderVuFace (takebackUnit, 512, &textItems, m);
        textures.takebackScreens = artwork::renderDesignedScreens (takebackUnit, config.panelTextureWidth / 2);
        textures.scopeDecal = artwork::renderDesignedDecal (scopeUnit, config.panelTextureWidth, &textItems);
        textures.scopeScreens = artwork::renderDesignedScreens (scopeUnit, config.panelTextureWidth / 2);
        // The newer units: only those in the rack now (a unit in the locker is baked when it is installed -
        // with 53 of them, baking all took seconds and 100+ MB); the layout audit wants every one
        const bool bakeAll = juce::SystemStats::getEnvironmentVariable ("PAD_UI_DUMP_ARTWORK", {}).isNotEmpty();
        genBaked.assign ((size_t) gen::count, false);
        for (int k = 0; k < gen::count; ++k)
        {
            const bool now = bakeAll || ! layout::isStored (firstGenUnit + k);
            textures.genDecal.push_back (now ? artwork::renderDesignedDecal (firstGenUnit + k, config.panelTextureWidth, &textItems) : artwork::RawTexture {});
            textures.genScreens.push_back (now ? artwork::renderDesignedScreens (firstGenUnit + k, config.panelTextureWidth / 2) : artwork::RawTexture {});
            genBaked[(size_t) k] = now;
        }
        textures.levelDecal = artwork::renderOneUDecal (levelUnit, config.panelTextureWidth, &textItems);
        textures.balancerDecal = artwork::renderOneUDecal (balancerUnit, config.panelTextureWidth, &textItems);
        textures.monitorDecal = artwork::renderOneUDecal (monitorUnit, config.panelTextureWidth, &textItems);
        textures.levelVuFace = artwork::renderVuFace (levelUnit, 1536, &textItems);
        textures.monitorVuFace[0] = artwork::renderVuFace (monitorUnit, 1024, &textItems, 0);
        textures.monitorVuFace[1] = artwork::renderVuFace (monitorUnit, 1024, &textItems, 1);
        textures.monitorLabels = artwork::renderWindowLabels (monitorUnit, 3072, &textItems);
        textures.balancerLabels = artwork::renderWindowLabels (balancerUnit, 3072, &textItems);
        scopeAnalyser.prepare (p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0);
        balancerAnalyser.prepare (p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0);
        waveReader.prepare (p.getSampleRate() > 0.0 ? p.getSampleRate() : 48000.0);
        artwork::collectKnobScaleText (textItems);

        // Dev-only: PAD_UI_DUMP_ARTWORK=<dir> writes the printed panels with measured clearances
        const auto dumpDir = juce::SystemStats::getEnvironmentVariable ("PAD_UI_DUMP_ARTWORK", {});
        // Dev-only: PAD_UI_EXPORT_SITE=<dir> writes the website's units gallery pictures (SiteExport.h) and closes
        if (const auto siteDir = juce::SystemStats::getEnvironmentVariable ("PAD_UI_EXPORT_SITE", {}); siteDir.isNotEmpty())
        {
            siteexport::write (juce::File (siteDir));
            juce::MessageManager::callAsync ([] { if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        }
        if (dumpDir.isNotEmpty())
        {
            audit::writeLayoutAudit (textures, textItems, juce::File (dumpDir));
            // (PAD_UI_DUMP_QUIT: the self-test's audit - nothing more to wait for, so the app closes)
            if (juce::SystemStats::getEnvironmentVariable ("PAD_UI_DUMP_QUIT", {}).isNotEmpty())
                juce::MessageManager::callAsync ([] { if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        }

        renderer = std::make_unique<HardwareRenderer> (bridge, shared, meters, config, std::move (textures), scopeCurve,
                                                       balancerCurve, displayHistory);

        juce::OpenGLPixelFormat format;
        format.depthBufferBits = 24;
        format.multisamplingLevel = config.msaaSamples;
        glContext.setPixelFormat (format);
        glContext.setMultisamplingEnabled (config.msaaSamples > 0);
        glContext.setPreferredVersion ({ 3, 2 });
        glContext.setComponentPaintingEnabled (false);
        glContext.setRenderer (renderer.get());
        glContext.setContinuousRepainting (true); // paced on the render thread, locked to vsync
        glContext.attachTo (*this);

        // Dev-only: PAD_UI_TEST_FOCUS=<unit index> starts walked up to that unit (close-up screenshots)
        const auto focusTest = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_FOCUS", {});
        if (focusTest.isNotEmpty())
            setFocus (juce::jlimit (0, numUnits - 1, focusTest.getIntValue()), 1.0f);
        // Dev-only: PAD_UI_TEST_TURN=<0..1> starts that far round behind the rack
        // Dev-only: PAD_UI_TEST_BACK=<unit> starts walked up to that unit's back (with PAD_UI_TEST_TURN=1)
        if (const auto t = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_BACK", {}); t.isNotEmpty())
        {
            shared.focusBack = t.getIntValue();
            shared.focusTarget = 1.0f;
        }
        // Dev-only: PAD_UI_TEST_PATCH="col,row" pulls the plug out of that bay jack 2 s in (turned round)
        if (const auto t = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_PATCH", {}); t.isNotEmpty())
            juce::Timer::callAfterDelay (2000, [safe = juce::Component::SafePointer<HardwareView> (this), t]
            {
                if (safe == nullptr) return;
                safe->lastPointer = safe->getLocalBounds().toFloat().getCentre().translated (120.0f, -40.0f);
                safe->jackClicked (t.upToFirstOccurrenceOf (",", false, false).getIntValue(), t.fromFirstOccurrenceOf (",", false, false).getIntValue());
            });
        if (const auto turnTest = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_TURN", {}); turnTest.isNotEmpty())
            shared.turnTarget = juce::jlimit (0.0f, 1.0f, turnTest.getFloatValue());

        glassPanel = std::make_unique<GlassPanel> (bridge, processor);
        if (juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_HOLO", {}).isNotEmpty())   // (dev: the HOLOGRAM panel)
            config.holoPanel = true;
        glassPanel->setHolo (config.holoPanel);
        shared.panelHolo = config.holoPanel;

        // The welcome screen: at start (unless switched off), and once after an update in any case. Not in
        // the screenshot runs (PAD_UI_TEST_SIZE) unless asked for (PAD_UI_TEST_WELCOME)
        welcome.version = JucePlugin_VersionString;
        welcome.showAtStart = config.showWelcome;
        welcome.holoPanel = config.holoPanel;
        {
            const bool testing = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_SIZE", {}).isNotEmpty();
            const bool asked = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_WELCOME", {}).isNotEmpty();
            if (asked || (! testing && (config.showWelcome || config.welcomeSeen != welcome.version)))
                showWelcome (true);
        }

        // Dev-only: PAD_UI_TEST_PANEL=<unit>[,<dropdown>[,<choice>]] opens a unit's glass panel, optionally
        // with a dropdown expanded and one of its choices hovered (screenshots, frame-time checks)
        const auto panelTest = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_PANEL", {});
        if (panelTest.isNotEmpty())
        {
            const auto parts = juce::StringArray::fromTokens (panelTest, ",", {});
            juce::Component::SafePointer<HardwareView> safe (this);
            juce::Timer::callAfterDelay (1200, [safe, parts]
            {
                if (safe == nullptr)
                    return;
                safe->openPanel (parts[0].getIntValue() == glass::lockerPage ? glass::lockerPage : juce::jlimit (0, numUnits - 1, parts[0].getIntValue()));
                // (PAD_UI_TEST_SEARCH: typed into THE GEAR LOCKER's search)
                for (auto c : juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_SEARCH", {}))
                    safe->glassPanel->keyPressed (juce::KeyPress (0, 0, c));
                // (PAD_UI_TEST_TUNE: and Enter - RACK TUNER tunes to what was typed)
                if (juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_TUNE", {}).isNotEmpty())
                    juce::Timer::callAfterDelay (1500, [safe] { if (safe != nullptr) safe->glassPanel->keyPressed (juce::KeyPress (juce::KeyPress::returnKey)); });   // (after PAD_UI_TEST_PARAMS)
                if (parts.size() > 1 && parts[1].startsWith ("t"))   // (",t2": show that tab; ",t1,h3": and hover its 4th unit)
                {
                    safe->glassPanel->selectTab (parts[1].substring (1).getIntValue());
                    if (parts.size() > 2 && parts[2].startsWith ("h"))
                        safe->glassPanel->testHoverUnit (parts[2].substring (1).getIntValue());
                }
                else if (parts.size() > 1)
                    safe->glassPanel->setExpanded (parts[1].getIntValue(), parts.size() > 2 ? parts[2].getIntValue() : -1);
                safe->publishPanel (true);
            });
            // Dev only: PAD_UI_TEST_PANEL_CLOSE=<ms> closes it again that long after it opened
            const int closeMs = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_PANEL_CLOSE", "0").getIntValue();
            if (closeMs > 0)
                juce::Timer::callAfterDelay (1200 + closeMs, [safe] { if (safe != nullptr) safe->openPanel (-1); });
        }

        openedAtMs = juce::Time::getMillisecondCounter();
        refreshOverlay();
        startTimerHz (30); // display text + hover callouts
    }

    HardwareView::~HardwareView()
    {
        stopTimer();

        if (dragParam >= 0)
            bridge.endGesture (dragParam);

        glContext.detach();
        renderer.reset();
    }

    void HardwareView::resized()
    {
        shared.viewWidth = juce::jmax (1, getWidth());
        shared.viewHeight = juce::jmax (1, getHeight());
        publishWindowGeometry();
        if (glassPanel != nullptr)
        {
            glassPanel->setViewSize (getLocalBounds().toFloat());
            publishPanel (true);
        }
    }

    //==============================================================================
    void HardwareView::showViewMenu()
    {
        const bool simple = hiddenUnits.load() != 0u;
        juce::PopupMenu m;
        m.addSectionHeader ("THE RACK");
        m.addItem (1, "Simple view - the 5 units you use", true, simple);
        m.addItem (2, "Full rack - every unit installed", true, ! simple);
        m.addSeparator();
        m.addItem (4, "Gear locker...  (swap units in and out of the rack)", true, glassPanel->getUnit() == glass::lockerPage);
        m.addItem (7, "Turn the rack round - its back, the patch bay  (or drag on empty space)", true, shared.turnTarget.load() > 0.5f);
        m.addItem (5, "Welcome screen...");
        m.addItem (6, "Hologram settings panel", true, config.holoPanel);
        m.addSeparator();
        m.addItem (3, "Units put away keep working, as the preset set them", false, false);
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                         [safe = juce::Component::SafePointer<HardwareView> (this)] (int chosen)
                         {
                             if (safe != nullptr && (chosen == 1 || chosen == 2))
                                 safe->setSimpleView (chosen == 1);
                             if (safe != nullptr && chosen == 4)
                                 safe->openPanel (glass::lockerPage);
                             if (safe != nullptr && chosen == 7)
                                 safe->setTurned (safe->shared.turnTarget.load() < 0.5f);
                             if (safe != nullptr && chosen == 5)
                                 safe->showWelcome (true);
                             if (safe != nullptr && chosen == 6)
                             {
                                 safe->config.holoPanel = ! safe->config.holoPanel;
                                 safe->welcome.holoPanel = safe->config.holoPanel;
                                 UIConfig::saveKey ("holoPanel", safe->config.holoPanel);
                                 safe->glassPanel->setHolo (safe->config.holoPanel);
                                 safe->shared.panelHolo = safe->config.holoPanel;
                                 safe->publishPanel (true);
                             }
                         });
    }

    //==============================================================================
    // THE PATCH BAY
    enh::patch::State HardwareView::currentPatch() const
    {
        auto s = processor.getPatch();
        if (s.cords.empty())
            s = enh::patch::straightThrough (backs::bay::unitsInOrder());
        return s;
    }

    bool HardwareView::bayJackAt (juce::Point<float> pos, int& col, int& row) const
    {
        if (shared.turnAmount.load() < 0.9f)
            return false;
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load(), shared.focus());
        const auto inv = layout::turnMatrixInverse (shared.turnAmount.load());
        const auto o = inv.transformPoint (cam.eye);
        const auto d = hwk::gfx::normalise (inv.transformDir (cam.rayDirection (2.0f * pos.x / w - 1.0f, 1.0f - 2.0f * pos.y / h)));
        const auto B = layout::bayToWorld();
        const auto p0 = B.transformPoint ({ 0.0f, backs::bay::faceY(), 0.0f });
        const auto n = geo::patch::bayOut();
        const float den = hwk::gfx::dot (d, n);
        if (std::abs (den) < 1.0e-4f)
            return false;
        const float t = hwk::gfx::dot (p0 - o, n) / den;
        if (t <= 0.0f)
            return false;
        const auto hit = o + d * t - p0;
        const auto ax = hwk::gfx::normalise (B.transformDir ({ 1.0f, 0.0f, 0.0f })), az = hwk::gfx::normalise (B.transformDir ({ 0.0f, 0.0f, 1.0f }));
        const float lx = hwk::gfx::dot (hit, ax), lz = hwk::gfx::dot (hit, az);
        const float pitch = backs::bay::jackX (0) - backs::bay::jackX (1);
        const float c = (backs::bay::jackX (0) - lx) / pitch;
        col = (int) std::lround (c);
        row = std::abs (lz - backs::bay::rowZ (0)) < std::abs (lz - backs::bay::rowZ (1)) ? 0 : 1;
        return col >= 0 && col < backs::bay::columns && std::abs (c - (float) col) < 0.48f && std::abs (lz - backs::bay::rowZ (row)) < 0.075f;
    }

    int HardwareView::backUnitAt (juce::Point<float> pos) const
    {
        if (shared.turnAmount.load() < 0.9f)
            return -1;
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load(), shared.focus());
        const auto inv = layout::turnMatrixInverse (shared.turnAmount.load());
        const auto o = inv.transformPoint (cam.eye);
        const auto d = hwk::gfx::normalise (inv.transformDir (cam.rayDirection (2.0f * pos.x / w - 1.0f, 1.0f - 2.0f * pos.y / h)));
        int best = -1;
        float bestT = 1.0e9f;
        for (int u : rackOrder)
        {
            if (! isShown (u))
                continue;
            const auto P = panelToWorld (u);
            const auto p0 = P.transformPoint ({ 0.0f, backs::plateY(), 0.0f });
            const auto n = hwk::gfx::normalise (P.transformDir ({ 0.0f, -1.0f, 0.0f }));
            const float den = hwk::gfx::dot (d, n);
            if (std::abs (den) < 1.0e-4f)
                continue;
            const float t = hwk::gfx::dot (p0 - o, n) / den;
            if (t <= 0.0f || t >= bestT)
                continue;
            const auto hit = o + d * t - p0;
            const float lx = hwk::gfx::dot (hit, hwk::gfx::normalise (P.transformDir ({ 1.0f, 0.0f, 0.0f })));
            const float lz = hwk::gfx::dot (hit, hwk::gfx::normalise (P.transformDir ({ 0.0f, 0.0f, 1.0f })));
            if (std::abs (lx) < chassisHalfW && std::abs (lz) < unitHalfH (u) + 0.5f * rackGap)
            {
                best = u;
                bestT = t;
            }
        }
        return best;
    }

    bool HardwareView::masterAt (juce::Point<float> pos) const
    {
        // The MASTER switch's plate: the ray (rack space) through its face
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load(), shared.focus());
        const auto inv = layout::turnMatrixInverse (shared.turnAmount.load());
        const auto o = inv.transformPoint (cam.eye);
        const auto d = hwk::gfx::normalise (inv.transformDir (cam.rayDirection (2.0f * pos.x / w - 1.0f, 1.0f - 2.0f * pos.y / h)));
        const auto M = geo::patch::masterFrame();
        const auto p0 = M.transformPoint ({ 0.0f, 0.0f, 0.0f });
        const auto n = hwk::gfx::normalise (M.transformDir ({ 0.0f, 1.0f, 0.0f }));
        const float den = hwk::gfx::dot (d, n);
        if (std::abs (den) < 1.0e-4f)
            return false;
        const float t = hwk::gfx::dot (p0 - o, n) / den;
        if (t <= 0.0f)
            return false;
        const auto hit = o + d * t - p0;
        const float lx = hwk::gfx::dot (hit, hwk::gfx::normalise (M.transformDir ({ 1.0f, 0.0f, 0.0f })));
        const float lz = hwk::gfx::dot (hit, hwk::gfx::normalise (M.transformDir ({ 0.0f, 0.0f, 1.0f })));
        return std::abs (lx) < geo::patch::masterHalfW + 0.05f && std::abs (lz) < geo::patch::masterHalfH + 0.15f;
    }

    hwk::gfx::Vec3 HardwareView::handAt (juce::Point<float> pos) const
    {
        // On a plane just out from the bay's face: the plug held up to the jacks
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load(), shared.focus());
        const auto inv = layout::turnMatrixInverse (shared.turnAmount.load());
        const auto o = inv.transformPoint (cam.eye);
        const auto d = hwk::gfx::normalise (inv.transformDir (cam.rayDirection (2.0f * pos.x / w - 1.0f, 1.0f - 2.0f * pos.y / h)));
        const auto n = geo::patch::bayOut();
        const auto p0 = layout::bayToWorld().transformPoint ({ 0.0f, backs::bay::faceY(), 0.0f }) + n * 0.30f;
        const float den = hwk::gfx::dot (d, n);
        const float t = std::abs (den) < 1.0e-4f ? 8.0f : std::max (0.5f, hwk::gfx::dot (p0 - o, n) / den);
        return o + d * t;
    }

    bool HardwareView::canGoInto (const enh::patch::State& s, int cord, int end, int col, int row) const
    {
        const auto chain = backs::bay::unitsInOrder();
        const auto port = backs::bay::portAt (chain, col, row);
        if (! enh::patch::isFree (s, port))
            return false;
        if (s.anything)
            return true;
        const auto& c = s.cords[(size_t) cord];
        const auto& other = end == 0 ? c.b : c.a;
        if (! other.plugged())
            return true;
        return other.isOut() ? row == 1 : row == 0;   // an OUT goes into an IN, and the other way round
    }

    void HardwareView::setValidJacks()
    {
        std::uint64_t valid[2] {};
        if (heldCord >= 0 && plugMove.cord < 0)
        {
            const auto s = currentPatch();
            if (heldCord < (int) s.cords.size())
                for (int row = 0; row < 2; ++row)
                    for (int col = 0; col < backs::bay::columns; ++col)
                        if (canGoInto (s, heldCord, heldEnd, col, row))
                            valid[row] |= std::uint64_t { 1 } << col;
        }
        shared.validTop = valid[0];
        shared.validBottom = valid[1];
    }

    void HardwareView::startInsert (int cord, int end, int col, int row)
    {
        plugMove = { cord, end, col, row, true, juce::Time::getMillisecondCounterHiRes(), 0.38f + 0.32f * plugRandom.nextFloat(), {} };
        for (auto& g : plugMove.grip)
            g = 0.30f + 0.70f * plugRandom.nextFloat();   // how freely it slides along each stretch
        heldCord = cord;
        heldEnd = end;
        shared.heldCord = cord;
        shared.heldEnd = end;
        shared.heldJackCol = col;
        shared.heldJackRow = row;
        shared.plugDepth = 0.0f;
        shared.validTop = 0;
        shared.validBottom = 0;
    }

    bool HardwareView::patchMouseDown (juce::Point<float> pos)
    {
        if (shared.turnAmount.load() < 0.9f)
            return false;
        if (plugMove.cord >= 0)
            return true;   // (one at a time: a plug is on its way)
        if (masterAt (pos))
        {
            auto s = currentPatch();
            s.anything = ! s.anything;
            processor.setPatch (s);
            processor.patchClick();
            setValidJacks();
            return true;
        }
        int col = 0, row = 0;
        const bool onJack = bayJackAt (pos, col, row);
        const auto s = currentPatch();

        if (heldCord < 0)
        {
            if (! onJack)
                return false;
            return jackClicked (col, row);
        }

        // A plug in the hand: into a jack it may go into; anywhere else it is put down (it lies on the shelf)
        if (onJack)
        {
            if (heldCord < (int) s.cords.size() && canGoInto (s, heldCord, heldEnd, col, row))
                startInsert (heldCord, heldEnd, col, row);
            return true;
        }
        heldCord = -1;
        shared.heldCord = -1;
        setValidJacks();
        return true;
    }

    bool HardwareView::jackClicked (int col, int row)
    {
        auto s = currentPatch();
        const auto chain = backs::bay::unitsInOrder();
        {
            const auto port = backs::bay::portAt (chain, col, row);
            for (int k = 0; k < (int) s.cords.size(); ++k)
                for (int e = 0; e < 2; ++e)
                    if (const auto& end = e == 0 ? s.cords[(size_t) k].a : s.cords[(size_t) k].b; end.plugged() && end == port)
                    {
                        // Out it comes: it slides out (a crackle as it goes), then it is in your hand
                        plugMove = { k, e, col, row, false, juce::Time::getMillisecondCounterHiRes(), 0.20f + 0.08f * plugRandom.nextFloat(), {} };
                        heldCord = k;
                        heldEnd = e;
                        shared.heldCord = k;
                        shared.heldEnd = e;
                        shared.heldJackCol = col;
                        shared.heldJackRow = row;
                        shared.plugDepth = 1.0f;
                        processor.patchClick();
                        return true;
                    }
            // An empty jack: a plug lying loose goes into it (the nearest one that may)
            int best = -1, bestEnd = 0;
            float bestDist = 1.0e9f;
            for (int k = 0; k < (int) s.cords.size(); ++k)
                for (int e = 0; e < 2; ++e)
                {
                    const auto& c = s.cords[(size_t) k];
                    if ((e == 0 ? c.a : c.b).plugged() || ! canGoInto (s, k, e, col, row))
                        continue;
                    int oc = 0, orow = 0;
                    const float dist = backs::bay::jackOf (chain, e == 0 ? c.b : c.a, oc, orow) ? std::abs ((float) (oc - col)) : 100.0f;
                    if (dist < bestDist) { bestDist = dist; best = k; bestEnd = e; }
                }
            if (best >= 0)
                startInsert (best, bestEnd, col, row);
            return true;
        }
    }

    void HardwareView::tickPatch()
    {
        // The processor's cords to the renderer
        auto sync = [this]
        {
            if (const int v = processor.getPatchVersion(); v != seenPatchVersion)
            {
                seenPatchVersion = v;
                {
                    const juce::SpinLock::ScopedLockType lock (shared.patchLock);
                    shared.patchCords = processor.getPatch();
                }
                shared.patchVersion.fetch_add (1);
                shared.patchAnything = processor.getPatch().anything;
            }
        };
        sync();
        shared.patchMuted = processor.isPatchMuted();
        shared.patchLoop = processor.isPatchLoop();
        // A unit put into the rack or taken out: the cords follow (not while a plug is in the hand)
        if (heldCord < 0)
            if (auto s = processor.getPatch(); ! s.cords.empty() && enh::patch::reconcile (s, backs::bay::unitsInOrder()))
                processor.setPatch (s);

        if (heldCord >= 0 && plugMove.cord < 0)
        {
            const auto p = handAt (lastPointer);
            shared.heldX = p.x; shared.heldY = p.y; shared.heldZ = p.z;
        }
        if (plugMove.cord < 0)
            return;

        const float t = (float) juce::jlimit (0.0, 1.0, (juce::Time::getMillisecondCounterHiRes() - plugMove.start) / (1000.0 * plugMove.seconds));
        if (! plugMove.inserting)
        {
            shared.plugDepth = 1.0f - t * t * (3.0f - 2.0f * t);
            processor.setPatchNoise (0.9f * std::sin (juce::MathConstants<float>::pi * t));
        }
        else
        {
            // along the way it grips and gives (each stretch its own), then seats with a click
            float total = 0.0f, done = 0.0f;
            const float at = t * (float) plugMove.grip.size();
            for (size_t i = 0; i < plugMove.grip.size(); ++i)
            {
                total += plugMove.grip[i];
                done += plugMove.grip[i] * juce::jlimit (0.0f, 1.0f, at - (float) i);
            }
            shared.plugDepth = done / total;
            processor.setPatchNoise (t < 0.92f ? 0.9f * std::sin (juce::MathConstants<float>::pi * juce::jmin (1.0f, t * 1.1f)) : 0.0f);
        }
        if (t < 1.0f)
            return;

        auto s = currentPatch();
        if (plugMove.cord < (int) s.cords.size())
        {
            auto& c = s.cords[(size_t) plugMove.cord];
            auto& end = plugMove.end == 0 ? c.a : c.b;
            if (plugMove.inserting)
                end = backs::bay::portAt (backs::bay::unitsInOrder(), plugMove.col, plugMove.row);
            else
            {
                end = {};   // out: in the hand
                const auto p = handAt (lastPointer);
                shared.heldX = p.x; shared.heldY = p.y; shared.heldZ = p.z;
            }
            processor.setPatch (s);
            sync();   // (the renderer has the new cords before it lets go of this one)
            if (plugMove.inserting)
            {
                heldCord = -1;
                shared.heldCord = -1;
                processor.patchClick();
            }
        }
        processor.setPatchNoise (0.0f);
        shared.heldJackCol = -1;
        plugMove.cord = -1;
        setValidJacks();
    }

    void HardwareView::setTurned (bool back)
    {
        if (back)
        {
            if (glassPanel->isOpen())
                openPanel (-1);
            setFocus (-1, 0.0f);
        }
        shared.turnTarget = back ? 1.0f : 0.0f;
    }

    void HardwareView::setSimpleView (bool simple)
    {
        hiddenUnits = simple ? simpleViewHidden : 0u;
        if (glassPanel->isOpen() && glassPanel->getUnit() < numUnits && ! isShown (glassPanel->getUnit()))
            openPanel (-1);
        if (! isShown (shared.focusUnit.load()))
            shared.focusUnit = enhUnit;   // it was walked up to a unit now put away: to the enhancer instead
        config.simpleView = simple;
        UIConfig::saveSimpleView (simple);
    }

    void HardwareView::openPanel (int unit)
    {
        // Level with the unit on screen (its centre, projected), so the line from it runs short
        float anchorY = 0.5f * (float) getHeight();
        if (unit >= 0 && unit < numUnits)
        {
            const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
            const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load(),
                                               shared.focus());
            float ax = 0.0f, ay = 0.0f;
            if (gfx::projectToNdc (cam.viewProj, panelToWorld (unit).transformPoint ({ 0.0f, 0.0f, 0.0f }), ax, ay))
                anchorY = (1.0f - ay) * 0.5f * h;
        }
        glassPanel->open (unit, anchorY, getLocalBounds().toFloat());
        publishPanel (true);
        // THE GEAR LOCKER takes the keyboard (its search): ask for it while it is open
        setWantsKeyboardFocus (glassPanel->wantsKeys());
        if (glassPanel->wantsKeys()) grabKeyboardFocus();
    }

    /** Hands the panel's rectangle and, when it changed, its print to the renderer. */
    void HardwareView::publishPanel (bool force)
    {
        glassPanel->pollValues();
        const auto r = glassPanel->getBounds();
        shared.panelX = r.getX();
        shared.panelY = r.getY();
        shared.panelW = r.getWidth();
        shared.panelH = r.getHeight();
        shared.panelUnit = glassPanel->getUnit();
        if (! glassPanel->isOpen() || (! force && ! glassPanel->needsRedraw()))
            return;

        const float scale = juce::jmax (1.0f, shared.platformScale.load()) * 2.0f;   // print at 2x: crisp text
        auto tex = glassPanel->render (scale);
        const juce::SpinLock::ScopedLockType lock (shared.panelLock);
        shared.panelPending = std::move (tex);
        shared.panelPendingRect = r;
        ++shared.panelVersion;
    }

    void HardwareView::publishWindowGeometry()
    {
        if (auto* peer = getPeer())
        {
            const auto origin = peer->getComponent().getLocalPoint (this, juce::Point<int>());
            shared.viewOffsetX = origin.x;
            shared.viewOffsetY = origin.y;
            shared.platformScale = (float) peer->getPlatformScaleFactor();
            shared.nativeWindow = (juce::uint64) (juce::pointer_sized_uint) peer->getNativeHandle();
        }
        else
        {
            shared.nativeWindow = 0;
        }
    }

    //==============================================================================
    int HardwareView::paramIndexForControl (int i) const
    {
        return boundParameter (bridge, i);
    }

    int HardwareView::pickControl (juce::Point<float> pos) const
    {
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load(),
                                           shared.focus());
        return pad::pickControl (cam, 2.0f * pos.x / w - 1.0f, 1.0f - 2.0f * pos.y / h);
    }

    int HardwareView::unitUnderPointer (juce::Point<float> pos) const
    {
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load(),
                                           shared.focus());
        const float ndcX = 2.0f * pos.x / w - 1.0f, ndcY = 1.0f - 2.0f * pos.y / h;

        for (int unit = 0; unit < numUnits; ++unit)
        {
            float lx = 0.0f, lz = 0.0f;
            if (cam.intersectUnit (unit, ndcX, ndcY, 0.0f, lx, lz)
                && std::abs (lx) <= unitHalfW (unit) && std::abs (lz) <= unitHalfH (unit))
                return unit;
        }

        return -1;
    }

    /** Walk toward a unit, or step back to see the whole rack. */
    void HardwareView::setFocus (int unit, float amount)
    {
        if (unit >= 0)
            shared.focusUnit = unit;
        shared.focusTarget = juce::jlimit (0.0f, 1.0f, amount);
    }

    void HardwareView::updateMouse (juce::Point<float> pos)
    {
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        shared.mouseNdcX = juce::jlimit (-1.0f, 1.0f, 2.0f * pos.x / w - 1.0f);
        shared.mouseNdcY = juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * pos.y / h);
        shared.mouseInside = true;
    }

    //==============================================================================
    void HardwareView::mouseEnter (const juce::MouseEvent& e) { mouseMove (e); }

    void HardwareView::mouseMove (const juce::MouseEvent& e)
    {
        updateMouse (e.position);
        lastPointer = e.position;
        // turned round: what is under the pointer, and who made it
        {
            const int back = backUnitAt (e.position);
            if (back != tipBack)
            {
                tipBack = back;
                setTooltip (back < 0 ? juce::String() : descriptions::tooltip (back, true));
            }
        }
        if (glassPanel->hitTest (e.position).inside)
        {
            // Over the glass: its own hover (the details area explains what is under the pointer), and
            // nothing on the rack behind it lights up
            if (glassPanel->hover (e.position))
                publishPanel();
            shared.hoveredControl = -1;
            shared.hoveredUnit = -1;
            setMouseCursor (glassPanel->hitTest (e.position).entry >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            return;
        }
        if (glassPanel->hover (e.position))
            publishPanel();

        const int hovered = pickControl (e.position);
        shared.hoveredControl = hovered;
        if (! shared.renderInteraction.load())
            shared.hoveredUnit = hovered >= 0 ? -1 : unitUnderPointer (e.position);

        if (hovered < 0)
            setMouseCursor (juce::MouseCursor::NormalCursor);
        else if (isSwitchLike (controls[(size_t) hovered].kind))
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void HardwareView::mouseExit (const juce::MouseEvent&)
    {
        glassPanel->unhover();
        shared.mouseInside = false;
        shared.hoveredControl = -1;
    }

    void HardwareView::showWelcome (bool show)
    {
        shared.welcomeOpen = show;
        if (show)
            publishWelcome();
        else if (config.welcomeSeen != welcome.version)
        {
            config.welcomeSeen = welcome.version;
            UIConfig::saveKey ("welcomeSeen", welcome.version);
        }
    }

    void HardwareView::publishWelcome()
    {
        const float scale = juce::jmax (1.0f, shared.platformScale.load()) * 1.5f;
        const auto img = welcome.renderFrames (scale);   // (its jitter frames, stacked: the shader flips between them)
        artwork::RawTexture tex { img.getWidth(), img.getHeight(), 4, {} };
        tex.pixels.resize ((size_t) (tex.width * tex.height * 4));
        const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < tex.height; ++y)
            for (int x = 0; x < tex.width; ++x)
            {
                const auto px = reinterpret_cast<const juce::PixelARGB*> (data.getPixelPointer (x, y))->getUnpremultiplied();
                auto* d = tex.pixels.data() + ((size_t) y * (size_t) tex.width + (size_t) x) * 4;
                d[0] = px.getRed(); d[1] = px.getGreen(); d[2] = px.getBlue(); d[3] = px.getAlpha();
            }
        const juce::SpinLock::ScopedLockType lock (shared.welcomeLock);
        shared.welcomePending = std::move (tex);
        ++shared.welcomeVersion;
    }

    bool HardwareView::welcomeClick (juce::Point<float> p)
    {
        if (! shared.welcomeOpen.load())
            return false;
        const auto r = holo::Welcome::placeIn ((float) getWidth(), (float) getHeight());
        if (! r.contains (p)) { showWelcome (false); return true; }   // (outside the card: it goes)
        const float s = r.getWidth() / holo::Welcome::cardW;
        switch (welcome.hit ((p - r.getPosition()) / s))
        {
            case 1: showWelcome (false); break;
            case 2:
                welcome.showAtStart = ! welcome.showAtStart;
                config.showWelcome = welcome.showAtStart;
                UIConfig::saveKey ("showWelcome", welcome.showAtStart);
                publishWelcome();
                break;
            case 3:
                welcome.holoPanel = ! welcome.holoPanel;
                config.holoPanel = welcome.holoPanel;
                UIConfig::saveKey ("holoPanel", welcome.holoPanel);
                glassPanel->setHolo (welcome.holoPanel);
                shared.panelHolo = welcome.holoPanel;
                publishPanel (true);
                publishWelcome();
                break;
            default: break;
        }
        return true;
    }

    void HardwareView::mouseDown (const juce::MouseEvent& e)
    {
        if (welcomeClick (e.position))
            return;
        updateMouse (e.position);

        if (e.mods.isPopupMenu())
        {
            showViewMenu();
            return;
        }

        // 1. The glass panel first: a click on it is its own, whatever is behind it
        if (glassPanel->hitTest (e.position).inside)
        {
            glassPanel->click (e.position);
            publishPanel (true);
            return;
        }

        // 2. Turned round: THE PATCH BAY's plugs (a click takes one out, or puts the one in the hand in)
        if (patchMouseDown (e.position))
            return;

        // 3. Controls on the rack work as always, panel open or not
        const int hit = pickControl (e.position);

        if (hit < 0)
        {
            // 3. A unit's faceplate opens its panel (another unit switches to it, the same one closes it).
            //    Clicking off the rack closes the panel; with none open it steps back as before.
            //    Walking up to a unit is the wheel's job now.
            const int unit = unitUnderPointer (e.position);
            if (unit >= 0 && shared.focusTarget.load() >= 0.15f)
                shared.focusUnit = unit;   // close up: the camera glides to the unit clicked
            if (unit >= 0)
            {
                openPanel (glassPanel->getUnit() == unit ? -1 : unit);
                return;
            }
            // Turned round, a unit's back: the camera walks up to it (the same one again: steps back)
            if (const int back = backUnitAt (e.position); back >= 0)
            {
                const bool same = shared.focusBack.load() == back && shared.focusTarget.load() > 0.5f;
                shared.focusBack = back;
                setFocus (-1, same ? 0.0f : 1.0f);
                return;
            }
            // Empty space (or the rack's back): a sideways drag turns the rack round; a plain click is
            // handled when the button comes up (it closes the panel, or steps back)
            turnDrag = true;
            turnDragMoved = false;
            turnDragX = e.position.x;
            turnDragFrom = shared.turnTarget.load();
            return;
        }

        const int p = paramIndexForControl (hit);
        const bool isToggle = isSwitchLike (controls[(size_t) hit].kind);

        if (shared.renderInteraction.load())
        {
            // The render thread already applied the change from the polled pointer;
            // mouse events (delivered late by some hosts) only frame the host gesture.
            pressEventMs = juce::Time::getMillisecondCounterHiRes();

            if (isToggle)
            {
                pendingToggle = hit;
            }
            else
            {
                gestureParam = p;
                bridge.beginGesture (p, ControlSource::user);
            }
            return;
        }

        if (isToggle)
        {
            bridge.beginGesture (p, ControlSource::user);
            bridge.setValueWithSource (p, switchTarget (controls[(size_t) hit], bridge.getNormalised (p)), ControlSource::user);
            bridge.endGesture (p);
            return;
        }

        dragControl = hit;
        dragParam = p;
        dragValue = bridge.getNormalised (p);
        lastDragPos = e.position;
        shared.activeControl = hit;
        shared.dragging = true;
        bridge.beginGesture (p, ControlSource::user);
    }

    void HardwareView::mouseDrag (const juce::MouseEvent& e)
    {
        updateMouse (e.position);
        lastPointer = e.position;
        if (turnDrag)
        {
            const float dx = e.position.x - turnDragX;
            if (! turnDragMoved && std::abs (dx) > 6.0f)
            {
                turnDragMoved = true;
                if (glassPanel->isOpen())
                    openPanel (-1);
                setFocus (-1, 0.0f);   // stepped back: the whole rack turns
            }
            if (turnDragMoved)   // (either way: it goes round the short way to where you drag it)
                shared.turnTarget = juce::jlimit (0.0f, 1.0f, turnDragFrom + std::abs (dx) / juce::jmax (200.0f, 0.55f * (float) getWidth()) * (turnDragFrom > 0.5f ? -1.0f : 1.0f));
            return;
        }
        if (dragParam < 0)
            return;

        const auto delta = e.position - lastDragPos;
        lastDragPos = e.position;

        const bool fine = e.mods.isShiftDown() || e.mods.isCtrlDown();
        const float pixelsForFullRange = fine ? 1400.0f : 260.0f;

        dragValue = juce::jlimit (0.0f, 1.0f, dragValue + (-delta.y + delta.x * 0.25f) / pixelsForFullRange);
        bridge.setValueWithSource (dragParam, dragValue, ControlSource::user);
    }

    void HardwareView::mouseUp (const juce::MouseEvent&)
    {
        if (turnDrag)
        {
            turnDrag = false;
            if (turnDragMoved)
                setTurned (shared.turnTarget.load() > 0.5f);   // settles round, one way or the other
            else if (glassPanel->isOpen())
                openPanel (-1);
            else
                setFocus (-1, 0.0f);
            return;
        }
        if (gestureParam >= 0)
        {
            bridge.endGesture (gestureParam);
            gestureParam = -1;
        }

        if (pendingToggle >= 0)
        {
            const int p = paramIndexForControl (pendingToggle);
            bridge.beginGesture (p, ControlSource::user);

            // A click shorter than one rendered frame can slip past the pointer poll: apply it here.
            if (shared.renderPressMs.load() < pressEventMs - 1500.0)
                bridge.setValueWithSource (p, switchTarget (controls[(size_t) pendingToggle], bridge.getNormalised (p)), ControlSource::user);

            bridge.endGesture (p);
            pendingToggle = -1;
        }

        if (dragParam >= 0)
            bridge.endGesture (dragParam);

        dragControl = dragParam = -1;

        if (! shared.renderInteraction.load())
        {
            shared.activeControl = -1;
            shared.dragging = false;
        }
    }

    void HardwareView::mouseDoubleClick (const juce::MouseEvent& e)
    {
        if (glassPanel->hitTest (e.position).inside)
            return;
        const int hit = pickControl (e.position);
        if (hit < 0 || isSwitchLike (controls[(size_t) hit].kind))
            return;

        const int p = paramIndexForControl (hit);
        bridge.beginGesture (p, ControlSource::user);
        bridge.setValueWithSource (p, bridge.getDefaultNormalised (p), ControlSource::user);
        bridge.endGesture (p);
    }

    void HardwareView::nudge (int controlIndex, float delta)
    {
        const int p = paramIndexForControl (controlIndex);
        bridge.beginGesture (p, ControlSource::user);
        bridge.setValueWithSource (p, bridge.getNormalised (p) + delta, ControlSource::user);
        bridge.endGesture (p);
    }

    void HardwareView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (glassPanel->hitTest (e.position).inside)
        {
            // Over the glass the wheel scrolls its list
            const float dy = (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
            glassPanel->scroll (dy * 160.0f);   // eases to it (tick)
            return;
        }
        const int hit = pickControl (e.position);
        const float step = (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);

        if (hit < 0)
        {
            // Not over a control: the wheel walks toward the unit under the pointer - chosen as the walk
            // starts, from the whole rack, and kept while you are close. (Picked again on every step, the
            // rack sliding under a still pointer handed the walk to the unit above, and the one above that.)
            // Click a unit to go to another one while close; or step back out first.
            const int unit = unitUnderPointer (e.position);
            if (unit >= 0 && step > 0.0f && shared.focusTarget.load() < 0.15f)
                shared.focusUnit = unit;
            if (shared.turnAmount.load() > 0.9f && step > 0.0f && shared.focusTarget.load() < 0.15f)
            {
                int col = 0, row = 0;
                shared.focusBack = bayJackAt (e.position, col, row) ? -1 : backUnitAt (e.position);   // (turned: a unit's back, or the bay)
            }
            setFocus (-1, shared.focusTarget.load() + step * 0.45f);
            return;
        }

        if (isSwitchLike (controls[(size_t) hit].kind))
            return;

        if (controls[(size_t) hit].kind == ControlKind::selector)
        {
            // One position per wheel step: POWER has three, CHARACTER's model selectors nine
            const float positions = controls[(size_t) hit].unit == characterUnit ? (float) characterModels : 3.0f;
            nudge (hit, (step > 0.0f ? 1.0f : -1.0f) / (positions - 1.0f));
        }
        else
            nudge (hit, step * (e.mods.isShiftDown() ? 0.01f : 0.05f));
    }

    //==============================================================================
    /** Dev-only (PAD_UI_TEST_DEMO): a plausible spectrum so screenshots show the analyser
        doing something without an audio device. */
    void HardwareView::fillDemoScope (float t)
    {
        using enh::dsp::ScopeCurve;

        for (int i = 0; i < ScopeCurve::numPoints; ++i)
        {
            const float u = (float) i / (float) (ScopeCurve::numPoints - 1);
            const float hz = ScopeCurve::hzForPoint (i);

            // Pink tilt, a couple of drifting resonances, and a little noise on top
            float db = -16.0f - 13.0f * u;
            db += 7.0f * std::exp (-std::pow ((std::log (hz / (180.0f + 60.0f * std::sin (t * 0.35f))) * 1.7f), 2.0f));
            db += 5.0f * std::exp (-std::pow ((std::log (hz / (2400.0f + 900.0f * std::sin (t * 0.21f))) * 2.1f), 2.0f));
            db += 3.0f * std::sin (u * 34.0f + t * 2.1f) * 0.5f;
            db -= 26.0f * std::pow (std::max (0.0f, u - 0.86f) / 0.14f, 2.0f);

            const float out = db + 2.2f * std::sin (u * 9.0f + t * 0.6f);
            scopeCurve.inputDb[(size_t) i].store (db);
            scopeCurve.outputDb[(size_t) i].store (out);
            scopeCurve.peakDb[(size_t) i].store (std::max (db, out) + 2.0f);
        }

        scopeCurve.active.store (true);
    }

    void HardwareView::refreshOverlay()
    {
        auto valueText = [this] (int index)
        {
            auto* param = bridge.getParameter (index);
            auto text = param->getCurrentValueAsText();
            if (param->getLabel().isNotEmpty())
                text << param->getLabel();
            return text;
        };

        // Values as the number under the knob's indicator
        auto dial = [this] (const char* paramId)
        {
            const int control = controlIndex (paramId);
            const int index = paramIndexForControl (control);
            auto* param = bridge.getParameter (index);
            const float value = param->convertFrom0to1 (bridge.getNormalised (index));
            // Percent parameters are printed 0-10 on the dial; CLARITY carries its own 0-30 / 0-10 scale
            return juce::String (param->getNormalisableRange().end > 50.0f ? value / 10.0f : value, 1);
        };

        const bool addMode = bridge.getNormalised (bridge.indexOf (pad::params::id::clarityMode)) > 0.5f;

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (meters.footstepConfidence.load (std::memory_order_relaxed) > 0.5f)
            lastStepSeenMs = now;

        int shown = dragControl >= 0 ? dragControl : shared.hoveredControl.load();

        artwork::DisplayText text;
        text.title = addMode ? "ADD + NORM" : "NORMALIZE";
        text.tag = now - lastStepSeenMs < 350.0 ? "STEP" : "";
        text.lineLeft = "CLR " + dial (pad::params::id::clarityNorm) + " ADP " + dial (pad::params::id::adaptSpeed)
                      + " SUB " + dial (pad::params::id::sub);

        // SPECTRAL LIMITER: what it is cutting, and where, while it is (whole dB, two significant
        // figures of Hz, so the header is re-rendered only when that changes)
        {
            const bool limiterIn = bridge.getNormalised (bridge.indexOf (pad::params::id::spectralActive)) > 0.5f;
            float depth = 0.0f, hz = 0.0f;
            const auto demo = demoLimiterSlots (juce::Time::getMillisecondCounterHiRes() * 0.001 - openedAtMs * 0.001);
            for (size_t s = 0; s < demo.size(); ++s)
            {
                const float d = demoScope ? demo[s].depthDb : meters.limitDepthDb[s].load (std::memory_order_relaxed);
                if (d > depth)
                {
                    depth = d;
                    hz = demoScope ? demo[s].hz : meters.limitHz[s].load (std::memory_order_relaxed);
                }
            }
            const float broadband = demoScope ? 0.0f : meters.limitBroadbandDb.load (std::memory_order_relaxed);
            if (limiterIn && depth > 0.5f)
            {
                const double p = std::pow (10.0, std::floor (std::log10 (std::max (1.0f, hz))) - 1.0);
                const int shownHz = (int) (std::round (hz / p) * p);
                text.limitLine = "LIMIT -" + juce::String (juce::roundToInt (depth)) + " dB @ "
                               + (shownHz >= 1000 ? juce::String (shownHz / 1000.0, 1) + "k" : juce::String (shownHz)) + " Hz";
                if (broadband > 0.5f)
                    text.limitLine << "  BB -" << juce::roundToInt (broadband);
            }
        }

        // The preset just loaded, for a few seconds (or while a PRESET button is hovered)
        {
            if (processor.getPresetLoadCount() != lastPresetLoads)
            {
                lastPresetLoads = processor.getPresetLoadCount();
                presetShownMs = now;
            }
            const bool hoveringPreset = shown >= 0 && ! hasLed (controls[(size_t) shown]);
            if (hoveringPreset || now - presetShownMs < 4000.0)
            {
                const int p = processor.getCurrentProgram();
                text.focusLine = "PRESET " + juce::String (p + 1) + "/" + juce::String (processor.getNumPrograms())
                               + "  " + processor.getProgramName (p);
                shown = -1;
            }
        }

        if (shown >= 0)
        {
            const int index = paramIndexForControl (shown);
            const auto& c = controls[(size_t) shown];
            text.focusLine = (c.group != nullptr ? juce::String (c.group) + " " : juce::String())
                           + juce::String (c.label) + "  " + valueText (index);

            if (c.altParamId != nullptr)
                text.focusLine << (addMode ? " / 10" : " / 30");
        }

        if (text == lastText)
            return;

        lastText = text;
        auto raw = artwork::renderDisplayOverlay (text);

        const juce::SpinLock::ScopedLockType lock (shared.overlayLock);
        shared.overlayPending = std::move (raw);
        ++shared.overlayVersion;
    }

    void HardwareView::applyTestParams()
    {
        // Dev-only: PAD_UI_TEST_PARAMS="clarity=0.8;modeFootstep=1" (normalised values),
        // written as host automation to exercise the UI without a host.
        const auto spec = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_PARAMS", {});

        for (auto& token : juce::StringArray::fromTokens (spec, ";", {}))
        {
            const auto key = token.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = token.fromFirstOccurrenceOf ("=", false, false).getFloatValue();

            if (const int index = bridge.indexOf (key); index >= 0)
                bridge.setValueWithSource (index, value, ControlSource::hostAutomation);
        }
    }

    bool HardwareView::updateRenderingState()
    {
        // Checked a few times a second; hidden = not showing, minimised peer, or the X window can't be seen
        if (--visibilityCountdown > 0)
            return renderingActive;
        visibilityCountdown = renderingActive ? 8 : 1;

        auto* peer = getPeer();
        const bool visible = isShowing() && peer != nullptr && ! peer->isMinimised()
                             && windowVisibility.isVisible ((unsigned long) shared.nativeWindow.load());

        // Visible but not in front (a game has the focus, the rack sits behind it): the meters still
        // move, at 10 frames a second from the timer instead of every vsync. Nobody is looking closely,
        // and the rack stops costing a third of a core while a game is running.
        static const bool forceBackground = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_BACKGROUND", {}) == "1";
        const bool background = visible && (forceBackground || ! juce::Process::isForegroundProcess());

        if (visible && background != backgroundPaced)
        {
            backgroundPaced = background;
            if (renderingActive)
                glContext.setContinuousRepainting (! backgroundPaced);
            if (! backgroundPaced)
                glContext.triggerRepaint();
            if (logPausing)
                std::fprintf (stderr, "[enh-stats] rendering %s\n", backgroundPaced ? "at 10 fps (in the background)" : "every frame (in front)");
        }

        if (visible != renderingActive)
        {
            renderingActive = visible;
            glContext.setContinuousRepainting (visible && ! backgroundPaced);

            if (visible)
            {
                glContext.triggerRepaint();
                startTimerHz (30);
            }
            else
            {
                shared.calloutVisible = false;
                startTimerHz (4);   // only watching for the window to come back
            }

            if (logPausing)
                std::fprintf (stderr, "[enh-stats] rendering %s\n", visible ? "resumed" : "paused (window minimised/hidden)");
        }

        return renderingActive;
    }

    /** The LEVEL & LOUDNESS waveform and readout, and the MIX BALANCER's spectrum and history. */
    void HardwareView::updateDisplayHistories (float dt)
    {
        if (demoScope)
        {
            // Screenshots: the balancer spectrum is the demo analyser's, a little rearranged
            for (int i = 0; i < enh::dsp::ScopeCurve::numPoints; ++i)
            {
                const float in = scopeCurve.inputDb[(size_t) i].load();
                balancerCurve.inputDb[(size_t) i].store (in);
                balancerCurve.outputDb[(size_t) i].store (in - 2.5f * std::exp (-std::pow ((float) i / 192.0f - 0.45f, 2.0f) * 60.0f));
            }
        }
        else
        {
            balancerAnalyser.update (processor.getBalancerInputScope(), processor.getBalancerOutputScope(), balancerCurve, dt);
            // SPEED 1 .. 10: about 20 s .. 1 s of audio across the screen
            if (const int speed = bridge.indexOf (pad::params::id::monitorSpeed); speed >= 0)
            {
                const auto* spec = pad::params::findSpec (pad::params::id::monitorSpeed);
                const float value = spec != nullptr ? spec->minValue + (spec->maxValue - spec->minValue) * bridge.getNormalised (speed) : 5.0f;
                waveReader.setSecondsAcross (20.0 * std::pow (0.05, (value - 1.0) / 9.0));
            }
            waveReader.setRms (displayChoice ("displayWaveform") == 1);   // WAVEFORM display setting
            waveReader.update (processor.getInputScope(), processor.getOutputScope(), displayHistory);
        }

        // One balancer history column per tick (~10 s across the display)
        {
            const int head = displayHistory.balHead.load (std::memory_order_relaxed);
            const auto k = (size_t) (head % DisplayHistory::balColumns);
            float cut = 0.0f;
            for (auto& g : meters.balanceGainDb)
                cut = std::min (cut, g.load (std::memory_order_relaxed));
            displayHistory.balInDb[k].store (balancerCurve.inputRmsDb.load (std::memory_order_relaxed));
            displayHistory.balOutDb[k].store (balancerCurve.outputRmsDb.load (std::memory_order_relaxed));
            displayHistory.balCutDb[k].store (cut);
            displayHistory.balHead.store (head + 1, std::memory_order_release);
        }

        // DUCK: which unit is taking the most away right now, where, and how much; and the loudness
        // keepers' lift. Checked every frame, the deepest held 1.5 s so a short duck can be read.
        const double now = juce::Time::getMillisecondCounterHiRes();
        {
            auto hzText = [] (float hz)
            {
                return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz < 10000.0f ? 1 : 0) + " kHz" : juce::String (juce::roundToInt (hz)) + " Hz";
            };
            float depth = 0.0f;
            juce::String what;
            auto consider = [&] (float db, const juce::String& text)
            {
                if (db > depth) { depth = db; what = text; }
            };
            auto load = [] (const std::atomic<float>& a) { return a.load (std::memory_order_relaxed); };

            consider (load (meters.tideGrDb), "COMPRESSOR  (WHOLE MIX)");
            for (size_t s = 0; s < meters.limitDepthDb.size(); ++s)
            {
                const int shape = meters.limitShape[s].load (std::memory_order_relaxed);
                const auto hz = hzText (load (meters.limitHz[s]));
                consider (load (meters.limitDepthDb[s]), "SPECTRAL LIMITER  " + (shape == 1 ? "BELOW " + hz : shape == 2 ? "ABOVE " + hz : "AT " + hz));
            }
            consider (load (meters.limitBroadbandDb), "SPECTRAL LIMITER  (WHOLE MIX)");
            for (int b = 0; b < enh::dsp::MixBalancer::numBands; ++b)
            {
                const float hz = enh::dsp::MixBalancer::centreHz[(size_t) b];
                consider (-load (meters.balanceGainDb[(size_t) b]),
                          "MULTIBAND BALANCER  " + (b == 0 ? "BELOW 100 Hz" : b == enh::dsp::MixBalancer::numBands - 1 ? "ABOVE 7.0 kHz" : "AT " + hzText (hz)));
            }
            for (int k = 0; k < enh::dsp::MixBalancer::numFine; ++k)
                consider (-load (meters.balanceFineGainDb[(size_t) k]), "MULTIBAND BALANCER  AT " + hzText (enh::dsp::MixBalancer::fineHz (k)));
            static const char* regionName[] { "BELOW 150 Hz", "AT 400 Hz", "AT 1.6 kHz", "ABOVE 4.0 kHz" };
            for (int r = 0; r < enh::dsp::FinalLimiter::numRegions; ++r)
                consider (load (meters.outputRegionCutDb[(size_t) r]), juce::String ("OUTPUT LIMITER  ") + regionName[r] + "  (OVER 0 dB)");
            consider (load (meters.outputLimitDb), "OUTPUT LIMITER  (WHOLE MIX, OVER 0 dB)");

            if (demoScope) { depth = 4.2f; what = "SPECTRAL LIMITER  AT 2.5 kHz"; }

            const int hold = displayChoice ("displayDuckHold");            // DUCK HOLD display setting
            if (depth >= duckHeldDb || now - duckHeldMs > (hold == 1 ? 500.0 : hold == 2 ? 4000.0 : 1500.0))
            {
                duckHeldDb = depth;
                duckHeldMs = now;
                const float keep = demoScope ? 1.6f : load (meters.limitMakeupDb) + load (meters.balanceMakeupDb);
                duckText = depth < 0.5f ? juce::String ("DUCK  NONE")
                                        : "DUCK  " + what + "   -" + juce::String (depth, 1) + " dB";
                if (keep >= 0.1f)
                    duckText << "      LOUDNESS KEPT  +" << juce::String (keep, 1) << " dB";
            }
        }

        // The readout under the waveform: integrated loudness and true peak, a few times a second
        if (now - lastReadoutMs < 250.0)
            return;
        lastReadoutMs = now;

        auto fmt = [] (float v, const char* unit, float floor)
        {
            return v <= floor ? juce::String ("--.-  ") + unit : juce::String (v, 1) + "  " + unit;
        };
        const float integrated = demoScope ? -16.4f : meters.integratedLufs.load (std::memory_order_relaxed);
        const float peak = demoScope ? -1.2f : meters.truePeakDb.load (std::memory_order_relaxed);
        const int toneChoice = displayChoice ("displayToneRange");
        const int toneRangeDb = toneChoice == 1 ? 6 : toneChoice == 2 ? 24 : 12;
        const auto text = juce::String (toneRangeDb) + "\x01" + "INTEGRATED  " + fmt (integrated, "LUFS", -69.9f) + "        TRUE PEAK  " + fmt (peak, "dBTP", -99.0f)
                          + "\n" + duckText;
        if (text == levelReadout)
            return;
        levelReadout = text;

        auto tex = artwork::renderWindowLabels (monitorUnit, 3072, nullptr, text.fromFirstOccurrenceOf ("\x01", false, false), toneRangeDb);
        const juce::SpinLock::ScopedLockType lock (shared.levelLabelsLock);
        shared.levelLabelsPending = std::move (tex);
        ++shared.levelLabelsVersion;
    }

    /** A display setting's choice (MethodRegistry.h, DISPLAY), 0 = the default. */
    int HardwareView::displayChoice (const char* paramId) const
    {
        const int p = bridge.indexOf (paramId);
        if (p < 0)
            return 0;
        const auto* spec = pad::params::findSpec (paramId);
        return juce::roundToInt (bridge.getNormalised (p) * (spec != nullptr ? spec->maxValue : 1.0f));
    }

    /** CUSTOM: the processor's design onto the slot - its knobs and switches where the design put them
        (the rest parked), its name, colour and print, and its textures for the renderer. */
    void HardwareView::applyCustomDesign()
    {
        seenCustomVersion = processor.getCustomVersion();
        const auto d = pad::custom::decode (processor.getCustomCode());
        auto look = std::make_shared<layout::custom::Look>();
        if (d.ok)
        {
            look->name = d.name.toStdString(); look->model = d.model.toStdString(); look->sub = d.sub.toStdString();
            std::copy (std::begin (d.plate), std::end (d.plate), look->plate);
            for (const auto& p : d.parts)
                look->print.push_back (layout::designed::Print { p.kind, p.x, p.z, p.w, p.h, p.size, look->keep (p.text.toStdString()), p.align, p.steps, p.nums, 0, p.sweep, look->keep (p.param.toStdString()) });
        }
        for (int i = 0; i < 20; ++i)
        {
            auto& c = layout::controls[(size_t) (layout::firstCustomControl + i)];
            const auto& sl = d.slots[(size_t) i];
            auto& label = layout::custom::labels[(size_t) i];
            const auto text = sl.used ? sl.label.substring (0, 23) : juce::String (i < 16 ? "KNOB " + juce::String (i + 1) : "SWITCH " + juce::String (i - 15));
            std::fill (label.begin(), label.end(), 0);
            text.copyToUTF8 (label.data(), label.size() - 1);
            c.label = label.data();
            c.x = sl.used ? sl.x : 0.0f;
            c.z = sl.used ? sl.z : layout::parkedZ;
            if (i < 16 && sl.used) c.size = sl.size;
        }
        layout::custom::set (look);
        if (renderer != nullptr)
            renderer->setCustomTextures (artwork::renderDesignedDecal (customUnit, config.panelTextureWidth, nullptr),
                                         artwork::renderDesignedScreens (customUnit, config.panelTextureWidth / 2));
    }

    /** Newer units installed that were in the locker when the view opened: their print and screens baked now. */
    void HardwareView::bakeInstalledUnits()
    {
        if (renderer == nullptr) return;
        for (int k = 0; k < gen::count && k < (int) genBaked.size(); ++k)
            if (! genBaked[(size_t) k] && ! layout::isStored (firstGenUnit + k))
            {
                genBaked[(size_t) k] = true;
                renderer->setGenTextures (k, artwork::renderDesignedDecal (firstGenUnit + k, config.panelTextureWidth, &textItems),
                                             artwork::renderDesignedScreens (firstGenUnit + k, config.panelTextureWidth / 2));
            }
    }

    void HardwareView::timerCallback()
    {
        tickPatch();
        glassPanel->tickTuner (1.0f / 30.0f);   // RACK TUNER: its glide and its faceplate's buttons, open or not
        bakeInstalledUnits();   // (a unit just installed from the locker: its print, if it has none yet)
        if (processor.getCustomVersion() != seenCustomVersion)
            applyCustomDesign();
        // The LUNCHBOX's locker (its page in THE GEAR LOCKER, a session loaded): modules into their slots
        if (const auto lbs = processor.getStoredModules(); lbs != seenStoredModules)
        {
            seenStoredModules = lbs;
            layout::placeLunchbox (lbs);
            if (renderer != nullptr)
                renderer->setLunchboxDecal (artwork::renderLunchboxDecal (config.panelTextureWidth / 2, nullptr));
        }
        // THE GEAR LOCKER: the processor's (a session loaded, the locker page) is what the rack shows
        if (const auto stored = processor.getStoredUnits(); stored != storedUnits.load())
        {
            storedUnits = stored;
            if (! isShown (shared.focusUnit.load()))
                shared.focusUnit = enhUnit;
        }
        // Spectrum: pull the newest window out of the audio thread's FIFOs and analyse it here
        {
            const double now = juce::Time::getMillisecondCounterHiRes();
            const float dt = lastScopeMs > 0.0 ? (float) juce::jlimit (0.005, 0.25, (now - lastScopeMs) * 0.001) : 0.033f;
            lastScopeMs = now;

            if (demoScope)
                fillDemoScope ((float) (now * 0.001));
            else
                scopeAnalyser.update (processor.getInputScope(), processor.getOutputScope(), scopeCurve, dt);

            updateDisplayHistories (dt);
        }

        if (! testParamsApplied && juce::Time::getMillisecondCounter() - openedAtMs > 1500)
        {
            testParamsApplied = true;
            applyTestParams();
        }

        // Dev-only: PAD_UI_TEST_MINIMISE="3,8" minimises the window after 3 s and restores it after 8 s
        const auto minimiseTest = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_MINIMISE", {});
        if (minimiseTest.containsChar (','))
            if (auto* peer = getPeer())
            {
                const double t = (juce::Time::getMillisecondCounter() - openedAtMs) / 1000.0;
                const bool wantMinimised = t >= minimiseTest.upToFirstOccurrenceOf (",", false, false).getDoubleValue()
                                        && t < minimiseTest.fromFirstOccurrenceOf (",", false, false).getDoubleValue();
                if (wantMinimised != peer->isMinimised())
                    peer->setMinimised (wantMinimised);
            }

        // Loudness RESET: momentary - clears integrated loudness and the true-peak hold, springs back
        if (const int reset = bridge.indexOf (pad::params::id::loudnessReset); reset >= 0 && bridge.getNormalised (reset) > 0.5f)
        {
            processor.resetLoudness();
            bridge.setValueWithSource (reset, 0.0f, ControlSource::user);
        }

        // PRESET PREV / NEXT: momentary - a press loads the preset here (message thread) and springs back
        for (auto [pid, delta] : { std::pair { pad::params::id::presetPrev, -1 }, std::pair { pad::params::id::presetNext, 1 } })
        {
            const int index = bridge.indexOf (pid);
            if (index >= 0 && bridge.getNormalised (index) > 0.5f)
            {
                processor.stepPreset (delta);
                bridge.setValueWithSource (index, 0.0f, ControlSource::user);
            }
        }

        publishWindowGeometry();
        if (! updateRenderingState())
            return;   // paused: no GL frames, no overlay or callout work

        if (backgroundPaced && ++backgroundTick % 3 == 0)
            glContext.triggerRepaint();   // 30 Hz timer / 3

        refreshOverlay();
        updateCallout();
        if (glassPanel->isOpen())
        {
            glassPanel->tick (1.0f / 30.0f);   // folding, hover and scrolling ease; the print follows
            publishPanel();                     // redraws only when something shown changed or is moving
        }
    }

    void HardwareView::updateCallout()
    {
        // Pointer: mouse events, or PAD_UI_TEST_HOVER="x,y" (logical px) for screenshots
        // Prefer the polled pointer where we have it: it is sampled every frame, so the loupe
        // follows the cursor even while dragging or when the host delivers mouse moves late.
        const bool polled = shared.renderInteraction.load();
        float ndcX = polled ? shared.pointerNdcX.load() : shared.mouseNdcX.load();
        float ndcY = polled ? shared.pointerNdcY.load() : shared.mouseNdcY.load();
        bool inside = polled ? shared.pointerInside.load() : shared.mouseInside.load();

        const auto testHover = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_HOVER", {});
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        if (testHover.containsChar (','))
        {
            ndcX = 2.0f * testHover.upToFirstOccurrenceOf (",", false, false).getFloatValue() / w - 1.0f;
            ndcY = 1.0f - 2.0f * testHover.fromFirstOccurrenceOf (",", false, false).getFloatValue() / h;
            inside = true;
        }

        auto hide = [this]
        {
            shared.calloutVisible = false;
            lastCalloutKey.clear();
        };

        if (! inside)
            return hide();
        // No loupe over the glass panel (it would magnify the rack behind it)
        if (shared.pointInPanel ((ndcX + 1.0f) * 0.5f * w, (1.0f - ndcY) * 0.5f * h))
            return hide();

        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load(),
                                           shared.focus());
        const bool addMode = bridge.getNormalised (bridge.indexOf (pad::params::id::clarityMode)) > 0.5f;

        // While a control is being adjusted the loupe stays locked on it: moving the mouse to change
        // the value must not pull the focus onto whatever print passes under the pointer. (In most
        // hosts the drag runs on the render thread, hence shared.dragging rather than dragControl.)
        const int adjusting = shared.dragging.load() ? shared.activeControl.load()
                                                     : (dragControl >= 0 ? dragControl : -1);

        // Smallest printed word under the pointer, on whichever panel the pointer is over.
        // The lens is never hit-tested, so the pointer looks straight through it: whatever is under
        // the cursor wins, even where the glass is drawn over it.
        const artwork::TextItem* best = nullptr;
        float bestArea = 1.0e9f;

        for (int unit = 0; unit < numUnits; ++unit)
        {
            if (adjusting >= 0)
                break;

            float lx = 0.0f, lz = 0.0f;
            if (! cam.intersectUnit (unit, ndcX, ndcY, 0.0f, lx, lz)
                || std::abs (lx) > unitHalfW (unit) || std::abs (lz) > unitHalfH (unit))
                continue;

            for (auto& item : textItems)
            {
                if (item.unit != unit || (item.clarityScale >= 0 && item.clarityScale != (addMode ? 1 : 0)))
                    continue;

                constexpr float pad = 0.018f;
                if (std::abs (lx - item.x) <= item.halfW + pad && std::abs (lz - item.z) <= item.halfH + pad
                    && item.halfW * item.halfH < bestArea)
                {
                    best = &item;
                    bestArea = item.halfW * item.halfH;
                }
            }
        }

        auto describe = [this] (int control)
        {
            const auto& c = controls[(size_t) control];
            auto* param = bridge.getParameter (paramIndexForControl (control));
            auto value = param != nullptr ? param->getCurrentValueAsText() + param->getLabel() : juce::String();
            if (c.altParamId != nullptr)
                value << (bridge.getNormalised (bridge.indexOf (c.modeParamId)) > 0.5f ? " / 10  (ADD + NORM)" : " / 30  (NORM)");
            return std::make_pair ((c.group != nullptr ? juce::String (c.group) + "  " : juce::String()) + c.label, value);
        };

        juce::String title, detail;
        int unit = enhUnit;
        float ax = 0.0f, az = 0.0f;

        if (best != nullptr)
        {
            title = best->text;
            if (best->control >= 0)
                std::tie (title, detail) = describe (best->control);
            unit = best->unit;
            ax = best->x;
            az = best->z;
        }
        else
        {
            const int control = adjusting >= 0 ? adjusting : shared.hoveredControl.load();
            if (control < 0)
                return hide();

            // A control without text under the pointer: the loupe looks at the control itself
            std::tie (title, detail) = describe (control);
            const auto& c = controls[(size_t) control];
            unit = c.unit;
            ax = c.x;
            az = c.z;
        }

        const auto key = title + "|" + detail + "|" + juce::String (unit) + "|" + juce::String (ax) + "|" + juce::String (az);
        if (key == lastCalloutKey)
            return;

        lastCalloutKey = key;
        constexpr float pixelScale = 2.0f;
        // The loupe itself shows the print; controls also get a small name + value pill under it
        const bool pill = detail.isNotEmpty();
        if (pill)
        {
            auto raw = artwork::renderCallout (title, detail, pixelScale);
            const juce::SpinLock::ScopedLockType lock (shared.calloutLock);
            shared.calloutPending = std::move (raw);
            ++shared.calloutVersion;
        }
        shared.calloutHasPill = pill;

        shared.calloutUnit = unit;
        shared.calloutX = ax;
        shared.calloutZ = az;
        shared.calloutPixelScale = pixelScale;
        shared.calloutAtPointer = ! testHover.containsChar (',');
        shared.calloutVisible = true;
    }

    bool HardwareView::keyPressed (const juce::KeyPress& k)
    {
        if (glassPanel != nullptr && glassPanel->isOpen() && glassPanel->keyPressed (k))
        {
            publishPanel (true);
            return true;
        }
        return false;
    }
}
