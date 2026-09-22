#pragma once

#include <juce_graphics/juce_graphics.h>
#include "../DSP/MethodRegistry.h"
#include "Scene/DeviceLayout.h"
#include "Scene/PanelArtwork.h"

class PluginProcessor;

namespace pad
{
    class ParameterBridge;

    /** The glass panel that opens when a rack unit is clicked: the unit's name and, for units whose
        processing is split into stages (DSP/MethodRegistry.h), a dropdown per stage and per knob
        modifier, with a details area that explains whatever method is under the pointer.

        Split between the threads:
          - GlassPanel (message thread) owns what is open, which dropdown is expanded and what is
            hovered, hit-tests clicks, applies choices, and draws the panel's print (white text and hairlines)
            into an RGBA image whenever any of that changes;
          - the renderer draws the glass itself (frosted: the scene behind it blurred, a hard-cornered
            edge catching the light, a soft shadow under it), the print on it, and the line from the
            unit, from the rectangle published in SharedUIState.

        The dropdowns are listed one under another in a column that scrolls; nothing sits side by side.

        Everything is in logical pixels, origin top left, like the component. */
    namespace glass
    {
        inline constexpr float width = 236.0f;          // narrow: a column, not a sheet
        inline constexpr float maxHeight = 470.0f;
        inline constexpr float gutter = 16.0f;         // from the window's edge
        inline constexpr float headerH = 62.0f, rowH = 48.0f, optionH = 28.0f, detailsH = 112.0f, bodyH = 90.0f;

        /** One dropdown: a stage of the unit, or a knob's modifier. */
        struct Row
        {
            const enh::dsp::methods::Stage* stage = nullptr;           // a stage ...
            const enh::dsp::methods::KnobModifiers* modifier = nullptr; // ... or a knob's input modifier
            int modifierIndex = -1;                                   // index in methods::knobModifiers
            int numChoices() const noexcept { return stage != nullptr ? stage->numMethods : (int) modifier->input->choices.size(); }
        };

        struct Layout
        {
            juce::Rectangle<float> panel;             // the glass
            juce::Rectangle<float> list;              // the scrolling part: the dropdowns, one under another
            std::vector<juce::Rectangle<float>> rows; // each dropdown's header (scrolled)
            std::vector<std::vector<juce::Rectangle<float>>> options;   // the expanded one's choices
            juce::Rectangle<float> details;
        };

        /** A hit on the panel: which dropdown, and which of its choices (-1 = the header). */
        struct Hit { int row = -1, option = -1; bool inside = false; };
    }

    class GlassPanel
    {
    public:
        GlassPanel (ParameterBridge&, PluginProcessor&);

        /** Opens the panel for a unit (-1 closes it). anchorY: where the unit is on screen, so the panel
            sits level with it. */
        void open (int unit, float anchorY, juce::Rectangle<float> view);
        void close()                              { open (-1, 0.0f, view); }
        int getUnit() const noexcept              { return unit; }
        bool isOpen() const noexcept              { return unit >= 0; }

        void setViewSize (juce::Rectangle<float> v);
        const glass::Layout& getLayout() const noexcept { return layout; }

        glass::Hit hitTest (juce::Point<float>) const;
        /** Pointer moved: returns true when what is hovered changed (the print needs redrawing). */
        bool hover (juce::Point<float>);
        /** A click on the panel (already hit-tested inside). */
        void click (juce::Point<float>);
        /** The wheel over the panel scrolls its list; returns true if it moved. */
        bool scroll (float deltaPx);
        /** Test hook: expand a dropdown and hover one of its choices. */
        void setExpanded (int row, int hoveredOption = -1);

        /** The print for the renderer, when something changed since the last call (else empty). */
        bool needsRedraw() const noexcept         { return dirty; }
        artwork::RawTexture render (float pixelScale);

        /** Choices can change from elsewhere (host, preset): redraw when any shown value moved. */
        void pollValues();

    private:
        int currentChoice (const glass::Row&) const;
        void choose (const glass::Row&, int choice);
        void relayout();

        ParameterBridge& bridge;
        PluginProcessor& processor;
        std::vector<glass::Row> rows;
        glass::Layout layout;
        juce::Rectangle<float> view;
        int unit = -1, expanded = -1;
        glass::Hit hovered;
        float anchorY = 0.0f, scrollY = 0.0f, contentH = 0.0f;
        std::vector<int> shownChoices;
        bool dirty = true;
    };
}
