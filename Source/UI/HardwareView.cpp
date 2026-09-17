#include "HardwareView.h"
#include "Scene/HardwareRenderer.h"
#include "Scene/CameraRig.h"
#include "Scene/DeviceLayout.h"
#include "../PluginProcessor.h"

namespace pad
{
    using namespace layout;

    HardwareView::HardwareView (PluginProcessor& p)
        : processor (p),
          bridge (p.getBridge()),
          config (UIConfig::loadOrCreate())
    {
        setOpaque (true);
        shared.pdFocus = processor.getPdFocus();

        renderer = std::make_unique<HardwareRenderer> (bridge, shared, config,
                                                       artwork::renderPanelDecal (config.panelTextureWidth),
                                                       artwork::renderKnobDial (512));

        juce::OpenGLPixelFormat format;
        format.depthBufferBits = 24;
        format.multisamplingLevel = config.msaaSamples;
        glContext.setPixelFormat (format);
        glContext.setMultisamplingEnabled (config.msaaSamples > 0);
        glContext.setPreferredVersion ({ 3, 2 });
        glContext.setComponentPaintingEnabled (false);
        glContext.setContinuousRepainting (false);
        glContext.setRenderer (renderer.get());
        glContext.attachTo (*this);

        openedAtMs = juce::Time::getMillisecondCounter();
        refreshOverlay();
        startTimerHz (config.frameRate);
        currentTimerHz = config.frameRate;
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
        glContext.triggerRepaint();
    }

    //==============================================================================
    int HardwareView::paramIndexForControl (int i) const
    {
        const auto& c = controls[(size_t) i];
        const int focus = shared.pdFocus.load();

        switch (c.binding)
        {
            case Binding::pdKp:  return bridge.indexOf (params::pdKpId (focus));
            case Binding::pdKd:  return bridge.indexOf (params::pdKdId (focus));
            case Binding::fixed: break;
        }
        return bridge.indexOf (c.paramId);
    }

    int HardwareView::pickControl (juce::Point<float> pos) const
    {
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        const auto cam = CameraRig::build (w / h, shared.parallaxX.load(), shared.parallaxY.load());
        const float nx = 2.0f * pos.x / w - 1.0f, ny = 1.0f - 2.0f * pos.y / h;

        int best = -1;
        float bestDist = 1.0e9f;

        for (int i = 0; i < numControls; ++i)
        {
            const auto& c = controls[(size_t) i];
            float hx = 0, hz = 0;

            if (c.kind == ControlKind::toggle)
            {
                if (cam.intersectPlaneY (nx, ny, panelTop + 0.08f, hx, hz))
                {
                    const Rect hit { c.x, c.z, switchWellHalfW + 0.07f, switchWellHalfD + 0.07f };
                    if (hit.contains (hx, hz))
                        return i;
                }
                continue;
            }

            if (! cam.intersectPlaneY (nx, ny, panelTop + capTop * c.scale * 0.75f, hx, hz))
                continue;

            const float d = std::hypot (hx - c.x, hz - c.z);
            if (d < flangeRadius * c.scale * 1.15f && d < bestDist)
            {
                best = i;
                bestDist = d;
            }
        }

        return best;
    }

    void HardwareView::updateMouse (juce::Point<float> pos)
    {
        const float w = (float) juce::jmax (1, getWidth()), h = (float) juce::jmax (1, getHeight());
        shared.mouseNdcX = juce::jlimit (-1.0f, 1.0f, 2.0f * pos.x / w - 1.0f);
        shared.mouseNdcY = juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * pos.y / h);
        shared.mouseInside = true;
    }

    void HardwareView::setFocus (int pdTarget)
    {
        if (pdTarget < 0 || pdTarget == shared.pdFocus.load())
            return;

        shared.pdFocus = pdTarget;
        processor.setPdFocus (pdTarget);
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

        const auto& c = controls[(size_t) hit];
        const int p = paramIndexForControl (hit);

        if (c.kind == ControlKind::toggle)
        {
            bridge.beginGesture (p, ControlSource::user);
            bridge.setValueWithSource (p, bridge.getNormalised (p) > 0.5f ? 0.0f : 1.0f, ControlSource::user);
            bridge.endGesture (p);
            return;
        }

        if (c.kind == ControlKind::knob)
            setFocus (c.pdTarget);

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
        return "--";
    }

    void HardwareView::refreshOverlay()
    {
        const int focus = shared.pdFocus.load();
        const int targetParam = bridge.indexOf (params::pdTargetIds()[(size_t) focus]);
        const int kp = bridge.indexOf (params::pdKpId (focus));
        const int kd = bridge.indexOf (params::pdKdId (focus));

        artwork::ScopeText text;
        text.title = "PD RESPONSE";
        text.target = controls[(size_t) focus].label;
        text.footer = "PREVIEW / NO DSP";
        text.lineLeft = "Kp " + bridge.getParameter (kp)->getCurrentValueAsText();
        text.lineMid = "Kd " + bridge.getParameter (kd)->getCurrentValueAsText();
        text.lineRight = sourceTag (bridge.getLastSource (targetParam));

        const int shown = dragControl >= 0 ? dragControl : shared.hoveredControl.load();

        if (shown >= 0)
        {
            const auto& c = controls[(size_t) shown];
            auto* param = bridge.getParameter (paramIndexForControl (shown));
            auto value = param->getCurrentValueAsText();
            const auto unit = param->getLabel();

            if (unit.isNotEmpty())
                value << " " << unit;

            text.focusLine = juce::String (c.label).paddedRight (' ', 11) + value;
        }

        if (text == lastText)
            return;

        lastText = text;
        auto raw = artwork::renderScopeOverlay (text);

        const juce::SpinLock::ScopedLockType lock (shared.overlayLock);
        shared.overlayPending = std::move (raw);
        ++shared.overlayVersion;
    }

    void HardwareView::applyTestParams()
    {
        // Dev-only: PAD_UI_TEST_PARAMS="focus=6;modeFootstep=1;adaptDepth=0.8"
        // Values are normalised and written as host automation, to exercise the UI without a host.
        const auto spec = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_PARAMS", {});

        for (auto& token : juce::StringArray::fromTokens (spec, ";", {}))
        {
            const auto key = token.upToFirstOccurrenceOf ("=", false, false).trim();
            const auto value = token.fromFirstOccurrenceOf ("=", false, false).getFloatValue();

            if (key == "focus")
                setFocus (juce::jlimit (0, params::numPdTargets - 1, (int) value));
            else if (const int index = bridge.indexOf (key); index >= 0)
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

        if (++overlayTick >= 3)
        {
            overlayTick = 0;
            refreshOverlay();
        }

        const bool busy = shared.mouseInside.load() || shared.animating.load() || dragParam >= 0;
        const int desired = busy ? config.frameRate : config.idleFrameRate;

        if (desired != currentTimerHz)
        {
            currentTimerHz = desired;
            startTimerHz (desired);
        }

        glContext.triggerRepaint();
    }
}
