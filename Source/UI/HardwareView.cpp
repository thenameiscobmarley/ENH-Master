#include "HardwareView.h"
#include "Scene/HardwareRenderer.h"
#include "Scene/CameraRig.h"
#include "Scene/DeviceLayout.h"
#include "../PluginProcessor.h"

namespace pad
{
    using namespace layout;

    HardwareView::HardwareView (PluginProcessor& p)
        : bridge (p.getBridge()),
          config (UIConfig::loadOrCreate())
    {
        setOpaque (true);

        renderer = std::make_unique<HardwareRenderer> (bridge, shared, config,
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
        const float nx = 2.0f * pos.x / w - 1.0f, ny = 1.0f - 2.0f * pos.y / h;

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            float lx = 0, lz = 0;

            if (c.kind == ControlKind::toggle)
            {
                if (cam.intersectPanel (nx, ny, 0.12f, lx, lz)
                    && Rect { c.x, c.z, switchPlateHalfW + 0.08f, switchPlateHalfD + 0.10f }.contains (lx, lz))
                    return i;
            }
            else if (cam.intersectPanel (nx, ny, capTop * 0.6f, lx, lz)
                     && std::hypot (lx - c.x, lz - c.z) < bezelRadius * 1.08f)
            {
                return i;
            }
        }

        return -1;
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

        if (controls[(size_t) hit].kind == ControlKind::toggle)
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
        if (dragParam >= 0)
            bridge.endGesture (dragParam);

        dragControl = dragParam = -1;
        shared.activeControl = -1;
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
    static juce::String sourceTag (ControlSource s)
    {
        switch (s)
        {
            case ControlSource::user:           return "USER";
            case ControlSource::hostAutomation: return "AUTO";
            case ControlSource::selfTune:       return "TUNE";
            case ControlSource::none:           break;
        }
        return "IDLE";
    }

    void HardwareView::refreshOverlay()
    {
        const int clarity = paramIndexForControl (0);
        const int speed = paramIndexForControl (1);

        auto valueText = [this] (int index)
        {
            auto* param = bridge.getParameter (index);
            auto s = param->getCurrentValueAsText();
            if (param->getLabel().isNotEmpty())
                s << param->getLabel();
            return s;
        };

        const int shown = dragControl >= 0 ? dragControl : shared.hoveredControl.load();

        artwork::DisplayText text;
        text.title = "RESPONSE";
        text.tag = sourceTag (shown >= 0 ? bridge.getLastSource (paramIndexForControl (shown)) : bridge.getLastSource (clarity));
        text.lineLeft = "CLR " + valueText (clarity);
        text.lineRight = "SPD " + valueText (speed);

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

        refreshOverlay();
    }
}
