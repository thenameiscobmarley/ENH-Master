#include "HardwareView.h"
#include "Scene/HardwareRenderer.h"
#include "Scene/CameraRig.h"
#include "Scene/DeviceLayout.h"
#include "Scene/Picking.h"
#include "../PluginProcessor.h"

namespace pad
{
    using namespace layout;

    HardwareView::HardwareView (PluginProcessor& p)
        : bridge (p.getBridge()),
          meters (p.getMeters()),
          config (UIConfig::loadOrCreate())
    {
        setOpaque (true);

        renderer = std::make_unique<HardwareRenderer> (bridge, shared, meters, config,
                                                       artwork::renderFaceplateDecal (config.panelTextureWidth),
                                                       artwork::renderKnobDial (512));

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

        openedAtMs = juce::Time::getMillisecondCounter();
        refreshOverlay();
        startTimerHz (20); // display text only
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
        return bridge.indexOf (controls[(size_t) i].paramId);
    }

    int HardwareView::pickControl (juce::Point<float> pos) const
    {
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load());
        return pad::pickControl (cam, 2.0f * pos.x / w - 1.0f, 1.0f - 2.0f * pos.y / h);
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
        const int hovered = pickControl (e.position);
        shared.hoveredControl = hovered;

        if (hovered < 0)
            setMouseCursor (juce::MouseCursor::NormalCursor);
        else if (controls[(size_t) hovered].kind == ControlKind::toggle)
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void HardwareView::mouseExit (const juce::MouseEvent&)
    {
        shared.mouseInside = false;
        shared.hoveredControl = -1;
    }

    void HardwareView::mouseDown (const juce::MouseEvent& e)
    {
        updateMouse (e.position);
        const int hit = pickControl (e.position);
        if (hit < 0)
            return;

        const int p = paramIndexForControl (hit);
        const bool isToggle = controls[(size_t) hit].kind == ControlKind::toggle;

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
        const int hit = pickControl (e.position);
        if (hit < 0 || controls[(size_t) hit].kind == ControlKind::toggle)
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
        const int hit = pickControl (e.position);
        if (hit < 0 || controls[(size_t) hit].kind == ControlKind::toggle)
            return;

        const float step = (std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
        nudge (hit, step * (e.mods.isShiftDown() ? 0.01f : 0.05f));
    }

    //==============================================================================
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

        auto percent = [this] (int control)
        {
            return juce::String (juce::roundToInt (bridge.getNormalised (paramIndexForControl (control)) * 100.0f));
        };

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (meters.footstepConfidence.load (std::memory_order_relaxed) > 0.5f)
            lastStepSeenMs = now;

        const int shown = dragControl >= 0 ? dragControl : shared.hoveredControl.load();

        artwork::DisplayText text;
        text.title = "ADAPTIVE EQ";
        text.tag = now - lastStepSeenMs < 350.0 ? "STEP" : "";
        text.lineLeft = "CLR " + percent (0) + "  ADP " + percent (1) + "  SUB " + percent (2);

        if (shown >= 0)
            text.focusLine = juce::String (controls[(size_t) shown].label) + "  " + valueText (paramIndexForControl (shown));

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

    void HardwareView::timerCallback()
    {
        if (! testParamsApplied && juce::Time::getMillisecondCounter() - openedAtMs > 1500)
        {
            testParamsApplied = true;
            applyTestParams();
        }

        publishWindowGeometry();
        refreshOverlay();
    }
}
