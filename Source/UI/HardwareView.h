#pragma once

#include <juce_opengl/juce_opengl.h>
#include "SharedUIState.h"
#include "../DSP/SpectrumScope.h"
#include "DisplayHistory.h"
#include "../Config/UIConfig.h"
#include "../DSP/EngineMeters.h"
#include "HardwareKit.h"
#include "GlassPanel.h"

class PluginProcessor;

namespace pad
{
    class HardwareRenderer;
    class ParameterBridge;

    /** Hosts the OpenGL context and handles mouse interaction/picking on the
        message thread. Frame pacing lives in the renderer (render thread). */
    class HardwareView final : public juce::Component,
                               private juce::Timer
    {
    public:
        explicit HardwareView (PluginProcessor&);
        ~HardwareView() override;

        void resized() override;
        void parentHierarchyChanged() override { publishWindowGeometry(); }
        void paint (juce::Graphics&) override {}

        void mouseMove (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        /** Which unit's faceplate is under a point, or -1 for the rack case / the room. */
        int unitUnderPointer (juce::Point<float>) const;
        void setFocus (int unit, float amount);

    private:
        void timerCallback() override;

        // Spectrum analyser: the FFT runs here, on the editor thread, and the curve it
        // publishes is uploaded by the renderer once a frame.
        enh::dsp::ScopeAnalyser scopeAnalyser;
        enh::dsp::ScopeCurve scopeCurve;

        // MIX BALANCER display (its own input / output spectrum) and the scrolling histories of the
        // LEVEL & LOUDNESS waveform and the balancer's levels
        enh::dsp::ScopeAnalyser balancerAnalyser;
        enh::dsp::ScopeCurve balancerCurve;
        DisplayHistory displayHistory;
        WaveformReader waveReader;
        juce::String levelReadout;
        double lastReadoutMs = 0.0;
        // The DUCK readout: the deepest duck anywhere in the rack, held so it can be read
        juce::String duckText;
        float duckHeldDb = 0.0f;
        double duckHeldMs = 0.0;
        void updateDisplayHistories (float dt);
        double lastScopeMs = 0.0;
        const bool demoScope = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_DEMO", {}).isNotEmpty();
        void fillDemoScope (float seconds);
        void updateMouse (juce::Point<float>);
        int  pickControl (juce::Point<float>) const;
        int  paramIndexForControl (int controlIndex) const;
        void nudge (int controlIndex, float delta);
        void refreshOverlay();
        void applyTestParams();
        void publishWindowGeometry();
        void updateCallout();
        int displayChoice (const char* paramId) const;

        // The glass panel (GlassPanel.h): opened by clicking a unit, closed by clicking off the rack
        std::unique_ptr<GlassPanel> glassPanel;
        void openPanel (int unit);
        void publishPanel (bool force = false);
        bool updateRenderingState();

        PluginProcessor& processor;
        ParameterBridge& bridge;
        const enh::dsp::EngineMeters& meters;
        UIConfig config;
        SharedUIState shared;

        std::unique_ptr<HardwareRenderer> renderer;
        juce::OpenGLContext glContext;

        int dragControl = -1, dragParam = -1;
        int gestureParam = -1, pendingToggle = -1;
        double pressEventMs = 0.0;
        float dragValue = 0.0f;
        juce::Point<float> lastDragPos;
        juce::uint32 openedAtMs = 0;
        double lastStepSeenMs = -10000.0, presetShownMs = -10000.0;
        juce::uint32 lastPresetLoads = 0;
        bool testParamsApplied = false;
        artwork::DisplayText lastText;

        artwork::TextRegistry textItems;   // every printed word on both panels, for the hover callouts
        juce::String lastCalloutKey;

        // Rendering is paused while the window is minimised or hidden
        WindowVisibility windowVisibility;
        bool renderingActive = true;
        int visibilityCountdown = 0;
        bool backgroundPaced = false;   // visible, but another app has the focus: 10 frames a second
        int backgroundTick = 0;
        const bool logPausing = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_STATS", {}).isNotEmpty();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardwareView)
    };
}
