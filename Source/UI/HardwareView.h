#pragma once

#include <juce_opengl/juce_opengl.h>
#include "SharedUIState.h"
#include "../Config/UIConfig.h"

class PluginProcessor;

namespace pad
{
    class HardwareRenderer;
    class ParameterBridge;

    /** Hosts the OpenGL context, handles mouse interaction/picking on the
        message thread, and paces rendering. */
    class HardwareView final : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit HardwareView (PluginProcessor&);
        ~HardwareView() override;

        void resized() override;
        void paint (juce::Graphics&) override {}

        void mouseMove (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    private:
        void timerCallback() override;
        void updateMouse (juce::Point<float>);
        int  pickControl (juce::Point<float>) const;
        int  paramIndexForControl (int controlIndex) const;
        void setFocus (int pdTarget);
        void nudge (int controlIndex, float delta);
        void refreshOverlay();
        void applyTestParams();

        PluginProcessor& processor;
        ParameterBridge& bridge;
        UIConfig config;
        SharedUIState shared;

        std::unique_ptr<HardwareRenderer> renderer;
        juce::OpenGLContext glContext;

        int dragControl = -1, dragParam = -1;
        float dragValue = 0.0f;
        juce::Point<float> lastDragPos;
        int currentTimerHz = 0;
        int overlayTick = 0;
        juce::uint32 openedAtMs = 0;
        bool testParamsApplied = false;
        artwork::ScopeText lastText;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardwareView)
    };
}
