#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <memory>
#include <vector>
#include "../Scene/DeviceLayout.h"
#include "../Scene/PanelArtwork.h"
#include "../Scene/ColourScreens.h"
#include "../GlassPanel.h"
#include "../Holo/HoloWelcome.h"
#include "../../Config/UIConfig.h"
#include "../../DSP/PatchBay.h"

class PluginProcessor;

namespace pad
{
    class ParameterBridge;

    /** ENH Master 2D: the rack without the 3D - every installed unit's faceplate, one under another, drawn
        flat (JUCE's own 2D drawing; no OpenGL, no GPU work to speak of). The same print as the 3D rack's
        (PanelArtwork), the same controls in the same places (DeviceLayout), the same settings panel and gear
        locker (GlassPanel, drawn as an image).

          - the wheel, or a drag on a faceplate, slides the rack up and down;
          - drag a knob up or down to turn it (Shift: finely; double-click: its default); click a selector to
            step it, a switch to flip it;
          - right-click a unit for its settings, or the gear locker.

        Each faceplate is drawn once into an image for the window's width (again when the width or the rack
        changes); a frame only copies those and draws the controls and the meters' needles over them, and
        only when something moved. */
    class FlatRackView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        explicit FlatRackView (PluginProcessor&);
        ~FlatRackView() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
        bool keyPressed (const juce::KeyPress&) override;

    private:
        struct Row
        {
            int unit = -1;
            float top = 0.0f, height = 0.0f, scale = 1.0f, left = 0.0f;   // in content pixels; scale: pixels a panel unit
            juce::Image face;
        };
        void timerCallback() override;
        void rebuildRows();
        juce::Image renderFace (int unit, float scale) const;
        const Row* rowAt (float contentY) const;
        int controlAt (juce::Point<float> p) const;                        // index into layout::controls, -1 none
        juce::Point<float> controlCentre (const Row&, const layout::ControlDef&) const;
        int paramFor (const layout::ControlDef&) const;                     // the parameter it turns now (-1)
        void drawControl (juce::Graphics&, const Row&, const layout::ControlDef&) const;
        void drawNeedles (juce::Graphics&, const Row&) const;
        juce::Rectangle<float> roomScreenRect (const Row&) const;
        void drawRoomScreen (juce::Graphics&, const Row&) const;
        void drawColourScreen (juce::Graphics&, const Row&, juce::Rectangle<float> box, pad::colourscreen::Kind) const;
        /** CHROMA SPACE's and HYPERCUBE's screens: each one's light buffer (it fades, not clears), redrawn at 30 fps. */
        struct ColourSlot { pad::colourscreen::Canvas cv; pad::colourscreen::CubeState cube; double clock = -1.0; juce::Image img; };
        mutable std::map<int, ColourSlot> colourSlots;
        float needleReading (int needle) const;
        void showMenu (int unit);
        void openPanel (int unit);
        float maxScroll() const noexcept;

        PluginProcessor& processor;
        ParameterBridge& bridge;
        std::unique_ptr<GlassPanel> glassPanel;
        juce::Image panelImage;

        std::vector<Row> rows;
        float contentH = 0.0f, scrollY = 0.0f, scrollTarget = 0.0f;
        layout::UnitMask builtFor = ~layout::UnitMask { 0 };
        std::uint32_t builtModules = 0xffffffffu;
        int builtWidth = -1;

        // A drag: turning a control, or sliding the rack
        int dragControl = -1, dragParam = -1, hoverControl = -1;
        float dragStartValue = 0.0f, dragStartScroll = 0.0f;
        bool draggingRack = false;

        std::array<float, layout::numNeedles> needleNow {};
        std::vector<float> shownValues;
        const bool demo = juce::SystemStats::getEnvironmentVariable ("PAD_UI_TEST_DEMO", {}).isNotEmpty();
        juce::TooltipWindow tips { this, 400 };
        UIConfig config = UIConfig::loadOrCreate();
        holo::Welcome welcome;
        bool welcomeOpen = false;
        juce::Image welcomeImage;                         // (the current jitter frame)
        std::array<juce::Image, holo::Welcome::frames> welcomeFrames;
        void renderWelcome() { for (int f = 0; f < holo::Welcome::frames; ++f) welcomeFrames[(size_t) f] = welcome.render (1.5f, f * 101); welcomeImage = welcomeFrames[0]; }
        std::vector<juce::Point<float>> welcomePts;
        double demoTime = 0.0;

        // THE PATCH BAY (right-click: Patch bay): the rack's back as the 2D rack shows it - the bay flat, its cords
        // hanging below it; click a plug to take it out, click a jack to put it in (the same cords as the 3D rack's)
        bool patchOpen = false;
        int heldCord = -1, heldEnd = 0;
        struct PlugMove { int cord = -1, end = 0, col = -1, row = 0; bool inserting = false; double start = 0.0; float seconds = 0.3f; };
        PlugMove plugMove;
        juce::Point<float> mousePos;
        juce::Image bayImage;
        std::vector<int> bayImageChain;
        int bayImageWidth = -1;
        juce::Random plugRandom;
        juce::Rectangle<float> bayRect() const;
        juce::Point<float> jackPos (int col, int row) const;
        bool jackAtPoint (juce::Point<float>, int& col, int& row) const;
        juce::Rectangle<float> masterRect() const;
        juce::Rectangle<float> patchCloseRect() const;
        enh::patch::State currentPatch() const;
        void paintPatch (juce::Graphics&);
        void patchMouseDown (const juce::MouseEvent&);
        void tickPatch();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlatRackView)
    };
}
