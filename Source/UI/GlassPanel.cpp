#include "GlassPanel.h"
#include "Scene/UnitDescriptions.h"
#include "Holo/VectorBeam.h"
#include "../PluginProcessor.h"
#include "../Parameters/ParameterBridge.h"
#include "../Parameters/KnobModifiers.h"
#include "Controls/ControlBinding.h"
#include "UnitFace.h"

namespace pad
{
    using namespace glass;
    namespace m = enh::dsp::methods;

    static juce::String str (std::string_view v) { return juce::String (v.data(), v.size()); }

    static juce::Font font (float heightPx, bool bold, float tracking = 0.0f)
    {
        auto opts = juce::FontOptions().withHeight (heightPx).withKerningFactor (tracking);
        return juce::Font (bold ? opts.withStyle ("Bold") : opts);
    }

    /** "SMOOTHING" -> "Smoothing", "OFF" -> "Off"; words with digits or already mixed case are left alone. */
    static juce::String plain (const juce::String& t)
    {
        if (t != t.toUpperCase())
            return t;
        juce::StringArray words;
        words.addTokens (t, " ", {});
        for (int i = 0; i < words.size(); ++i)
        {
            auto w = words[i];
            if (w.containsAnyOf ("0123456789") || w.length() <= 1 || (i > 0 && w.length() <= 3 && ! w.containsAnyOf ("AEIOUY")))
                continue;   // (numbers, and short acronyms such as M/S, HF, LF, dB stay)
            words.set (i, i == 0 ? w.substring (0, 1) + w.substring (1).toLowerCase() : w.toLowerCase());
        }
        return words.joinIntoString (" ");
    }

    /** What a method is called in the panel: its full name, never its code. */
    static juce::String methodLabel (const m::Method& me) { return plain (str (me.fullName)); }

    /** A tab's name, from the registry's category. */
    static juce::String tabName (const juce::String& category)
    {
        if (category == "PROCESSING") return "Sound";
        if (category == "KNOBS")      return "Knobs";
        if (category == "STEREO")     return "Stereo";
        if (category == "OUTPUT")     return "Output";
        if (category == "DISPLAY")    return "Display";
        if (category == "DESIGN")     return "Design";
        return plain (category);
    }

    static float ease (float current, float target, float rate, float dt) noexcept
    {
        const float next = target + (current - target) * std::exp (-rate * dt);
        return std::abs (next - target) < 0.002f ? target : next;
    }

    static const juce::Colour amber { 0xffe8b25a };   // (render() shadows it: HOLOGRAM has its own)
    static const juce::String sep = juce::String::fromUTF8 ("   \xc2\xb7   ");   // (a middle dot between two facts)   // the one accent: a setting that is not at its default, the open tab

    int Entry::numChoices() const noexcept
    {
        if (kind == stage)
            return stageInfo->numMethods;
        if (kind == modifier)
            return (int) m::modifiers[(size_t) modifierKind].choices.size();
        return 0;
    }

    GlassPanel::GlassPanel (ParameterBridge& b, PluginProcessor& p) : tuner (b, p), bridge (b), processor (p) {}

    //==============================================================================
    void GlassPanel::setViewSize (juce::Rectangle<float> v)
    {
        if (v == view)
            return;
        view = v;
        layoutEntries();
    }

    void GlassPanel::open (int newUnit, float y, juce::Rectangle<float> v)
    {
        view = v;
        anchorY = y;
        if (newUnit != unit)
        {
            unit = newUnit;
            hovered = {};
            scrollY = scrollTarget = 0.0f;
            tab = 0; tabSlide = 0.0f;
            entries.clear();
            categories.clear();
            lockerNote = {};
            if (unit == lockerPage)
                buildLocker();
            else if (unit >= 0)
            {
                const auto list = m::stagesForUnit (unit);
                auto categoryOf = [this] (const juce::String& name)
                {
                    if (! categories.contains (name))
                    {
                        categories.add (name);
                        Entry e;
                        e.kind = Entry::category;
                        e.title = tabName (name);
                        e.categoryIndex = categories.size() - 1;
                        entries.push_back (e);
                    }
                    return categories.indexOf (name);
                };

                if (unit == layout::customUnit)   // CUSTOM: its first tab loads a design from the clipboard, or empties the slot
                {
                    const int c = categoryOf ("DESIGN");
                    for (int a = 0; a < 2; ++a)
                    {
                        Entry act; act.kind = Entry::designAction; act.rackUnit = a; act.categoryIndex = c;
                        entries.push_back (act);
                    }
                }

                if (unit == layout::tunerUnit)   // RACK TUNER: its words, then TUNE, UNDO and A/B
                {
                    const int c = categoryOf ("TUNE");
                    for (int a : { 0, 2, 1 })   // (Tune, A/B, Undo first: always in view; the words under them)
                    {
                        Entry act; act.kind = Entry::tunerAction; act.rackUnit = a; act.categoryIndex = c;
                        entries.push_back (act);
                    }
                    Entry chips; chips.kind = Entry::tunerChips; chips.categoryIndex = c;
                    entries.push_back (chips);
                    lockerNote = tuner.lastReport();
                }

                // Unit-wide stages, by category (KNOBS' laws come with their knobs below)
                for (const juce::String cat : { "PROCESSING", "STEREO", "OUTPUT", "DISPLAY" })
                    for (int i = 0; i < list.count; ++i)
                        if (str (list.stages[i].category) == cat && list.stages[i].knobParam.empty())
                        {
                            Entry e;
                            e.kind = Entry::stage;
                            e.stageInfo = &list.stages[i];
                            e.categoryIndex = categoryOf (cat);
                            entries.push_back (e);
                        }

                // KNOBS: every continuous knob on this unit - a heading, its law (if it has one), then its modifiers
                std::vector<int> knobsHere;
                for (int c = 0; c < layout::numControls; ++c)
                {
                    const auto& ctl = layout::controls[(size_t) c];
                    if (ctl.unit != unit)
                        continue;
                    for (const char* pid : { ctl.paramId, ctl.altParamId })
                        if (pid != nullptr)
                            if (const int k = KnobModifiers::indexOf (pid); k >= 0 && std::find (knobsHere.begin(), knobsHere.end(), k) == knobsHere.end())
                                knobsHere.push_back (k);
                }
                juce::StringArray titles;
                for (int k : knobsHere)
                {
                    const juce::String pid (enh::dsp::knobFields[(size_t) k].param);
                    const auto* spec = pad::params::findSpec (pid);
                    Entry h;
                    h.kind = Entry::knobHeader;
                    h.title = spec != nullptr ? (spec->name.isNotEmpty() ? spec->name : spec->shortLabel) : pid;
                    if (titles.contains (h.title))   // two knobs with one name (a knob and its second function)
                        h.title << " " << juce::String (titles.size() + 1);
                    titles.add (h.title);
                    h.categoryIndex = categoryOf ("KNOBS");
                    const int header = (int) entries.size();
                    entries.push_back (h);
                    for (int i = 0; i < list.count; ++i)
                        if (str (list.stages[i].knobParam) == pid)
                        {
                            Entry e;
                            e.kind = Entry::stage;
                            e.stageInfo = &list.stages[i];
                            e.categoryIndex = h.categoryIndex;
                            e.knobGroup = header;
                            entries.push_back (e);
                        }
                    for (int kind = 0; kind < m::numModifierKinds; ++kind)
                    {
                        Entry e;
                        e.kind = Entry::modifier;
                        e.knob = k;
                        e.modifierKind = kind;
                        e.categoryIndex = h.categoryIndex;
                        e.knobGroup = header;
                        entries.push_back (e);
                    }
                }

                Entry r;
                r.kind = Entry::reset;
                r.categoryIndex = -1;
                entries.push_back (r);
            }
            pollValues();
        }
        layoutEntries();
    }

    void GlassPanel::showTab (int t)
    {
        t = juce::jlimit (0, std::max (0, categories.size() - 1), t);
        if (t == tab)
            return;
        tab = t;
        scrollY = scrollTarget = 0.0f;
        for (auto& e : entries)
            e.open = e.openTarget = 0.0f;
        lockerNote = {};
        layoutEntries();
    }

    //==============================================================================
    float GlassPanel::contentHeight() const noexcept
    {
        float h = 0.0f;
        for (auto& e : entries)
            h = std::max (h, e.y + e.h);
        return h;
    }

    int GlassPanel::resetEntry() const noexcept
    {
        for (int i = 0; i < (int) entries.size(); ++i)
            if (entries[(size_t) i].kind == Entry::reset)
                return i;
        return -1;
    }

    float GlassPanel::footerHeight() const noexcept
    {
        return helpH + (unit == layout::tunerUnit ? 28.0f : 0.0f) + (resetEntry() >= 0 ? resetH : 0.0f);   // (RACK TUNER: room for what the tune did)
    }

    /** The open tab's entries stacked (the others take no room); the panel sized to fit (up to its maximum). */
    void GlassPanel::layoutEntries()
    {
        dirty = true;
        if (unit < 0)
        {
            panel = {};
            return;
        }
        float y = 6.0f;
        for (auto& e : entries)
        {
            const bool here = e.categoryIndex == tab && e.kind != Entry::category && e.kind != Entry::reset;
            switch (e.kind)
            {
                case Entry::knobHeader:   e.h = groupH; break;
                case Entry::lockerUnit:   e.h = unitRowHeight (e); break;
                case Entry::lockerGroup:  e.h = groupH + 8.0f; break;
                case Entry::lockerSub:    e.h = 24.0f; break;
                case Entry::designAction: e.h = actionH; break;
                case Entry::tunerAction:  e.h = actionH - 6.0f; break;
                case Entry::tunerChips:   { const auto boxes = chipBoxes (e); e.h = boxes.empty() ? 0.0f : boxes.back().getBottom() - entryBox (e).getY() + 10.0f; break; }
                case Entry::stage:
                case Entry::modifier:     e.h = rowH + (optionH * (float) e.numChoices() + 8.0f) * e.open; break;
                default:                  e.h = 0.0f; break;
            }
            if (! here || (e.kind == Entry::lockerUnit && ! matchesSearch (e)))
                e.h = 0.0f;
            // (the locker's categories: a group shows while it has a match; what is in it, as far as it is folded out)
            if (e.kind == Entry::lockerGroup && ! groupHasMatch ((int) (&e - entries.data())))
                e.h = 0.0f;
            if (e.kind == Entry::lockerSub && ! groupHasMatch (e.knobGroup, (int) (&e - entries.data())))
                e.h = 0.0f;
            if ((e.kind == Entry::lockerUnit || e.kind == Entry::lockerSub) && e.knobGroup >= 0)
                e.h *= search.isNotEmpty() ? 1.0f : entries[(size_t) e.knobGroup].open;   // (searching: every group with a match, open)
            e.y = y;
            y += e.h;
        }
        y += 6.0f;

        const float w = std::min (width, view.getWidth() - 2.0f * gutter);
        const float h = std::min ({ headerH + tabsH + searchHeight() + y + footerHeight(), maxHeight, view.getHeight() - 2.0f * gutter });
        const float x = view.getRight() - w - gutter;
        const float top = juce::jlimit (view.getY() + gutter, std::max (view.getY() + gutter, view.getBottom() - gutter - h), anchorY - 0.5f * h);
        panel = { x, top, w, h };

        const float maxScroll = std::max (0.0f, y - listArea().getHeight());
        scrollTarget = juce::jlimit (0.0f, maxScroll, scrollTarget);
        scrollY = juce::jlimit (0.0f, maxScroll, scrollY);
    }

    /** THE GEAR LOCKER's search: a unit or module matches when every word typed is in its name or what it does. */
    bool GlassPanel::matchesSearch (const Entry& e) const
    {
        if (search.isEmpty() || e.kind != Entry::lockerUnit)
            return true;
        juce::String hay;
        if (e.lbModule >= 0)
            hay << layout::lb::nameOf (e.lbModule) << " " << (e.lbModule >= 4 ? enh::dsp::lbmods::info[e.lbModule - 4].role : "") << " 500 lunchbox module";
        else if (e.rackUnit >= 0)
            hay << layout::unitInfo[(size_t) e.rackUnit].name << " " << layout::unitInfo[(size_t) e.rackUnit].role;
        if (e.knobGroup >= 0 && e.knobGroup < (int) entries.size())   // (and the category and section it is filed under)
            hay << " " << entries[(size_t) e.knobGroup].title << " " << e.title;
        hay = hay.toLowerCase();
        juce::StringArray words; words.addTokens (search.toLowerCase(), " ", {});
        for (const auto& w : words)
            if (w.isNotEmpty() && ! hay.contains (w))
                return false;
        return true;
    }

    float GlassPanel::searchHeight() const noexcept { return unit == lockerPage || (unit == layout::tunerUnit && tab == 0) ? 44.0f : 0.0f; }

    bool GlassPanel::tunerShowsBefore() const
    {
        const int ab = bridge.indexOf ("tnAB");
        return ab >= 0 && bridge.getNormalised (ab) > 0.5f;
    }

    std::vector<juce::Rectangle<float>> GlassPanel::chipBoxes (const Entry& e) const
    {
        std::vector<juce::Rectangle<float>> out;
        const auto box = entryBox (e);
        const auto f = font (11.5f, true);
        float x = box.getX(), y = box.getY() + 6.0f;
        for (const auto& w : tuner::chipWords())
        {
            const float cw = juce::GlyphArrangement::getStringWidth (f, w) + 18.0f;
            if (x + cw > box.getRight() && x > box.getX()) { x = box.getX(); y += 30.0f; }
            out.push_back ({ x, y, cw, 24.0f });
            x += cw + 6.0f;
        }
        return out;
    }

    juce::Rectangle<float> GlassPanel::searchBox() const noexcept
    {
        return { panel.getX() + pad, panel.getY() + headerH + tabsH + 8.0f, panel.getWidth() - 2.0f * pad, 30.0f };
    }

    bool GlassPanel::keyPressed (const juce::KeyPress& k)
    {
        if (unit == layout::tunerUnit)   // RACK TUNER: its words (Enter tunes)
        {
            auto w = tuner.getWords();
            if (k == juce::KeyPress::returnKey) { lockerNote = tuner.tune(); dirty = true; return true; }
            if (k == juce::KeyPress::escapeKey) w.clear();
            else if (k == juce::KeyPress::backspaceKey) w = w.dropLastCharacters (1);
            else
            {
                const auto c = k.getTextCharacter();
                if (c < 32 || k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown()) return false;
                if (w.length() < 80) w += juce::String::charToString (c);
            }
            tuner.setWords (w);
            dirty = true;
            return true;
        }
        if (unit != lockerPage)
            return false;
        const auto before = search;
        if (k == juce::KeyPress::escapeKey) search.clear();
        else if (k == juce::KeyPress::backspaceKey) search = search.dropLastCharacters (1);
        else
        {
            const auto c = k.getTextCharacter();
            if (c < 32 || k.getModifiers().isCommandDown() || k.getModifiers().isCtrlDown()) return false;
            if (search.length() < 40) search += juce::String::charToString (c);
        }
        if (search == before) return true;
        scrollY = scrollTarget = 0.0f;
        lockerNote = {};
        layoutEntries();
        return true;
    }

    juce::Rectangle<float> GlassPanel::listArea() const noexcept
    {
        const float top = panel.getY() + headerH + tabsH + searchHeight();
        return { panel.getX(), top, panel.getWidth(), std::max (0.0f, panel.getBottom() - footerHeight() - top) };
    }

    juce::String GlassPanel::tabLabel (int t) const
    {
        juce::String label = tabName (categories[t]);
        if (unit == lockerPage)
        {
            std::set<int> seen;   // (a unit in two categories counts once)
            for (auto& e : entries) if (e.kind == Entry::lockerUnit && e.categoryIndex == t && matchesSearch (e)) seen.insert (e.lbModule >= 0 ? 1000 + e.lbModule : e.rackUnit);
            label << "  " << (int) seen.size();
        }
        return label;
    }

    juce::Rectangle<float> GlassPanel::tabBox (int t) const noexcept
    {
        // Tabs share the width by the length of their names
        const auto f = font (12.5f, true, 0.01f);
        float total = 0.0f, before = 0.0f, mine = 0.0f;
        for (int i = 0; i < categories.size(); ++i)
        {
            const float tw = juce::GlyphArrangement::getStringWidth (f, tabLabel (i)) + 20.0f;
            if (i < t) before += tw;
            if (i == t) mine = tw;
            total += tw;
        }
        const float avail = panel.getWidth() - 2.0f * pad;
        const float k = total > avail ? avail / total : 1.0f;
        return { panel.getX() + pad + before * k, panel.getY() + headerH, mine * k, tabsH };
    }

    juce::Rectangle<float> GlassPanel::resetBox() const noexcept
    {
        return { panel.getX() + pad, panel.getBottom() - resetH + 8.0f, panel.getWidth() - 2.0f * pad, resetH - 20.0f };
    }

    juce::Rectangle<float> GlassPanel::entryBox (const Entry& e) const noexcept
    {
        const auto list = listArea();
        return { list.getX() + pad, list.getY() + e.y - scrollY, list.getWidth() - 2.0f * pad, e.h };
    }

    juce::Rectangle<float> GlassPanel::optionBox (const Entry& e, int k) const noexcept
    {
        const auto box = entryBox (e);
        return { box.getX(), box.getY() + rowH + optionH * (float) k, box.getWidth(), optionH };
    }

    //==============================================================================
    Hit GlassPanel::hitTest (juce::Point<float> p) const
    {
        Hit hit;
        if (unit < 0 || ! panel.contains (p))
            return hit;
        hit.inside = true;
        if (categories.size() > 1)
            for (int t = 0; t < categories.size(); ++t)
                if (tabBox (t).contains (p))
                {
                    hit.tab = t;
                    return hit;
                }
        if ((unit == lockerPage || searchHeight() > 0.0f) && searchBox().contains (p))
        {
            hit.search = true;
            return hit;
        }
        if (const int r = resetEntry(); r >= 0 && resetBox().contains (p))
        {
            hit.entry = r;
            return hit;
        }
        const auto list = listArea();
        if (! list.contains (p))
            return hit;
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            const auto& e = entries[(size_t) i];
            const auto box = entryBox (e);
            if (e.h < 6.0f || e.kind == Entry::knobHeader || e.kind == Entry::lockerSub || ! box.expanded (pad, 0.0f).contains (p))
                continue;
            hit.entry = i;
            if (e.kind == Entry::tunerChips)
            {
                const auto boxes = chipBoxes (e);
                for (int k = 0; k < (int) boxes.size(); ++k)
                    if (boxes[(size_t) k].contains (p)) hit.option = k;
            }
            if ((e.kind == Entry::stage || e.kind == Entry::modifier) && e.open > 0.9f)
                for (int k = 0; k < e.numChoices(); ++k)
                    if (optionBox (e, k).expanded (pad, 0.0f).contains (p))
                        hit.option = k;
        }
        return hit;
    }

    bool GlassPanel::hover (juce::Point<float> p)
    {
        const auto h = hitTest (p);
        if (h == hovered)
            return false;
        hovered = h;
        dirty = true;
        return true;
    }

    void GlassPanel::unhover()
    {
        if (hovered.entry >= 0 || hovered.tab >= 0 || hovered.inside)
        {
            hovered = {};
            dirty = true;
        }
    }

    void GlassPanel::click (juce::Point<float> p)
    {
        const auto h = hitTest (p);
        if (h.tab >= 0)
        {
            showTab (h.tab);
            hovered = hitTest (p);
            return;
        }
        if (h.entry < 0)
            return;
        auto& e = entries[(size_t) h.entry];
        switch (e.kind)
        {
            case Entry::reset:
                resetUnit();
                break;
            case Entry::designAction:
            {
                if (e.rackUnit == 0)
                {
                    const auto why = processor.setCustomCode (juce::SystemClipboard::getTextFromClipboard().substring (0, 30000), true);
                    lockerNote = why.isEmpty() ? "Loaded: " + pad::custom::decode (processor.getCustomCode()).name + ". Its knobs and switches are where the design put them."
                                               : why + " Copy a code (or a share link) from the designer first.";
                }
                else { processor.setCustomCode ({}, false); lockerNote = "The slot is empty: it passes the sound untouched."; }
                break;
            }
            case Entry::tunerAction:
                if (e.rackUnit == 2)   // A/B: the faceplate's switch flipped (the tuner follows it)
                {
                    if (const int ab = bridge.indexOf ("tnAB"); ab >= 0)
                        bridge.setValueWithSource (ab, tunerShowsBefore() ? 0.0f : 1.0f, ControlSource::user);
                }
                else lockerNote = e.rackUnit == 0 ? tuner.tune() : tuner.undo();
                dirty = true;
                break;
            case Entry::tunerChips:
                if (h.option >= 0)
                {
                    const auto word = tuner::chipWords()[h.option];
                    auto w = tuner.getWords().trim();
                    if ((" " + w + " ").contains (" " + word + " ")) w = (" " + w + " ").replace (" " + word + " ", " ").trim();   // (clicked again: taken out)
                    else w = (w + " " + word).trim();
                    tuner.setWords (w);
                    dirty = true;
                }
                break;
            case Entry::lockerGroup:
                e.openTarget = e.openTarget > 0.5f ? 0.0f : 1.0f;
                if (e.openTarget > 0.5f) openGroups.insert (e.title); else openGroups.erase (e.title);
                break;
            case Entry::lockerUnit:
                if (e.lbModule >= 0) toggleModule (e.lbModule);
                else                 toggleStored (e.rackUnit);
                break;
            case Entry::stage:
            case Entry::modifier:
                if (h.option >= 0)
                {
                    choose (e, h.option);
                    e.openTarget = 0.0f;     // a choice folds its list away
                }
                else if (e.numChoices() > 1)
                {
                    const bool opening = e.openTarget < 0.5f;
                    for (auto& other : entries)   // one list open at a time
                        if (other.kind == Entry::stage || other.kind == Entry::modifier)
                            other.openTarget = 0.0f;
                    e.openTarget = opening ? 1.0f : 0.0f;
                }
                break;
            default:
                break;
        }
        dirty = true;
    }

    bool GlassPanel::scroll (float deltaPx)
    {
        const float before = scrollTarget;
        const float maxScroll = std::max (0.0f, contentHeight() + 6.0f - listArea().getHeight());
        scrollTarget = juce::jlimit (0.0f, maxScroll, scrollTarget - deltaPx);
        return scrollTarget != before;
    }

    bool GlassPanel::tick (float dt)
    {
        if (unit < 0)
            return false;
        bool moving = false;
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            auto& e = entries[(size_t) i];
            const float o = e.open, h = e.hover;
            e.open = ease (e.open, e.openTarget, 18.0f, dt);
            e.hover = ease (e.hover, hovered.entry == i && hovered.option < 0 ? 1.0f : 0.0f, 22.0f, dt);
            moving = moving || e.open != o || e.hover != h;
        }
        const float s = scrollY, ts = tabSlide;
        scrollY = ease (scrollY, scrollTarget, 18.0f, dt);
        tabSlide = ease (tabSlide, (float) tab, 20.0f, dt);
        moving = moving || scrollY != s || tabSlide != ts;
        if (moving)
            layoutEntries();
        return moving;
    }

    void GlassPanel::setExpanded (int setting, int hoveredOption, bool)
    {
        int n = 0, found = -1;
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            auto& e = entries[(size_t) i];
            if (e.kind == Entry::stage || e.kind == Entry::modifier)
            {
                const bool it = n++ == setting;
                e.open = e.openTarget = it ? 1.0f : 0.0f;
                if (it)
                    found = i;
            }
        }
        if (found >= 0)
        {
            tab = entries[(size_t) found].categoryIndex;
            tabSlide = (float) tab;
            if (hoveredOption >= 0)
                hovered = { found, hoveredOption, -1, true };
        }
        layoutEntries();
        if (found >= 0)
        {
            scrollY = scrollTarget = std::max (0.0f, entries[(size_t) found].y - 40.0f);
            layoutEntries();
        }
    }

    //==============================================================================
    int GlassPanel::currentChoice (const Entry& e) const
    {
        if (e.kind == Entry::stage)
        {
            if (e.stageInfo->param.empty())
                return 0;
            const int p = bridge.indexOf (str (e.stageInfo->param));
            return p < 0 ? 0 : juce::jlimit (0, e.stageInfo->numMethods - 1, juce::roundToInt (bridge.getNormalised (p) * (float) (e.stageInfo->numMethods - 1)));
        }
        if (e.kind == Entry::modifier)
        {
            const float v = processor.getKnobModifier (e.knob, e.modifierKind);
            const auto& c = m::modifiers[(size_t) e.modifierKind].choices;
            int best = 0;
            for (int k = 1; k < (int) c.size(); ++k)
                if (std::abs (c[(size_t) k] - v) < std::abs (c[(size_t) best] - v))
                    best = k;
            return best;
        }
        return 0;
    }

    void GlassPanel::choose (const Entry& e, int choice)
    {
        if (e.kind == Entry::stage)
        {
            if (e.stageInfo->param.empty() || e.stageInfo->numMethods < 2)
                return;
            const int p = bridge.indexOf (str (e.stageInfo->param));
            if (p < 0)
                return;
            bridge.beginGesture (p, ControlSource::user);
            bridge.setValueWithSource (p, (float) choice / (float) (e.stageInfo->numMethods - 1), ControlSource::user);
            bridge.endGesture (p);
        }
        else if (e.kind == Entry::modifier)
        {
            processor.setKnobModifier (e.knob, e.modifierKind, m::modifiers[(size_t) e.modifierKind].choices[(size_t) juce::jlimit (0, 3, choice)]);
        }
        dirty = true;
    }

    void GlassPanel::resetUnit()
    {
        for (auto& e : entries)
            if ((e.kind == Entry::stage || e.kind == Entry::modifier) && currentChoice (e) != 0)
                choose (e, 0);
    }

    void GlassPanel::pollValues()
    {
        std::vector<int> now;
        for (auto& e : entries)
            now.push_back (currentChoice (e));
        if (now != shownChoices)
        {
            shownChoices = std::move (now);
            dirty = true;
        }
    }

    juce::String GlassPanel::choiceText (const Entry& e, int k) const
    {
        if (e.kind == Entry::stage)
            return methodLabel (e.stageInfo->methods[k]);
        return plain (str (m::modifiers[(size_t) e.modifierKind].labels[(size_t) k]));
    }

    juce::String GlassPanel::valueText (const Entry& e, int k) const
    {
        return choiceText (e, k);
    }


    //==============================================================================
    namespace
    {
        /** THE GEAR LOCKER's categories: groups that fold out, each in sections; a unit by its key (the newer
            units' keys are UnitList.h's; the rest named here). A unit may sit in several. */
        struct LockerSub { const char* name; std::vector<const char*> keys; };
        struct LockerGroup { const char* name; std::vector<LockerSub> subs; };
        const std::vector<LockerGroup>& lockerCategories()
        {
            static const std::vector<LockerGroup> table {
                { "Low end", { { "Sub bass", { "deepsub", "submaxx" } }, { "Simulated sub bass", { "subdriver" } }, { "Bass control", { "seismo", "lathe" } } } },
                { "Dynamics", { { "Compressors and levellers", { "compressor", "leveler", "opto", "varimu", "level" } },
                                { "Limiters and loudness", { "limiter", "clip", "pressure", "hourglass", "rider" } },
                                { "Transients and feel", { "sonar", "suspension", "takeback" } } } },
                { "EQ and tone", { { "Tone and balance", { "tonespace", "dyneq", "balance", "balancer" } }, { "Clarity and presence", { "enhancer", "detail", "clarity", "maximizer" } },
                                   { "Air and sheen", { "aurora", "velvet" } }, { "Harshness and resonances", { "deharsh", "skyline" } } } },
                { "Filters", { { "Resonant filters", { "lavalamp", "talkbox" } }, { "Formant and vocal", { "vocoder", "talkbox" } } } },
                { "Saturation and colour", { { "Tape", { "tape", "cassette" } }, { "Valve and heat", { "valveamp", "furnace", "character" } },
                                             { "Multiband colour", { "prism", "x4" } }, { "Clipping", { "clip" } } } },
                { "Space", { { "Space and tone", { "chroma" } }, { "Reverbs", { "shimmer", "plate", "spring" } }, { "Rooms", { "rayroom", "club", "cartest" } }, { "Echoes and delays", { "tapeecho", "bounce", "grain" } } } },
                { "Modulation and movement", { { "Chorus and rotation", { "bbd", "rotary" } }, { "Motion", { "pendulum", "flyby" } }, { "Pitch and frequency", { "harm", "bode" } } } },
                { "Stereo and phase", { { "Width", { "shuffler", "field" } }, { "Phase", { "rotator", "compass" } } } },
                { "Simulated", { { "Machines and media", { "vinyl", "cassette", "tapeecho", "radio", "lathe", "dither" } },
                                 { "Physical objects", { "pendulum", "bounce", "sympathy", "tesla", "lavalamp", "talkbox", "rotary" } },
                                 { "Speakers and rooms", { "speakercab", "rayroom", "cartest", "phonecheck", "club", "subdriver" } },
                                 { "Mastering simulations", { "detail", "clarity", "subdriver", "lathe", "cartest", "phonecheck", "club", "pressure", "balance", "field", "sonar",
                                                              "seismo", "prism", "furnace", "dither", "rider", "compass", "suspension", "skyline", "hourglass", "aurora" } } } },
                { "Mastering", { { "Final stage", { "hourglass", "pressure", "dither", "limiter" } }, { "Translation checks", { "cartest", "phonecheck", "club" } },
                                 { "Loudness", { "rider", "level" } } } },
                { "Lo-fi and character", { { "Worn and vintage", { "vinyl", "radio", "cassette" } }, { "Wild", { "tesla", "sympathy", "vocoder", "chroma" } } } },
                { "Meters and tools", { { "Scopes and meters", { "scope", "radar" } }, { "Visualisers", { "hypercube" } }, { "Whole-rack tuning", { "tuner" } } } },
                { "Designed units", { { "From the Rack Unit Designer", { "x4", "velvet", "takeback", "custom" } } } },
            };
            return table;
        }

        int unitOfKey (std::string_view key)
        {
            using namespace layout;
            struct Named { const char* key; int unit; };
            static const Named named[] { { "enhancer", enhUnit }, { "tonespace", tubeUnit }, { "compressor", tideUnit }, { "leveler", lumenUnit }, { "limiter", limiterUnit },
                                         { "level", levelUnit }, { "balancer", balancerUnit }, { "deepsub", deepUnit }, { "character", characterUnit }, { "radar", radarUnit },
                                         { "x4", x4Unit }, { "velvet", velvetUnit }, { "takeback", takebackUnit }, { "scope", scopeUnit }, { "custom", customUnit } };
            for (const auto& n : named) if (key == n.key) return n.unit;
            for (int k = 0; k < enh::dsp::units::count; ++k) if (key == enh::dsp::units::info[k].key) return firstGenUnit + k;
            return -1;
        }
    }

    /** A category (or one of its sections) shows while something in it matches the search. */
    bool GlassPanel::groupHasMatch (int groupEntry, int subEntry) const
    {
        if (groupEntry < 0) return true;
        const int from = subEntry >= 0 ? subEntry + 1 : groupEntry + 1;
        for (int i = from; i < (int) entries.size(); ++i)
        {
            const auto& e = entries[(size_t) i];
            if (e.knobGroup != groupEntry || e.kind == Entry::lockerGroup) break;
            if (subEntry >= 0 && e.kind == Entry::lockerSub) break;
            if (e.kind == Entry::lockerUnit && matchesSearch (e)) return true;
        }
        return false;
    }

    /** A unit's row: its name and size over its faceplate's picture (as tall as the plate is at the row's width);
        a 500-series module's has no picture. */
    float GlassPanel::unitRowHeight (const Entry& e) const
    {
        if (e.lbModule >= 0 || e.rackUnit < 0) return lockerRowH;
        const float w = std::min (width, view.getWidth() - 2.0f * gutter) - 2.0f * pad - 80.0f;
        return 30.0f + w * layout::unitHalfH (e.rackUnit) / layout::unitHalfW (e.rackUnit) + 10.0f;
    }

    /** A unit's faceplate, painted once at `pixelWidth` (or wider) - at most `budget` new ones a frame. */
    const juce::Image* GlassPanel::thumbFor (int u, float pixelWidth, int& budget)
    {
        auto it = thumbs.find (u);
        if (it != thumbs.end() && (float) it->second.getWidth() >= pixelWidth * 0.95f) return &it->second;
        if (budget <= 0) return it != thumbs.end() ? &it->second : nullptr;
        --budget;
        // (painted at least 900 px wide: from a print that small, lettering still reads when scaled down)
        thumbs[u] = face::render (u, std::max (900.0f, pixelWidth) / (2.0f * layout::unitHalfW (u)), true);
        return &thumbs[u];
    }

    //==============================================================================
    /** THE GEAR LOCKER: what is in the rack (bottom to top, the order the sound runs), then what is stored. */
    void GlassPanel::buildLocker()
    {
        entries.clear();
        categories.clear();
        thumbs.erase (layout::customUnit);   // (CUSTOM's look changes with the design loaded)
        const auto stored = processor.getStoredUnits();
        for (int pass = 0; pass < 2; ++pass)
        {
            Entry c;
            c.kind = Entry::category;
            c.title = pass == 0 ? "In the rack" : "Locker";
            c.categoryIndex = pass;
            c.open = c.openTarget = 1.0f;
            categories.add (c.title);
            entries.push_back (c);
            if (pass == 0)   // in the rack: bottom to top, the order the sound runs
            {
                for (int u : layout::rackOrder)
                {
                    if (! layout::isStorable (u) || (((stored >> u) & 1u) != 0u))
                        continue;
                    Entry e;
                    e.kind = Entry::lockerUnit;
                    e.rackUnit = u;
                    e.categoryIndex = 0;
                    entries.push_back (e);
                }
                continue;
            }
            // the locker: by category, each folding out, with its sections (a unit can be in more than one)
            std::set<int> placed;
            auto storedHere = [&] (int u) { return u >= 0 && layout::isStorable (u) && ((stored >> u) & 1u) != 0u; };
            auto addGroup = [&] (const juce::String& title)
            {
                Entry g; g.kind = Entry::lockerGroup; g.title = title; g.categoryIndex = 1;
                g.open = g.openTarget = openGroups.count (title) > 0 ? 1.0f : 0.0f;
                entries.push_back (g);
                return (int) entries.size() - 1;
            };
            for (const auto& group : lockerCategories())
            {
                const int gi = addGroup (group.name);
                bool any = false;
                for (const auto& sub : group.subs)
                {
                    std::vector<int> units;
                    for (const auto* key : sub.keys) if (const int u = unitOfKey (key); storedHere (u)) units.push_back (u);
                    if (units.empty()) continue;
                    Entry sh; sh.kind = Entry::lockerSub; sh.title = sub.name; sh.categoryIndex = 1; sh.knobGroup = gi;
                    entries.push_back (sh);
                    for (int u : units)
                    {
                        Entry e; e.kind = Entry::lockerUnit; e.rackUnit = u; e.categoryIndex = 1; e.knobGroup = gi; e.title = sub.name;   // (its section: searched too)
                        entries.push_back (e); placed.insert (u); any = true;
                    }
                }
                if (! any) entries.erase (entries.begin() + gi, entries.end());
            }
            // anything no category names (a unit added later): its own group
            std::vector<int> rest;
            for (int u : layout::rackOrder) if (storedHere (u) && placed.count (u) == 0) rest.push_back (u);
            for (int u = 0; u < layout::numUnits; ++u) if (storedHere (u) && placed.count (u) == 0 && std::find (rest.begin(), rest.end(), u) == rest.end()) rest.push_back (u);
            if (! rest.empty())
            {
                const int gi = addGroup ("Other");
                for (int u : rest) { Entry e; e.kind = Entry::lockerUnit; e.rackUnit = u; e.categoryIndex = 1; e.knobGroup = gi; entries.push_back (e); }
            }
        }
        // The LUNCHBOX's modules (its meter always stays in), in the order they sit in the frame
        Entry c;
        c.kind = Entry::category;
        c.title = "500 series";
        c.categoryIndex = 2;
        categories.add (c.title);
        entries.push_back (c);
        for (int m : layout::lb::signalOrder())
        {
            if (m == 3) continue;
            Entry e;
            e.kind = Entry::lockerUnit;
            e.lbModule = m;
            e.categoryIndex = 2;
            entries.push_back (e);
        }
    }

    void GlassPanel::toggleModule (int m)
    {
        auto mask = processor.getStoredModules();
        const std::uint32_t bit = std::uint32_t { 1 } << m;
        if ((mask & bit) != 0u)
        {
            const int need = layout::lb::slotsFor (mask & ~bit) - layout::lbSlots;
            if (need > 0)
            {
                lockerNote = juce::String (layout::lb::nameOf (m)) + " needs " + juce::String (need) + (need == 1 ? " more slot" : " more slots")
                           + " in the LUNCHBOX: take a module out first.";
                dirty = true;
                return;
            }
            mask &= ~bit;
        }
        else
            mask |= bit;
        processor.setStoredModules (mask);
        lockerNote = {};
        buildLocker();
        layoutEntries();
        pollValues();
    }

    void GlassPanel::toggleStored (int u)
    {
        auto mask = processor.getStoredUnits();
        if (((mask >> u) & 1u) != 0u)
        {
            const int need = layout::usedU (mask & ~layout::unitBit (u)) - layout::rackCapacityU;
            if (need > 0)
            {
                lockerNote = juce::String (layout::unitInfo[(size_t) u].name) + " needs " + juce::String (need)
                           + "U more room: put a unit away first.";
                dirty = true;
                return;
            }
            mask &= ~layout::unitBit (u);
        }
        else
            mask |= layout::unitBit (u);
        processor.setStoredUnits (mask);
        layout::storedUnits = mask;
        lockerNote = {};
        buildLocker();
        layoutEntries();
        pollValues();
    }

    //==============================================================================
    artwork::RawTexture GlassPanel::render (float pixelScale)
    {
        dirty = false;
        if (unit < 0 || panel.isEmpty())
            return {};

        const float s = juce::jlimit (1.0f, 3.0f, pixelScale);
        const int w = juce::roundToInt (panel.getWidth() * s), h = juce::roundToInt (panel.getHeight() * s);
        juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::translation (-panel.getX(), -panel.getY()).scaled (s));

            // (HOLOGRAM: the same layout in the scope's green - bright for what matters, never dim to unreadable)
            const auto white = holo ? juce::Colour (0xff8dffb2) : juce::Colours::white;
            const auto soft = white.withAlpha (holo ? 0.78f : 0.62f), dim = white.withAlpha (holo ? 0.55f : 0.40f), line = white.withAlpha (holo ? 0.14f : 0.07f);
            const auto amber = holo ? juce::Colour (0xffd8ff6a) : pad::amber;
            const float x0 = panel.getX(), y0 = panel.getY(), pw = panel.getWidth();
            auto text = [&] (const juce::String& t, juce::Rectangle<float> r, const juce::Font& f, juce::Colour c,
                             juce::Justification j = juce::Justification::centredLeft)
            {
                if (holo)   // HOLOGRAM: drawn by the beam, in the scope's single-stroke letters
                {
                    holo::vec::text (g, t, r, f.getHeight() * 0.68f, c, j, 1.0f, t.hashCode() & 0xffff, 0.45f);
                    return;
                }
                g.setFont (f);
                g.setColour (c);
                g.drawText (t, r, j, true);
            };
            auto chevron = [&] (float cx, float cy, float turn, juce::Colour c)   // ">" turning to "v" as its list opens
            {
                juce::Path p;
                p.startNewSubPath (-2.0f, -4.0f); p.lineTo (2.0f, 0.0f); p.lineTo (-2.0f, 4.0f);
                p.applyTransform (juce::AffineTransform::rotation (turn * juce::MathConstants<float>::halfPi).translated (cx, cy));
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            };
            auto settingName = [&] (const Entry& e)
            {
                return e.kind == Entry::stage ? plain (str (e.stageInfo->name)) : plain (str (m::modifiers[(size_t) e.modifierKind].fullName));
            };
            auto isChanged = [&] (const Entry& e) { return (e.kind == Entry::stage || e.kind == Entry::modifier) && currentChoice (e) != 0; };

            // --- header: the name, and one quiet line under it --------------------------------
            const bool locker = unit == lockerPage;
            int changed = 0;
            for (auto& e : entries)
                changed += isChanged (e) ? 1 : 0;
            juce::String name, sub;
            if (locker)
            {
                const auto stored = processor.getStoredUnits();
                name = "Gear locker";
                sub = tab == 2 ? "500-SERIES RACK " + juce::String (layout::lb::slotsFor (processor.getStoredModules())) + " of " + juce::String (layout::lbSlots) + " slots in use"
                               : "Rack " + juce::String (layout::usedU (stored)) + " of " + juce::String (layout::rackCapacityU) + "U in use";
            }
            else
            {
                name = juce::String (layout::unitInfo[(size_t) unit].name);
                // What this unit delays the sound by (look-ahead, oversampling): gamers want to know
                // (EnhEngine::getLatencyBreakdown order: enhancer, radar, tone & space, character, ear guard, limiter)
                const int stage = unit == layout::enhUnit ? 0 : unit == layout::radarUnit ? 1 : unit == layout::tubeUnit ? 2
                                : unit == layout::characterUnit ? 3 : -1;
                if (unit == layout::monitorUnit)
                    sub = "Whole rack delays the sound " + juce::String (processor.getRackLatencyMs(), 1) + " ms";
                else if (stage >= 0)
                    sub = "Delays the sound " + juce::String (processor.getStageLatencyMs (stage), 1) + " ms";
                else
                    sub = "No delay";
                if (changed > 0)
                    sub << sep << changed << (changed == 1 ? " setting changed" : " settings changed");
            }
            text (name, { x0 + pad, y0 + 14.0f, pw - 2.0f * pad, 22.0f }, font (17.0f, true, 0.01f), white);
            text (sub, { x0 + pad, y0 + 35.0f, pw - 2.0f * pad, 14.0f }, font (11.0f, false), soft);

            // --- tabs ---------------------------------------------------------------------------
            const float tabsY = y0 + headerH;
            g.setColour (line);
            g.fillRect (x0 + pad, tabsY + tabsH - 1.0f, pw - 2.0f * pad, 1.0f);
            if (categories.size() > 0)
            {
                for (int t = 0; t < categories.size(); ++t)
                {
                    const auto b = tabBox (t);
                    int n = 0;
                    for (auto& e : entries)
                        n += e.categoryIndex == t && isChanged (e) ? 1 : 0;
                    const bool on = t == tab, over = hovered.tab == t;
                    const juce::String label = tabLabel (t);
                    text (label, b.withTrimmedBottom (2.0f), font (12.5f, on, 0.01f), on ? white : over ? white.withAlpha (0.85f) : dim, juce::Justification::centred);
                    if (n > 0)
                    {
                        g.setColour (amber);
                        g.fillEllipse (b.getRight() - 7.0f, b.getY() + 10.0f, 4.0f, 4.0f);
                    }
                }
                // The underline slides to the open tab
                const int t0 = juce::jlimit (0, categories.size() - 1, (int) std::floor (tabSlide));
                const int t1 = juce::jmin (categories.size() - 1, t0 + 1);
                const float f = juce::jlimit (0.0f, 1.0f, tabSlide - (float) t0);
                const auto a = tabBox (t0), b = tabBox (t1);
                const float ux = a.getX() + (b.getX() - a.getX()) * f, uw = a.getWidth() + (b.getWidth() - a.getWidth()) * f;
                g.setColour (amber);
                g.fillRoundedRectangle (ux + 6.0f, tabsY + tabsH - 2.0f, uw - 12.0f, 2.0f, 1.0f);
            }

            // --- THE GEAR LOCKER's search --------------------------------------------------------
            if (locker)
            {
                const auto sb = searchBox();
                g.setColour (white.withAlpha (search.isEmpty() ? 0.05f : 0.09f));
                g.fillRoundedRectangle (sb, 7.0f);
                g.setColour (search.isEmpty() ? line : amber.withAlpha (0.6f));
                g.drawRoundedRectangle (sb, 7.0f, 1.0f);
                // the magnifier
                const float mx = sb.getX() + 16.0f, my = sb.getCentreY();
                g.setColour (soft);
                g.drawEllipse (mx - 6.0f, my - 6.0f, 10.0f, 10.0f, 1.5f);
                g.drawLine (mx + 2.5f, my + 2.5f, mx + 6.5f, my + 6.5f, 1.8f);
                const auto tb = sb.withTrimmedLeft (32.0f).withTrimmedRight (10.0f);
                if (search.isEmpty())
                    text ("Type to search units and modules", tb, font (12.0f, false), dim);
                else
                    text (search + "_", tb, font (12.5f, true), white);
                bool any = false;
                for (auto& e : entries) any = any || (e.kind == Entry::lockerUnit && e.categoryIndex == tab && matchesSearch (e));
                if (! any && search.isNotEmpty())
                    text ("Nothing here matches \"" + search + "\" - try another tab, or Esc to clear.", listArea().withHeight (40.0f).reduced (pad, 8.0f), font (11.5f, false), soft);
            }

            // --- RACK TUNER's words ------------------------------------------------------------------
            if (unit == layout::tunerUnit && searchHeight() > 0.0f)
            {
                const auto sb = searchBox();
                const auto& w = tuner.getWords();
                g.setColour (white.withAlpha (w.isEmpty() ? 0.05f : 0.09f));
                g.fillRoundedRectangle (sb, 7.0f);
                g.setColour (w.isEmpty() ? line : amber.withAlpha (0.7f));
                g.drawRoundedRectangle (sb, 7.0f, 1.0f);
                const auto tb = sb.reduced (12.0f, 0.0f);
                if (w.isEmpty()) text ("Type what you want: warm punchy hip-hop...", tb, font (12.0f, false), dim);
                else             text (w + "_", tb, font (12.5f, true), white);
            }

            // --- the list ---------------------------------------------------------------------
            const auto list = listArea();
            const Entry* detailEntry = nullptr;
            int detailChoice = 0;
            int preview = -1, thumbBudget = 2;   // (the hovered unit's big picture; new faceplates painted this frame)
            thumbsPending = false;
            {
                juce::Graphics::ScopedSaveState clipList (g);
                g.reduceClipRegion (list.toNearestInt());
                for (int i = 0; i < (int) entries.size(); ++i)
                {
                    const auto& e = entries[(size_t) i];
                    const auto box = entryBox (e);
                    if (e.h < 0.5f || box.getBottom() < list.getY() || box.getY() > list.getBottom())
                        continue;
                    juce::Graphics::ScopedSaveState clipEntry (g);
                    g.reduceClipRegion (box.expanded (pad, 0.0f).toNearestInt());
                    const auto hoverFill = [&] (juce::Rectangle<float> r, float amount)
                    {
                        if (amount > 0.01f)
                        {
                            g.setColour (white.withAlpha (0.05f * amount));
                            g.fillRoundedRectangle (r.expanded (8.0f, -2.0f), 5.0f);
                        }
                    };

                    if (e.kind == Entry::knobHeader)
                    {
                        text (e.title.toUpperCase(), { box.getX(), box.getY() + 12.0f, box.getWidth(), 14.0f }, font (10.0f, true, 0.14f), dim);
                    }
                    else if (e.kind == Entry::lockerUnit)
                    {
                        bool isStored; int need; juce::String name, size;
                        if (e.lbModule >= 0)
                        {
                            const auto stored = processor.getStoredModules();
                            const std::uint32_t bit = std::uint32_t { 1 } << e.lbModule;
                            isStored = (stored & bit) != 0u;
                            need = isStored ? layout::lb::slotsFor (stored & ~bit) - layout::lbSlots : 0;
                            name = layout::lb::nameOf (e.lbModule);
                            const int w = layout::lb::widthOf (e.lbModule);
                            size = juce::String (w) + (w == 1 ? " slot" : " slots") + (isStored && need > 0 ? sep + "needs " + juce::String (need) + " more" : juce::String());
                        }
                        else
                        {
                            const auto stored = processor.getStoredUnits();
                            isStored = ((stored >> e.rackUnit) & 1u) != 0u;
                            need = isStored ? layout::usedU (stored & ~layout::unitBit (e.rackUnit)) - layout::rackCapacityU : 0;
                            name = layout::unitInfo[(size_t) e.rackUnit].name;
                            size = juce::String (layout::unitU (e.rackUnit)) + "U" + (isStored && need > 0 ? sep + "needs " + juce::String (need) + "U more room" : juce::String());
                        }
                        const bool can = ! isStored || need <= 0;
                        hoverFill (box, e.hover);
                        text (name, { box.getX(), box.getY() + 7.0f, box.getWidth() - 80.0f, 17.0f }, font (13.0f, true), white);
                        text (size, { box.getX(), box.getY() + 24.0f, box.getWidth() - 80.0f, 14.0f }, font (11.0f, false), soft);
                        // its faceplate under its name (bigger, over the list, while it is hovered: drawn after it)
                        if (e.lbModule < 0 && e.rackUnit >= 0)
                        {
                            const juce::Rectangle<float> pic (box.getX(), box.getY() + 42.0f, box.getWidth() - 80.0f, box.getHeight() - 50.0f);
                            if (const auto* img = thumbFor (e.rackUnit, pic.getWidth() * s * 2.2f, thumbBudget))
                            {
                                g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (pic.translated (1.0f, 2.0f), 2.0f);
                                g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                                g.drawImage (*img, pic, juce::RectanglePlacement::stretchToFit);
                            }
                            else { g.setColour (white.withAlpha (0.06f)); g.fillRoundedRectangle (pic, 2.0f); thumbsPending = true; }
                            if (e.hover > 0.02f) { preview = i; }
                        }
                        const juce::Rectangle<float> b (box.getRight() - 70.0f, box.getY() + 8.0f, 70.0f, 24.0f);
                        g.setColour (white.withAlpha ((0.06f + 0.08f * e.hover) * (can ? 1.0f : 0.4f)));
                        g.fillRoundedRectangle (b, 5.0f);
                        text (isStored ? "Install" : "Store", b, font (11.5f, true), (isStored ? amber : white).withAlpha (can ? 1.0f : 0.35f), juce::Justification::centred);
                        g.setColour (line);
                        g.fillRect (box.getX(), box.getBottom() - 1.0f, box.getWidth(), 1.0f);
                    }
                    else if (e.kind == Entry::lockerGroup)
                    {
                        int n = 0; { std::set<int> seen; for (int k = i + 1; k < (int) entries.size() && entries[(size_t) k].knobGroup == i; ++k) if (entries[(size_t) k].kind == Entry::lockerUnit && matchesSearch (entries[(size_t) k])) seen.insert (entries[(size_t) k].rackUnit); n = (int) seen.size(); }
                        const float open = search.isNotEmpty() ? 1.0f : e.open;
                        const auto row = box.reduced (0.0f, 3.0f);
                        g.setColour (white.withAlpha (0.045f + 0.05f * e.hover + 0.03f * open));
                        g.fillRoundedRectangle (row.expanded (6.0f, 0.0f), 6.0f);
                        text (e.title, row.withTrimmedLeft (4.0f).withTrimmedRight (60.0f), font (13.0f, true), white);
                        text (juce::String (n), row.withTrimmedRight (24.0f), font (11.5f, false), soft, juce::Justification::centredRight);
                        chevron (row.getRight() - 8.0f, row.getCentreY(), open, white.withAlpha (0.5f + 0.4f * e.hover));
                    }
                    else if (e.kind == Entry::lockerSub)
                    {
                        text (e.title.toUpperCase(), { box.getX() + 4.0f, box.getY() + 8.0f, box.getWidth(), 13.0f }, font (9.5f, true, 0.14f), amber.withAlpha (0.8f));
                    }
                    else if (e.kind == Entry::designAction)
                    {
                        const auto b = box.reduced (0.0f, 8.0f);
                        g.setColour (white.withAlpha (0.06f + 0.08f * e.hover));
                        g.fillRoundedRectangle (b, 6.0f);
                        text (e.rackUnit == 0 ? "Paste a design code" : "Empty the slot", b, font (12.5f, true), e.rackUnit == 0 ? amber : white, juce::Justification::centred);
                    }
                    else if (e.kind == Entry::tunerChips)
                    {
                        const auto boxes = chipBoxes (e);
                        const auto words = (" " + tuner.getWords().toLowerCase() + " ");
                        for (int k = 0; k < (int) boxes.size(); ++k)
                        {
                            const auto& word = tuner::chipWords()[k];
                            const bool in = words.contains (" " + word + " "), over = hovered.entry == i && hovered.option == k;
                            g.setColour (in ? amber.withAlpha (0.85f) : white.withAlpha (over ? 0.14f : 0.07f));
                            g.fillRoundedRectangle (boxes[(size_t) k], 12.0f);
                            text (word, boxes[(size_t) k], font (11.5f, true), in ? juce::Colour (0xff15110c) : white.withAlpha (0.88f), juce::Justification::centred);
                        }
                    }
                    else if (e.kind == Entry::tunerAction)
                    {
                        const auto b = box.reduced (0.0f, 6.0f);
                        const bool before = tunerShowsBefore();
                        g.setColour (e.rackUnit == 0 ? amber.withAlpha (0.20f + 0.15f * e.hover) : white.withAlpha (0.06f + 0.08f * e.hover));
                        g.fillRoundedRectangle (b, 6.0f);
                        const juce::String label = e.rackUnit == 0 ? "Tune the rack" : e.rackUnit == 1 ? "Undo the last tune"
                                                 : before ? "A/B: hearing BEFORE - click for after" : "A/B: hearing AFTER - click for before";
                        text (label, b, font (12.5f, true), e.rackUnit == 0 ? amber : (e.rackUnit == 1 && ! tuner.hasHistory()) ? dim : white, juce::Justification::centred);
                    }
                    else if (e.kind == Entry::stage || e.kind == Entry::modifier)
                    {
                        const int current = currentChoice (e);
                        const bool changedHere = current != 0, many = e.numChoices() > 1;
                        const auto row = box.withHeight (rowH);
                        if (many)
                            hoverFill (row, std::max (e.hover, e.open));
                        // Name ........ Value >
                        const float valueW = row.getWidth() * 0.56f;
                        text (settingName (e), row.withWidth (row.getWidth() - valueW - 6.0f), font (12.5f, false), white.withAlpha (0.78f));
                        auto valueArea = row.withTrimmedLeft (row.getWidth() - valueW).withTrimmedRight (many ? 16.0f : 0.0f);
                        text (valueText (e, current), valueArea, font (12.5f, true), changedHere ? amber : white, juce::Justification::centredRight);
                        if (many)
                            chevron (row.getRight() - 5.0f, row.getCentreY(), e.open, white.withAlpha (0.45f + 0.4f * e.hover));

                        // Its choices, fading in as the list opens
                        if (e.open > 0.01f)
                            for (int k = 0; k < e.numChoices(); ++k)
                            {
                                const auto o = optionBox (e, k);
                                const bool on = k == current, oh = hovered.entry == i && hovered.option == k;
                                const float a = e.open;
                                if (oh)
                                {
                                    g.setColour (white.withAlpha (0.07f * a));
                                    g.fillRoundedRectangle (o.expanded (4.0f, -1.0f), 4.0f);
                                }
                                if (on)   // a check mark
                                {
                                    juce::Path tick;
                                    tick.startNewSubPath (o.getX() + 4.0f, o.getCentreY()); tick.lineTo (o.getX() + 7.5f, o.getCentreY() + 3.5f); tick.lineTo (o.getX() + 13.0f, o.getCentreY() - 4.0f);
                                    g.setColour (amber.withMultipliedAlpha (a));
                                    g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                                }
                                text (choiceText (e, k), o.withTrimmedLeft (22.0f).withTrimmedRight (60.0f), font (12.0f, on), (on || oh ? white : soft).withMultipliedAlpha (a));
                                if (k == 0)
                                    text ("default", o.withTrimmedRight (2.0f), font (10.5f, false), dim.withMultipliedAlpha (a), juce::Justification::centredRight);
                            }

                        if (hovered.entry == i)
                        {
                            detailEntry = &e;
                            detailChoice = hovered.option >= 0 ? hovered.option : current;
                        }
                        g.setColour (line);
                        g.fillRect (box.getX(), box.getBottom() - 1.0f, box.getWidth(), 1.0f);
                    }
                }
                // The hovered unit's faceplate, grown to the panel's width over the list (it eases in)
                if (preview >= 0)
                {
                    const auto& e = entries[(size_t) preview];
                    const auto box = entryBox (e);
                    const float t = e.hover * e.hover * (3.0f - 2.0f * e.hover);
                    const float fullW = panel.getWidth() - 12.0f, smallW = box.getWidth() - 80.0f;
                    // (it grows downward only, from under its name and its Install button - never over them - and
                    // no further than the list's bottom)
                    const float aspect = layout::unitHalfH (e.rackUnit) / layout::unitHalfW (e.rackUnit);
                    const float py = std::max (list.getY() + 4.0f, box.getY() + 42.0f);
                    const float roomW = std::max (smallW, (list.getBottom() - 4.0f - py) / aspect);
                    const float pw = smallW + (std::min (fullW, roomW) - smallW) * t, ph = pw * aspect;
                    const float px = box.getX() + (panel.getX() + 6.0f - box.getX()) * t * (pw - smallW) / std::max (1.0f, fullW - smallW);
                    const juce::Rectangle<float> big (px, py, pw, ph);
                    if (const auto* img = thumbFor (e.rackUnit, fullW * s, thumbBudget))
                    {
                        g.setColour (juce::Colours::black.withAlpha (0.55f * t)); g.fillRoundedRectangle (big.expanded (3.0f).translated (0.0f, 4.0f), 5.0f);
                        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
                        g.setOpacity (1.0f);
                        g.drawImage (*img, big, juce::RectanglePlacement::stretchToFit);
                        g.setColour (amber.withAlpha (0.55f * t)); g.drawRoundedRectangle (big.expanded (1.5f), 3.0f, 1.2f);
                    }
                    else thumbsPending = true;
                }
            }

            // A thin rounded scroll bar, when the list is longer than its area
            const float content = contentHeight() + 6.0f;
            if (content > list.getHeight() + 0.5f && list.getHeight() > 10.0f)
            {
                const float barH = std::max (24.0f, list.getHeight() * list.getHeight() / content);
                const float barY = list.getY() + (list.getHeight() - barH) * (scrollY / std::max (1.0f, content - list.getHeight()));
                g.setColour (white.withAlpha (0.25f));
                g.fillRoundedRectangle (panel.getRight() - 7.0f, barY, 3.0f, barH, 1.5f);
            }

            // --- footer: help for whatever is under the pointer, then RESET --------------------
            const float footY = list.getBottom();
            g.setColour (line);
            g.fillRect (x0 + pad, footY, pw - 2.0f * pad, 1.0f);
            auto area = juce::Rectangle<float> (x0 + pad, footY + 10.0f, pw - 2.0f * pad, helpH - 16.0f + (unit == layout::tunerUnit ? 28.0f : 0.0f));
            auto para = [&] (const juce::String& body, int maxLines, juce::Colour c)
            {
                const int lines = juce::jmin (maxLines, (int) (area.getHeight() / 14.0f));
                if (lines <= 0)
                    return;
                if (holo)
                {
                    const float used = holo::vec::paragraph (g, body, area.withHeight (14.0f * (float) lines), 8.0f, 6.0f, c, 1.0f, body.hashCode() & 0xffff, 0.4f, lines);
                    area.removeFromTop (used + 3.0f);
                    return;
                }
                g.setColour (c);
                g.setFont (font (11.0f, false));
                g.drawFittedText (body, area.withHeight (14.0f * (float) lines).toNearestInt(), juce::Justification::topLeft, lines, 1.0f);
                area.removeFromTop (14.0f * (float) lines + 3.0f);
            };
            auto heading = [&] (const juce::String& t)
            {
                text (t, area.withHeight (16.0f), font (12.0f, true), white);
                area.removeFromTop (18.0f);
            };
            const auto* hoveredEntry = juce::isPositiveAndBelow (hovered.entry, (int) entries.size()) ? &entries[(size_t) hovered.entry] : nullptr;
            if (detailEntry != nullptr && detailEntry->kind == Entry::stage)
            {
                const auto& me = detailEntry->stageInfo->methods[detailChoice];
                heading (methodLabel (me));
                para (str (me.sound), 3, soft);
            }
            else if (detailEntry != nullptr && detailEntry->kind == Entry::modifier)
            {
                const auto& mo = m::modifiers[(size_t) detailEntry->modifierKind];
                heading (plain (str (mo.fullName)) + ": " + plain (str (mo.labels[(size_t) detailChoice])));
                para (str (mo.does), 3, soft);
            }
            else if (lockerNote.isNotEmpty())
                para (lockerNote, unit == layout::tunerUnit ? 6 : 4, white.withAlpha (0.9f));
            else if (hoveredEntry != nullptr && hoveredEntry->kind == Entry::tunerAction && lockerNote.isEmpty())
                para (hoveredEntry->rackUnit == 0 ? "Picks a setting for every unit from its 60,000 to fit your words (a different pick each time: VARIETY). The knobs glide there; LEVEL MATCH keeps it as loud as before."
                    : hoveredEntry->rackUnit == 1 ? "Takes the last tune back: the rack glides to how it was."
                                                  : "Flips between the rack before the last tune and after it, level-matched, to hear what it did.", 4, soft);
            else if (hoveredEntry != nullptr && hoveredEntry->kind == Entry::designAction)
                para (hoveredEntry->rackUnit == 0 ? "Copy a unit's share code (or its link) in the Rack Unit Designer, then click: its panel, knobs and sound load here. Saved with the session."
                                                  : "Unloads the design: the slot passes the sound untouched.", 4, soft);
            else if (hoveredEntry != nullptr && hoveredEntry->kind == Entry::lockerUnit && hoveredEntry->lbModule >= 0)
            {
                const int m = hoveredEntry->lbModule;
                const bool isStored = ((processor.getStoredModules() >> m) & 1u) != 0u;
                static constexpr const char* firstRoles[3] { "The classic British console channel EQ, with its transformers.", "A split-band de-esser for harsh presence.", "Headphone crossfeed: speakers' natural blend, on headphones." };
                heading (juce::String (layout::lb::nameOf (m)));
                const auto described = m >= 4 ? descriptions::forKey (enh::dsp::lbmods::info[m - 4].key) : juce::String();
                para (m < 3 ? juce::String (firstRoles[m]) : described.isNotEmpty() ? described
                      : juce::String (enh::dsp::lbmods::info[m - 4].role).toLowerCase().replaceSection (0, 1, juce::String (enh::dsp::lbmods::info[m - 4].role).substring (0, 1)) + ".", 3, white.withAlpha (0.9f));
                para (isStored ? "Install: it goes into the LUNCHBOX at its place in the signal chain."
                               : "Store: it leaves the frame and stops processing. Its settings are kept.", 2, soft);
            }
            else if (hoveredEntry != nullptr && hoveredEntry->kind == Entry::lockerUnit)
            {
                const int u = hoveredEntry->rackUnit;
                const bool isStored = ((processor.getStoredUnits() >> u) & 1u) != 0u;
                heading (juce::String (layout::unitInfo[(size_t) u].name));
                para (descriptions::forUnit (u), 3, white.withAlpha (0.9f));   // (what it is and does)
                para (isStored ? "Install: it goes into the rack at its place in the signal chain, as you left it."
                               : "Store: it leaves the rack and stops processing (no CPU). Its settings are kept.", 2, soft);
            }
            else if (locker && tab == 2)
                para ("The LUNCHBOX holds " + juce::String (layout::lbSlots) + " slots; its OUTPUT meter always stays in. A module in the locker takes no slot and no CPU. Saved with the session.", 4, soft);
            else if (locker)
                para ("The rack holds " + juce::String (layout::rackCapacityU) + "U. What is in the locker takes no room and no CPU. Type to search; Esc clears it.", 4, soft);
            else if (hoveredEntry != nullptr && hoveredEntry->kind == Entry::reset)
                para ("Puts every setting of this unit back to its default: exactly how it sounded before these settings existed. Knobs keep their positions.", 4, soft);
            else if (hovered.tab >= 0)
                para (categories[hovered.tab] == "KNOBS" ? "Per knob: how its travel maps, and what sits between the knob and the processing."
                      : categories[hovered.tab] == "DISPLAY" ? "How the displays draw. Changes nothing in the sound."
                      : categories[hovered.tab] == "OUTPUT" ? "What leaves the rack."
                      : categories[hovered.tab] == "DESIGN" ? "Load a design from the Rack Unit Designer into this slot."
                                                            : "How the unit measures, calculates and moves.", 4, soft);
            else
                para ("Hover a setting to hear what it does. Choices are saved with the session; presets leave them alone.", 4, dim);

            if (const int r = resetEntry(); r >= 0)
            {
                const auto b = resetBox();
                const float hv = entries[(size_t) r].hover;
                g.setColour (white.withAlpha ((changed > 0 ? 0.08f : 0.035f) + 0.06f * hv));
                g.fillRoundedRectangle (b, 6.0f);
                text ("Reset to defaults", b, font (12.0f, true), white.withAlpha (changed > 0 ? 0.95f : 0.35f), juce::Justification::centred);
            }
        }

        // Straight RGBA (un-premultiplied) for GL blending, straight from the image's pixels
        artwork::RawTexture tex { w, h, 4, {} };
        tex.pixels.resize ((size_t) (w * h * 4));
        const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
        {
            auto* dst = tex.pixels.data() + (size_t) (y * w * 4);
            for (int x = 0; x < w; ++x, dst += 4)
            {
                const auto px = reinterpret_cast<const juce::PixelARGB*> (data.getPixelPointer (x, y))->getUnpremultiplied();
                dst[0] = px.getRed(); dst[1] = px.getGreen(); dst[2] = px.getBlue(); dst[3] = px.getAlpha();
            }
        }
        if (thumbsPending) dirty = true;   // (faceplates still to paint: a couple more next frame)
        return tex;
    }
}
