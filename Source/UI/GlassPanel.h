#pragma once

#include <juce_graphics/juce_graphics.h>
#include "../DSP/MethodRegistry.h"
#include "Scene/DeviceLayout.h"
#include "Scene/PanelArtwork.h"

class PluginProcessor;

namespace pad
{
    class ParameterBridge;

    /** The glass panel that opens when a rack unit is clicked: the unit's name and every setting it has,
        grouped in categories, one under another in a column that scrolls - nothing side by side.

          PROCESSING  the unit's stages (DSP/MethodRegistry.h): how it measures, calculates, smooths...
          KNOBS       per knob: its own law (where it has one), then its SMOOTHING, CURVE and RANGE
          OUTPUT      output settings (the output limiter's ceiling)
          DISPLAY     display settings
          RESET       puts every setting of the unit back to its default

        Categories fold open and shut; a setting opens to list its choices. Everything eases (opening,
        folding, hover, scrolling), the settings that are not at their default are marked, and the bottom
        of the panel explains whatever is under the pointer.

        Split between the threads:
          - GlassPanel (message thread) owns what is open and hovered, animates it (tick), hit-tests
            clicks, applies choices, and draws the panel's print (white text and hairlines) into an RGBA
            image whenever any of that changes;
          - the renderer draws the glass itself (frosted: the scene behind it blurred, a hard-cornered edge
            catching the light, a soft shadow under it), the print on it, and the line from the unit.

        Everything is in logical pixels, origin top left, like the component. */
    namespace glass
    {
        inline constexpr float width = 244.0f;          // a column, not a sheet
        inline constexpr float maxHeight = 500.0f;
        inline constexpr float gutter = 16.0f;          // from the window's edge
        inline constexpr float headerH = 62.0f, categoryH = 30.0f, knobH = 24.0f, rowH = 46.0f, optionH = 27.0f,
                               resetH = 44.0f, detailsH = 128.0f, bodyH = 90.0f;

        /** One line of the list. */
        struct Entry
        {
            enum Kind { category, knobHeader, stage, modifier, reset } kind = stage;
            juce::String title;                                   // category / knob name
            int categoryIndex = 0;                                // the category it belongs to
            const enh::dsp::methods::Stage* stageInfo = nullptr;  // stage
            int knob = -1, modifierKind = -1;                     // modifier: knob (knobFields index) and kind

            // Animation (0..1) and where it is now, in list coordinates
            float open = 0.0f, openTarget = 0.0f;                 // category: folded out; setting: choices shown
            float hover = 0.0f;
            float y = 0.0f, h = 0.0f;

            int numChoices() const noexcept;
        };

        /** A hit on the panel: which entry, and which of its choices (-1 = the entry itself). */
        struct Hit { int entry = -1, option = -1; bool inside = false; };
    }

    class GlassPanel
    {
    public:
        GlassPanel (ParameterBridge&, PluginProcessor&);

        /** Opens the panel for a unit (-1 closes it). anchorY: where the unit is on screen. */
        void open (int unit, float anchorY, juce::Rectangle<float> view);
        void close()                              { open (-1, 0.0f, view); }
        int getUnit() const noexcept              { return unit; }
        bool isOpen() const noexcept              { return unit >= 0; }

        void setViewSize (juce::Rectangle<float> v);
        juce::Rectangle<float> getBounds() const noexcept { return panel; }

        glass::Hit hitTest (juce::Point<float>) const;
        bool hover (juce::Point<float>);          // true when what is hovered changed
        void unhover();
        void click (juce::Point<float>);
        bool scroll (float deltaPx);              // the wheel over the panel

        /** Advances the animations; true while anything is still moving (the print needs redrawing). */
        bool tick (float dt);

        /** Test hook: open the Nth setting's choices (and hover one), unfolding every category. */
        void setExpanded (int setting, int hoveredOption = -1, bool unfoldAll = true);

        bool needsRedraw() const noexcept         { return dirty; }
        artwork::RawTexture render (float pixelScale);

        /** Choices can change from elsewhere (host, preset): redraw when any shown value moved. */
        void pollValues();

    private:
        int currentChoice (const glass::Entry&) const;
        void choose (const glass::Entry&, int choice);
        void resetUnit();
        void layoutEntries();
        float contentHeight() const noexcept;
        juce::Rectangle<float> listArea() const noexcept;
        juce::Rectangle<float> entryBox (const glass::Entry&) const noexcept;       // on screen
        juce::Rectangle<float> optionBox (const glass::Entry&, int k) const noexcept;
        juce::String valueText (const glass::Entry&, int choice) const;
        juce::String choiceText (const glass::Entry&, int choice) const;

        ParameterBridge& bridge;
        PluginProcessor& processor;
        std::vector<glass::Entry> entries;
        juce::StringArray categories;
        juce::Rectangle<float> view, panel;
        int unit = -1;
        glass::Hit hovered;
        float anchorY = 0.0f, scrollY = 0.0f, scrollTarget = 0.0f;
        std::vector<int> shownChoices;
        bool dirty = true;
    };
}
