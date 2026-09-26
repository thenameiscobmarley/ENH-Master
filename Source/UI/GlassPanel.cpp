#include "GlassPanel.h"
#include "../PluginProcessor.h"
#include "../Parameters/ParameterBridge.h"
#include "../Parameters/KnobModifiers.h"
#include "Controls/ControlBinding.h"

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

    /** How a method is printed: its code and name, or only the name when the code says nothing more
        ("0.3" beside "-0.3 dBFS", "STD" beside "1.5 dB") */
    static juce::String methodLabel (const m::Method& me)
    {
        const auto code = str (me.shortName), name = str (me.fullName);
        const bool numeric = code.containsOnly ("0123456789.");
        if (numeric || code == "STD" || name.containsIgnoreCase (code))
            return name;
        return code + "   " + name;
    }

    static float ease (float current, float target, float rate, float dt) noexcept
    {
        const float next = target + (current - target) * std::exp (-rate * dt);
        return std::abs (next - target) < 0.002f ? target : next;
    }

    int Entry::numChoices() const noexcept
    {
        if (kind == stage)
            return stageInfo->numMethods;
        if (kind == modifier)
            return (int) m::modifiers[(size_t) modifierKind].choices.size();
        return 0;
    }

    GlassPanel::GlassPanel (ParameterBridge& b, PluginProcessor& p) : bridge (b), processor (p) {}

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
            entries.clear();
            categories.clear();
            if (unit >= 0)
            {
                const auto list = m::stagesForUnit (unit);
                auto categoryOf = [this] (const juce::String& name)
                {
                    if (! categories.contains (name))
                    {
                        categories.add (name);
                        Entry e;
                        e.kind = Entry::category;
                        e.title = name;
                        e.categoryIndex = categories.size() - 1;
                        e.open = e.openTarget = name == "KNOBS" ? 0.0f : 1.0f;   // the long knob list starts folded
                        entries.push_back (e);
                    }
                    return categories.indexOf (name);
                };

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

                // KNOBS: every continuous knob on this unit, its law (if it has one), then its modifiers
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
                for (int k : knobsHere)
                {
                    const juce::String pid (enh::dsp::knobFields[(size_t) k].param);
                    const auto* spec = pad::params::findSpec (pid);
                    Entry h;
                    h.kind = Entry::knobHeader;
                    h.title = spec != nullptr ? spec->shortLabel.toUpperCase() : pid;
                    h.categoryIndex = categoryOf ("KNOBS");
                    h.open = h.openTarget = 0.0f;   // each knob is its own dropdown, folded
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
        }
        layoutEntries();
    }

    //==============================================================================
    float GlassPanel::contentHeight() const noexcept
    {
        return entries.empty() ? 0.0f : entries.back().y + entries.back().h;
    }

    /** Every entry's height from its animation state, stacked; the panel sized to fit (up to its maximum). */
    void GlassPanel::layoutEntries()
    {
        dirty = true;
        if (unit < 0)
        {
            panel = {};
            return;
        }
        auto categoryOpen = [this] (int index)
        {
            for (auto& e : entries)
                if (e.kind == Entry::category && e.categoryIndex == index)
                    return e.open;
            return 1.0f;
        };
        float y = 0.0f;
        for (auto& e : entries)
        {
            float fold = e.kind == Entry::category || e.kind == Entry::reset ? 1.0f : categoryOpen (e.categoryIndex);
            if (e.knobGroup >= 0)
                fold *= entries[(size_t) e.knobGroup].open;   // inside a knob's own dropdown
            switch (e.kind)
            {
                case Entry::category:   e.h = categoryH; break;
                case Entry::knobHeader: e.h = knobH * fold; break;
                case Entry::reset:      e.h = resetH; break;
                default:                e.h = (rowH + (optionH * (float) e.numChoices() + 6.0f) * e.open) * fold; break;
            }
            e.y = y;
            y += e.h;
        }

        const float w = std::min (width, view.getWidth() - 2.0f * gutter);
        const float h = std::min ({ headerH + y + detailsH, maxHeight, view.getHeight() - 2.0f * gutter });
        const float x = view.getRight() - w - gutter;
        const float top = juce::jlimit (view.getY() + gutter, std::max (view.getY() + gutter, view.getBottom() - gutter - h), anchorY - 0.5f * h);
        panel = { x, top, w, h };

        const float maxScroll = std::max (0.0f, y - listArea().getHeight());
        scrollTarget = juce::jlimit (0.0f, maxScroll, scrollTarget);
        scrollY = juce::jlimit (0.0f, maxScroll, scrollY);
    }

    juce::Rectangle<float> GlassPanel::listArea() const noexcept
    {
        return { panel.getX(), panel.getY() + headerH, panel.getWidth(), std::max (0.0f, panel.getHeight() - headerH - detailsH) };
    }

    juce::Rectangle<float> GlassPanel::entryBox (const Entry& e) const noexcept
    {
        const auto list = listArea();
        return { list.getX() + 14.0f, list.getY() + e.y - scrollY, list.getWidth() - 28.0f, e.h };
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
        const auto list = listArea();
        if (! list.contains (p))
            return hit;
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            const auto& e = entries[(size_t) i];
            const auto box = entryBox (e);
            if (e.h < 6.0f || ! box.contains (p))
                continue;
            hit.entry = i;
            if ((e.kind == Entry::stage || e.kind == Entry::modifier) && e.open > 0.9f)
                for (int k = 0; k < e.numChoices(); ++k)
                    if (optionBox (e, k).contains (p))
                        hit.option = k;
        }
        return hit;
    }

    bool GlassPanel::hover (juce::Point<float> p)
    {
        const auto h = hitTest (p);
        if (h.entry == hovered.entry && h.option == hovered.option && h.inside == hovered.inside)
            return false;
        hovered = h;
        dirty = true;
        return true;
    }

    void GlassPanel::unhover()
    {
        if (hovered.entry >= 0 || hovered.inside)
        {
            hovered = {};
            dirty = true;
        }
    }

    void GlassPanel::click (juce::Point<float> p)
    {
        const auto h = hitTest (p);
        if (h.entry < 0)
            return;
        auto& e = entries[(size_t) h.entry];
        switch (e.kind)
        {
            case Entry::category:
                e.openTarget = e.openTarget > 0.5f ? 0.0f : 1.0f;
                break;
            case Entry::reset:
                resetUnit();
                break;
            case Entry::knobHeader:
                e.openTarget = e.openTarget > 0.5f ? 0.0f : 1.0f;
                break;
            default:
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
        }
        dirty = true;
    }

    bool GlassPanel::scroll (float deltaPx)
    {
        const float before = scrollTarget;
        const float maxScroll = std::max (0.0f, contentHeight() - listArea().getHeight());
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
            e.open = ease (e.open, e.openTarget, 16.0f, dt);
            e.hover = ease (e.hover, hovered.entry == i && hovered.option < 0 ? 1.0f : 0.0f, 20.0f, dt);
            moving = moving || e.open != o || e.hover != h;
        }
        const float s = scrollY;
        scrollY = ease (scrollY, scrollTarget, 18.0f, dt);
        moving = moving || scrollY != s;
        if (moving)
            layoutEntries();
        return moving;
    }

    void GlassPanel::setExpanded (int setting, int hoveredOption, bool unfoldAll)
    {
        int n = 0;
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            auto& e = entries[(size_t) i];
            if (e.kind == Entry::category && unfoldAll)
                e.open = e.openTarget = 1.0f;
            if (e.kind == Entry::stage || e.kind == Entry::modifier)
            {
                const bool it = n++ == setting;
                e.open = e.openTarget = it ? 1.0f : 0.0f;
                if (it && e.knobGroup >= 0)   // the knob it belongs to opens too; the others stay folded
                    entries[(size_t) e.knobGroup].open = entries[(size_t) e.knobGroup].openTarget = 1.0f;
                if (it && hoveredOption >= 0)
                    hovered = { i, hoveredOption, true };
            }
        }
        layoutEntries();
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
        return str (m::modifiers[(size_t) e.modifierKind].labels[(size_t) k]);
    }

    juce::String GlassPanel::valueText (const Entry& e, int k) const
    {
        return choiceText (e, k);
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

            // White on frosted glass: full white for what matters, softer for the rest, hairlines
            const auto white = juce::Colours::white, soft = white.withAlpha (0.64f), faint = white.withAlpha (0.16f);
            const float x0 = panel.getX(), y0 = panel.getY(), pw = panel.getWidth();

            // A soft dark halo behind the letters keeps white text readable over bright parts of the rack
            auto text = [&] (const juce::String& t, juce::Rectangle<float> r, const juce::Font& f, juce::Colour c,
                             juce::Justification j = juce::Justification::left)
            {
                g.setFont (f);
                g.setColour (juce::Colours::black.withAlpha (0.22f * c.getFloatAlpha()));
                g.drawText (t, r.translated (0.0f, 1.0f), j, true);
                g.setColour (c);
                g.drawText (t, r, j, true);
            };

            // --- header -------------------------------------------------------------------
            int settings = 0, changed = 0;
            for (auto& e : entries)
                if (e.kind == Entry::stage || e.kind == Entry::modifier)
                {
                    ++settings;
                    changed += currentChoice (e) != 0 ? 1 : 0;
                }
            const auto& info = layout::unitInfo[(size_t) unit];
            text (juce::String (info.name), { x0 + 14.0f, y0 + 13.0f, pw - 28.0f, 20.0f }, font (15.5f, true, 0.02f), white);
            // What this unit delays the sound by (look-ahead, oversampling): gamers want to know
            juce::String delay;
            {
                // EnhEngine::getLatencyBreakdown order: enhancer, radar, tone & space, character, ear guard, limiter
                const int stage = unit == layout::enhUnit ? 0 : unit == layout::radarUnit ? 1 : unit == layout::tubeUnit ? 2
                                : unit == layout::characterUnit ? 3 : -1;
                if (unit == layout::monitorUnit)
                    delay = "RACK DELAY " + juce::String (processor.getRackLatencyMs(), 1) + " MS";   // the whole rack, as it leaves
                else if (stage >= 0)
                    delay = "DELAY " + juce::String (processor.getStageLatencyMs (stage), 1) + " MS";
                else
                    delay = "NO DELAY";
            }
            text (juce::String (settings) + " SETTINGS   /   " + delay + (changed > 0 ? "   /   " + juce::String (changed) + " CHANGED" : juce::String()),
                  { x0 + 14.0f, y0 + 35.0f, pw - 28.0f, 13.0f }, font (9.0f, true, 0.16f), soft);
            g.setColour (faint);
            g.fillRect (x0 + 14.0f, y0 + headerH - 1.0f, pw - 28.0f, 1.0f);

            // --- the list ----------------------------------------------------------------
            const auto list = listArea();
            const Entry* detailEntry = nullptr;
            int detailChoice = 0;
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
                    g.reduceClipRegion (box.expanded (8.0f, 0.0f).toNearestInt());

                    if (e.kind == Entry::category)
                    {
                        int catChanged = 0;
                        for (auto& o : entries)
                            if (o.categoryIndex == e.categoryIndex && (o.kind == Entry::stage || o.kind == Entry::modifier))
                                catChanged += currentChoice (o) != 0 ? 1 : 0;
                        if (e.hover > 0.01f)
                        {
                            g.setColour (white.withAlpha (0.07f * e.hover));
                            g.fillRect (box.expanded (8.0f, 0.0f));
                        }
                        // A hard triangle that turns as the category folds out
                        juce::Path tri;
                        tri.addTriangle (-3.0f, -4.0f, -3.0f, 4.0f, 4.0f, 0.0f);
                        tri.applyTransform (juce::AffineTransform::rotation (e.open * juce::MathConstants<float>::halfPi)
                                                .translated (box.getX() + 4.0f, box.getCentreY()));
                        g.setColour (white);
                        g.fillPath (tri);
                        text (e.title + (catChanged > 0 ? "   " + juce::String (catChanged) + " CHANGED" : juce::String()),
                              box.withTrimmedLeft (16.0f), font (10.0f, true, 0.18f), white);
                        g.setColour (faint);
                        g.fillRect (box.getX(), box.getBottom() - 1.0f, box.getWidth(), 1.0f);
                    }
                    else if (e.kind == Entry::knobHeader)
                    {
                        // A knob's own dropdown: its name, and under it what is set (or that it is as it always was)
                        juce::StringArray set;
                        for (auto& o : entries)
                            if (o.knobGroup == i && (o.kind == Entry::stage || o.kind == Entry::modifier))
                                if (const int c = currentChoice (o); c != 0)
                                    set.add ((o.kind == Entry::stage ? str (o.stageInfo->name) : str (m::modifiers[(size_t) o.modifierKind].fullName)).toUpperCase()
                                             + " " + (o.kind == Entry::stage ? str (o.stageInfo->methods[c].shortName) : str (m::modifiers[(size_t) o.modifierKind].labels[(size_t) c])));
                        if (e.hover > 0.01f)
                        {
                            g.setColour (white.withAlpha (0.07f * e.hover));
                            g.fillRect (box.withHeight (knobH).expanded (8.0f, 0.0f));
                        }
                        juce::Path tri;
                        tri.addTriangle (-2.5f, -3.5f, -2.5f, 3.5f, 3.5f, 0.0f);
                        tri.applyTransform (juce::AffineTransform::rotation (e.open * juce::MathConstants<float>::halfPi)
                                                .translated (box.getX() + 4.0f, box.getY() + 14.5f));
                        g.setColour (white.withAlpha (0.8f));
                        g.fillPath (tri);
                        text (e.title, { box.getX() + 14.0f, box.getY() + 8.0f, box.getWidth() - 14.0f, 13.0f }, font (10.5f, true, 0.12f), white);
                        text (set.isEmpty() ? juce::String ("as it is") : set.joinIntoString ("  /  "),
                              { box.getX() + 14.0f, box.getY() + 22.0f, box.getWidth() - 14.0f, 12.0f }, font (9.5f, false, 0.02f),
                              set.isEmpty() ? soft.withMultipliedAlpha (0.8f) : white.withAlpha (0.9f));
                        g.setColour (faint);
                        g.fillRect (box.getX(), box.getY() + knobH - 1.0f, box.getWidth(), 1.0f);
                    }
                    else if (e.kind == Entry::reset)
                    {
                        const auto button = box.reduced (0.0f, 9.0f);
                        g.setColour (white.withAlpha (0.06f + 0.10f * e.hover));
                        g.fillRect (button);
                        g.setColour (white.withAlpha (changed > 0 ? 0.70f : 0.30f));
                        g.drawRect (button, 1.0f);
                        text ("RESET TO DEFAULTS", button, font (10.0f, true, 0.16f), white.withAlpha (changed > 0 ? 1.0f : 0.45f),
                              juce::Justification::centred);
                    }
                    else
                    {
                        const int current = currentChoice (e);
                        const bool isChanged = current != 0;
                        if (e.hover > 0.01f && e.numChoices() > 1)
                        {
                            g.setColour (white.withAlpha (0.07f * e.hover));
                            g.fillRect (box.withHeight (rowH).expanded (8.0f, 0.0f));
                        }
                        const juce::String title = e.kind == Entry::stage ? str (e.stageInfo->name)
                                                                          : str (m::modifiers[(size_t) e.modifierKind].fullName).toUpperCase();
                        // Title over its value, stacked; a hard square marks a setting that is not at its default
                        const float indent = e.knobGroup >= 0 ? 14.0f : 0.0f;   // a knob's settings sit under its name
                        text (title, { box.getX() + indent, box.getY() + 7.0f, box.getWidth() - indent, 13.0f }, font (9.5f, true, 0.14f), soft);
                        float vx = box.getX() + indent;
                        if (isChanged)
                        {
                            g.setColour (white);
                            g.fillRect (vx, box.getY() + 26.0f, 5.0f, 5.0f);
                            vx += 11.0f;
                        }
                        text (valueText (e, current), { vx, box.getY() + 20.0f, box.getRight() - vx, 17.0f }, font (12.5f, true, 0.02f), white);

                        // Its choices, fading in as the list opens
                        if (e.open > 0.01f)
                            for (int k = 0; k < e.numChoices(); ++k)
                            {
                                const auto o = optionBox (e, k);
                                const bool on = k == current, oh = hovered.entry == i && hovered.option == k;
                                const float a = e.open;
                                if (oh)
                                {
                                    g.setColour (white.withAlpha (0.10f * a));
                                    g.fillRect (o.expanded (8.0f, 0.0f));
                                }
                                const juce::Rectangle<float> mark (o.getX() + 2.0f + indent, o.getCentreY() - 4.0f, 8.0f, 8.0f);
                                g.setColour (white.withAlpha ((on ? 1.0f : 0.55f) * a));
                                if (on) g.fillRect (mark);
                                else    g.drawRect (mark, 1.0f);
                                text (choiceText (e, k) + (k == 0 ? "   (default)" : ""), o.withTrimmedLeft (18.0f + indent),
                                      font (11.5f, on, 0.02f), (on || oh ? white : soft).withMultipliedAlpha (a));
                            }

                        if (hovered.entry == i)
                        {
                            detailEntry = &e;
                            detailChoice = hovered.option >= 0 ? hovered.option : current;
                        }
                        g.setColour (faint);
                        g.fillRect (box.getX(), box.getBottom() - 1.0f, box.getWidth(), 1.0f);
                    }
                }
            }

            // A hard white sliver of a scroll bar, when the list is longer than its area
            const float content = contentHeight();
            if (content > list.getHeight() + 0.5f && list.getHeight() > 10.0f)
            {
                const float barH = std::max (18.0f, list.getHeight() * list.getHeight() / content);
                const float barY = list.getY() + (list.getHeight() - barH) * (scrollY / std::max (1.0f, content - list.getHeight()));
                g.setColour (white.withAlpha (0.45f));
                g.fillRect (panel.getRight() - 5.0f, barY, 2.0f, barH);
            }

            // --- details: the registry's text for whatever is under the pointer ---------------
            const juce::Rectangle<float> d (x0 + 14.0f, list.getBottom(), pw - 28.0f, detailsH - 12.0f);
            g.setColour (faint);
            g.fillRect (d.getX(), d.getY(), d.getWidth(), 1.0f);
            auto line = d.withTrimmedTop (9.0f);
            auto para = [&] (const juce::String& body, int maxLines, juce::Colour c)
            {
                const int lines = juce::jmin (maxLines, (int) (line.getHeight() / 12.0f));
                if (lines <= 0)
                    return;
                g.setColour (c);
                g.setFont (font (10.0f, false, 0.0f));
                g.drawFittedText (body, line.withHeight (12.0f * (float) lines).toNearestInt(), juce::Justification::topLeft, lines, 1.0f);
                line.removeFromTop (12.0f * (float) lines + 3.0f);
            };
            auto heading = [&] (const juce::String& t)
            {
                text (t, line.withHeight (15.0f), font (11.5f, true, 0.02f), white);
                line.removeFromTop (17.0f);
            };
            const auto* hoveredEntry = juce::isPositiveAndBelow (hovered.entry, (int) entries.size()) ? &entries[(size_t) hovered.entry] : nullptr;
            if (detailEntry != nullptr && detailEntry->kind == Entry::stage)
            {
                const auto& me = detailEntry->stageInfo->methods[detailChoice];
                heading (methodLabel (me));
                para (str (me.sound), 6, white.withAlpha (0.9f));
                para (str (me.cost), 2, soft);
            }
            else if (detailEntry != nullptr && detailEntry->kind == Entry::modifier)
            {
                const auto& mo = m::modifiers[(size_t) detailEntry->modifierKind];
                heading (str (mo.fullName) + "   " + str (mo.labels[(size_t) detailChoice]));
                para (str (mo.does), 4, white.withAlpha (0.9f));
                para (str (mo.cost), 2, soft);
            }
            else if (hoveredEntry != nullptr && hoveredEntry->kind == Entry::reset)
                para ("Puts every setting of this unit back to its default: exactly how it sounded before these settings "
                      "existed. Knobs keep their positions.", 6, soft);
            else if (hoveredEntry != nullptr && hoveredEntry->kind == Entry::knobHeader)
                para ("This knob's own settings: how its travel maps (where it has a law), and SMOOTHING, CURVE and "
                      "RANGE between the knob and the processing. Click to open or close it.", 6, soft);
            else if (hoveredEntry != nullptr && hoveredEntry->kind == Entry::category)
                para (hoveredEntry->title == "KNOBS" ? "Per knob: how its travel maps, and what sits between the knob and the "
                                                       "processing. The host always sees the knob's raw value."
                      : hoveredEntry->title == "DISPLAY" ? "How the displays draw. Changes nothing in the sound."
                      : hoveredEntry->title == "OUTPUT" ? "What leaves the rack."
                                                        : "How the unit measures, calculates and moves.", 6, soft);
            else
                para ("Hover a setting to see how it changes the sound. Choices are saved with the session; presets "
                      "leave them alone.", 6, soft);
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
        return tex;
    }
}
