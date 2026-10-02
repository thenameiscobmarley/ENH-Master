#pragma once

#include <juce_graphics/juce_graphics.h>
#include <map>
#include <set>
#include "../DSP/MethodRegistry.h"
#include "Scene/DeviceLayout.h"
#include "Scene/PanelArtwork.h"
#include "RackTuner.h"

class PluginProcessor;

namespace pad
{
    class ParameterBridge;

    /** The glass panel that opens when a rack unit is clicked: dark smoked glass with the unit's name, a row
        of tabs, and one tab's settings at a time, each a plain row - "Detection ........ Standard >".

          Sound      the unit's stages (DSP/MethodRegistry.h): how it measures, calculates, smooths...
          Stereo     stereo stages (where a unit has them)
          Output     output settings (the output limiter's ceiling)
          Display    display settings
          Knobs      per knob (a small heading each): its own law (where it has one), SMOOTHING, CURVE, RANGE
          Design     CUSTOM only: paste a design code, empty the slot
          Tune       RACK TUNER only: type what you want (or click words), TUNE, UNDO, A/B (RackTuner.h)

        Clicking a row opens its choices under it (one list open at a time); a setting not at its default
        has an amber dot. The footer explains whatever is under the pointer, and holds RESET TO DEFAULTS.
        THE GEAR LOCKER is the same panel with two tabs: In the rack, Locker.

        Split between the threads:
          - GlassPanel (message thread) owns what is open and hovered, animates it (tick), hit-tests
            clicks, applies choices, and draws the panel's print into an RGBA image whenever any of that
            changes;
          - the renderer draws the glass itself (smoked, a faint hint of the rack behind it, a thin edge of
            light, a soft shadow under it), the print on it, and the line from the unit.

        Everything is in logical pixels, origin top left, like the component. */
    namespace glass
    {
        inline constexpr float width = 300.0f;
        inline constexpr float maxHeight = 540.0f;
        inline constexpr float gutter = 16.0f;          // from the window's edge
        /** The panel's page that is not a unit's: THE GEAR LOCKER (units in and out of the rack). */
        inline constexpr int lockerPage = 1000;
        inline constexpr float headerH = 58.0f, tabsH = 36.0f, groupH = 32.0f, rowH = 38.0f, optionH = 28.0f,
                               lockerRowH = 46.0f, actionH = 50.0f, helpH = 74.0f, resetH = 50.0f, pad = 18.0f;

        /** One line of the list. */
        struct Entry
        {
            enum Kind { category, knobHeader, stage, modifier, reset, lockerUnit, designAction, lockerGroup, lockerSub, tunerAction, tunerChips } kind = stage;   // (tunerAction: RACK TUNER's TUNE / UNDO / A-B, rackUnit = which; tunerChips: its words to click)   // (designAction: CUSTOM's paste / clear; rackUnit = which; lockerGroup / lockerSub: the locker's categories, which fold out, and their sections)
            int rackUnit = -1;                                    // lockerUnit: which unit
            int lbModule = -1;                                    // lockerUnit on the "500 series" tab: which LUNCHBOX module
            juce::String title;                                   // category / knob name
            int categoryIndex = 0;                                // the category it belongs to
            int knobGroup = -1;                                   // a knob's setting: its knob header's entry index; a locker unit or section: its group's
            const enh::dsp::methods::Stage* stageInfo = nullptr;  // stage
            int knob = -1, modifierKind = -1;                     // modifier: knob (knobFields index) and kind

            // Animation (0..1) and where it is now, in list coordinates
            float open = 0.0f, openTarget = 0.0f;                 // category: folded out; setting: choices shown
            float hover = 0.0f;
            float y = 0.0f, h = 0.0f;

            int numChoices() const noexcept;
        };

        /** A hit on the panel: which entry, and which of its choices (-1 = the entry itself). */
        struct Hit { int entry = -1, option = -1, tab = -1; bool inside = false, search = false;
                     bool operator== (const Hit& o) const noexcept { return entry == o.entry && option == o.option && tab == o.tab && inside == o.inside && search == o.search; } };
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
        /** Typing: in THE GEAR LOCKER, into its search; on RACK TUNER, its words (true: the key was the panel's). */
        bool keyPressed (const juce::KeyPress&);
        bool wantsKeys() const noexcept           { return unit == glass::lockerPage || unit == layout::tunerUnit; }
        /** RACK TUNER, 30 times a second whether the panel is open or not (its glide, its faceplate's buttons). */
        void tickTuner (float dt)                 { tuner.tick (dt); if (unit == layout::tunerUnit && tuner.lastReport() != lockerNote) { lockerNote = tuner.lastReport(); dirty = true; } }

        /** Advances the animations; true while anything is still moving (the print needs redrawing). */
        bool tick (float dt);

        /** Test hook: open the Nth setting's choices (and hover one), unfolding every category. */
        void setExpanded (int setting, int hoveredOption = -1, bool unfoldAll = true);

        bool needsRedraw() const noexcept         { return dirty; }
        void selectTab (int t)                    { showTab (t); dirty = true; }
        /** Dev-only (screenshots): hovers the nth unit shown in the open tab. */
        void testHoverUnit (int nth)
        {
            layoutEntries();
            for (int i = 0, k = 0; i < (int) entries.size(); ++i)
                if (entries[(size_t) i].kind == glass::Entry::lockerUnit && entries[(size_t) i].h > 1.0f && k++ == nth) { hovered = { i, -1, -1, true }; dirty = true; return; }
        }
        /** HOLOGRAM style: the print in the scope's phosphor green (the renderer draws the glass to match). */
        void setHolo (bool h)                     { holo = h; dirty = true; }
        artwork::RawTexture render (float pixelScale);

        /** Choices can change from elsewhere (host, preset): redraw when any shown value moved. */
        void pollValues();

    private:
        int currentChoice (const glass::Entry&) const;
        void choose (const glass::Entry&, int choice);
        void resetUnit();
        void buildLocker();                       // THE GEAR LOCKER's list: the rack, then the locker
        void toggleStored (int rackUnit);
        void toggleModule (int lbModule);         // the LUNCHBOX's own locker
        juce::String search;                      // THE GEAR LOCKER's search (typed while it is open)
        bool matchesSearch (const glass::Entry&) const;
        float searchHeight() const noexcept;
        juce::Rectangle<float> searchBox() const noexcept;
        juce::String lockerNote;                  // why a unit could not go in, while it stands (RACK TUNER: what the last tune did)
        RackTuner tuner;
        std::vector<juce::Rectangle<float>> chipBoxes (const glass::Entry&) const;   // RACK TUNER's words, on screen
        bool tunerShowsBefore() const;           // its A/B switch: the rack before the last tune
        std::set<juce::String> openGroups;        // the locker's categories folded out (kept while it is rebuilt)
        std::map<int, juce::Image> thumbs;        // each unit's faceplate, painted once (UnitFace.h), for its preview
        bool thumbsPending = false;
        bool groupHasMatch (int groupEntry, int subEntry = -1) const;
        float unitRowHeight (const glass::Entry&) const;
        const juce::Image* thumbFor (int rackUnit, float pixelWidth, int& budget);
        void layoutEntries();
        float contentHeight() const noexcept;
        juce::Rectangle<float> listArea() const noexcept;
        juce::Rectangle<float> tabBox (int t) const noexcept;
        juce::String tabLabel (int t) const;
        juce::Rectangle<float> resetBox() const noexcept;
        float footerHeight() const noexcept;
        int resetEntry() const noexcept;
        void showTab (int t);
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
        int tab = 0; float tabSlide = 0.0f;       // the open tab; the underline easing to it
        bool holo = false;
        std::vector<int> shownChoices;
        bool dirty = true;
    };
}
