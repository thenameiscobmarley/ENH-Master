#include "HardwareView.h"
#include "Scene/HardwareRenderer.h"
#include "Scene/CameraRig.h"
#include "Scene/DeviceLayout.h"
#include "Scene/Picking.h"
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
        if (dumpDir.isNotEmpty())
            audit::writeLayoutAudit (textures, textItems, juce::File (dumpDir));

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

        glassPanel = std::make_unique<GlassPanel> (bridge, processor);

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
                safe->openPanel (juce::jlimit (0, numUnits - 1, parts[0].getIntValue()));
                if (parts.size() > 1)
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
        m.addItem (2, "Full rack - all 11 units", true, ! simple);
        m.addSeparator();
        m.addItem (3, "Units put away keep working, as the preset set them", false, false);
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                         [safe = juce::Component::SafePointer<HardwareView> (this)] (int chosen)
                         {
                             if (safe != nullptr && (chosen == 1 || chosen == 2))
                                 safe->setSimpleView (chosen == 1);
                         });
    }

    void HardwareView::setSimpleView (bool simple)
    {
        hiddenUnits = simple ? simpleViewHidden : 0u;
        if (glassPanel->isOpen() && ! isShown (glassPanel->getUnit()))
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
        if (unit >= 0)
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

    void HardwareView::mouseDown (const juce::MouseEvent& e)
    {
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

        // 2. Controls on the rack work as always, panel open or not
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
                openPanel (glassPanel->getUnit() == unit ? -1 : unit);
            else if (glassPanel->isOpen())
                openPanel (-1);
            else
                setFocus (-1, 0.0f);
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
            bridge.setValueWithSource (p, bridge.getNormalised (p) > 0.5f ? 0.0f : 1.0f, ControlSource::user);
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
                bridge.setValueWithSource (p, bridge.getNormalised (p) > 0.5f ? 0.0f : 1.0f, ControlSource::user);

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
                          "MIX BALANCER  " + (b == 0 ? "BELOW 100 Hz" : b == enh::dsp::MixBalancer::numBands - 1 ? "ABOVE 7.0 kHz" : "AT " + hzText (hz)));
            }
            for (int k = 0; k < enh::dsp::MixBalancer::numFine; ++k)
                consider (-load (meters.balanceFineGainDb[(size_t) k]), "MIX BALANCER  AT " + hzText (enh::dsp::MixBalancer::fineHz (k)));
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

    void HardwareView::timerCallback()
    {
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
}
