#include "GlassPanel.h"
#include "../PluginProcessor.h"
#include "../Parameters/ParameterBridge.h"
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

    GlassPanel::GlassPanel (ParameterBridge& b, PluginProcessor& p) : bridge (b), processor (p) {}

    void GlassPanel::setViewSize (juce::Rectangle<float> v)
    {
        if (v == view)
            return;
        view = v;
        relayout();
    }

    void GlassPanel::open (int newUnit, float y, juce::Rectangle<float> v)
    {
        view = v;
        anchorY = y;
        if (newUnit != unit)
        {
            unit = newUnit;
            expanded = -1;
            hovered = {};
            rows.clear();
            if (unit >= 0)
            {
                const auto list = m::stagesForUnit (unit);
                for (int i = 0; i < list.count; ++i)
                    rows.push_back ({ &list.stages[i], nullptr, -1 });
                // The knobs on this unit that have modifiers
                for (int k = 0; k < (int) m::knobModifiers.size(); ++k)
                {
                    const int p = bridge.indexOf (str (m::knobModifiers[(size_t) k].param));
                    for (int c = 0; c < layout::numControls; ++c)
                        if (layout::controls[(size_t) c].unit == unit && boundParameter (bridge, c) == p)
                        {
                            rows.push_back ({ nullptr, &m::knobModifiers[(size_t) k], k });
                            break;
                        }
                }
            }
        }
        relayout();
    }

    void GlassPanel::relayout()
    {
        layout = {};
        dirty = true;
        if (unit < 0)
            return;

        // Content: every dropdown one under another, the expanded one's choices listed under it
        contentH = 0.0f;
        for (int r = 0; r < (int) rows.size(); ++r)
            contentH += rowH + (r == expanded ? optionH * (float) rows[(size_t) r].numChoices() + 6.0f : 0.0f);

        const float w = std::min (width, view.getWidth() - 2.0f * gutter);
        const float footer = rows.empty() ? bodyH : detailsH;
        const float h = std::min ({ headerH + contentH + footer, maxHeight, view.getHeight() - 2.0f * gutter });
        const float x = view.getRight() - w - gutter;
        const float y = juce::jlimit (view.getY() + gutter, std::max (view.getY() + gutter, view.getBottom() - gutter - h), anchorY - 0.5f * h);
        layout.panel = { x, y, w, h };
        layout.list = { x, y + headerH, w, std::max (0.0f, h - headerH - footer) };
        scrollY = juce::jlimit (0.0f, std::max (0.0f, contentH - layout.list.getHeight()), scrollY);

        float at = layout.list.getY() - scrollY;
        for (int r = 0; r < (int) rows.size(); ++r)
        {
            layout.rows.push_back ({ x + 14.0f, at, w - 28.0f, rowH });
            at += rowH;
            std::vector<juce::Rectangle<float>> opts;
            if (r == expanded)
            {
                for (int k = 0; k < rows[(size_t) r].numChoices(); ++k)
                    opts.push_back ({ x + 14.0f, at + optionH * (float) k, w - 28.0f, optionH });
                at += optionH * (float) rows[(size_t) r].numChoices() + 6.0f;
            }
            layout.options.push_back (std::move (opts));
        }
        layout.details = { x + 14.0f, layout.list.getBottom(), w - 28.0f, footer - 12.0f };
    }

    bool GlassPanel::scroll (float deltaPx)
    {
        const float before = scrollY;
        scrollY -= deltaPx;
        relayout();
        return scrollY != before;
    }

    Hit GlassPanel::hitTest (juce::Point<float> p) const
    {
        Hit hit;
        if (unit < 0 || ! layout.panel.contains (p))
            return hit;
        hit.inside = true;
        if (! layout.list.contains (p))    // only what is scrolled into view can be clicked
            return hit;
        for (int r = 0; r < (int) layout.rows.size(); ++r)
        {
            if (layout.rows[(size_t) r].contains (p))
                hit.row = r;
            for (int k = 0; k < (int) layout.options[(size_t) r].size(); ++k)
                if (layout.options[(size_t) r][(size_t) k].contains (p))
                {
                    hit.row = r;
                    hit.option = k;
                }
        }
        return hit;
    }

    bool GlassPanel::hover (juce::Point<float> p)
    {
        const auto h = hitTest (p);
        if (h.row == hovered.row && h.option == hovered.option && h.inside == hovered.inside)
            return false;
        hovered = h;
        dirty = true;
        return true;
    }

    void GlassPanel::setExpanded (int row, int hoveredOption)
    {
        expanded = juce::isPositiveAndBelow (row, (int) rows.size()) ? row : -1;
        relayout();
        if (expanded >= 0 && hoveredOption >= 0)
            hovered = { expanded, hoveredOption, true };
    }

    void GlassPanel::click (juce::Point<float> p)
    {
        const auto h = hitTest (p);
        if (h.row < 0)
            return;
        if (h.option >= 0)
        {
            choose (rows[(size_t) h.row], h.option);
            expanded = -1;              // a choice closes its dropdown
        }
        else
        {
            const auto& row = rows[(size_t) h.row];
            if (row.numChoices() > 1)
                expanded = expanded == h.row ? -1 : h.row;
        }
        relayout();
        hover (p);
    }

    int GlassPanel::currentChoice (const Row& row) const
    {
        if (row.stage != nullptr)
        {
            if (row.stage->param.empty())
                return 0;
            const int p = bridge.indexOf (str (row.stage->param));
            return p < 0 ? 0 : juce::roundToInt (bridge.getNormalised (p) * (float) (row.stage->numMethods - 1));
        }
        const float value = processor.getKnobInputModifier (row.modifierIndex);
        const auto& c = row.modifier->input->choices;
        int best = 0;
        for (int k = 1; k < (int) c.size(); ++k)
            if (std::abs (c[(size_t) k] - value) < std::abs (c[(size_t) best] - value))
                best = k;
        return best;
    }

    void GlassPanel::choose (const Row& row, int choice)
    {
        if (row.stage != nullptr)
        {
            if (row.stage->param.empty() || row.stage->numMethods < 2)
                return;
            const int p = bridge.indexOf (str (row.stage->param));
            if (p < 0)
                return;
            bridge.beginGesture (p, ControlSource::user);
            bridge.setValueWithSource (p, (float) choice / (float) (row.stage->numMethods - 1), ControlSource::user);
            bridge.endGesture (p);
        }
        else
        {
            processor.setKnobInputModifier (row.modifierIndex, row.modifier->input->choices[(size_t) juce::jlimit (0, 3, choice)]);
        }
        dirty = true;
    }

    void GlassPanel::pollValues()
    {
        std::vector<int> now;
        for (auto& r : rows)
            now.push_back (currentChoice (r));
        if (now != shownChoices)
        {
            shownChoices = std::move (now);
            dirty = true;
        }
    }

    //==============================================================================
    artwork::RawTexture GlassPanel::render (float pixelScale)
    {
        dirty = false;
        if (unit < 0 || layout.panel.isEmpty())
            return {};

        const float s = juce::jlimit (1.0f, 3.0f, pixelScale);
        const auto panel = layout.panel;
        const int w = juce::roundToInt (panel.getWidth() * s), h = juce::roundToInt (panel.getHeight() * s);
        juce::Image img (juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::translation (-panel.getX(), -panel.getY()).scaled (s));

            // White on frosted glass: full white for what matters, softer white for the rest, hairlines
            const auto white = juce::Colours::white, soft = juce::Colours::white.withAlpha (0.66f),
                       faint = juce::Colours::white.withAlpha (0.18f);
            const float x0 = panel.getX(), y0 = panel.getY(), pw = panel.getWidth();

            // A soft dark halo behind every letter keeps white text readable over bright parts of the rack
            auto text = [&] (const juce::String& str_, juce::Rectangle<float> r, const juce::Font& f, juce::Colour c,
                             juce::Justification j = juce::Justification::left)
            {
                g.setFont (f);
                g.setColour (juce::Colours::black.withAlpha (0.22f));
                g.drawText (str_, r.translated (0.0f, 1.0f), j, true);
                g.setColour (c);
                g.drawText (str_, r, j, true);
            };

            // Header: the unit's name, what it is
            const auto& info = layout::unitInfo[(size_t) unit];
            text (juce::String (info.name), { x0 + 14.0f, y0 + 14.0f, pw - 28.0f, 20.0f }, font (15.5f, true, 0.02f), white);
            text (juce::String (info.role), { x0 + 14.0f, y0 + 35.0f, pw - 28.0f, 13.0f }, font (9.5f, false, 0.04f), soft);
            g.setColour (faint);
            g.fillRect (x0 + 14.0f, y0 + headerH - 1.0f, pw - 28.0f, 1.0f);

            if (rows.empty())
            {
                g.setColour (soft);
                g.setFont (font (11.0f, false, 0.0f));
                g.drawFittedText ("Its processing is not split into swappable stages yet: the ADAPTIVE COMPRESSOR is the pilot. "
                                  "Scroll the rack to walk up to this unit.",
                                  juce::Rectangle<int> ((int) x0 + 14, (int) (y0 + headerH + 10.0f), (int) pw - 28, (int) bodyH - 20),
                                  juce::Justification::topLeft, 5, 1.0f);
            }

            // The list, one dropdown under another, clipped to its scrolling area
            const m::Method* detail = nullptr;
            const m::Modifier* detailModifier = nullptr;
            const m::Stage* detailStage = nullptr;
            float detailValue = 0.0f;
            {
                juce::Graphics::ScopedSaveState clip (g);
                g.reduceClipRegion (layout.list.toNearestInt());
                for (int r = 0; r < (int) rows.size(); ++r)
                {
                    const auto& row = rows[(size_t) r];
                    const auto box = layout.rows[(size_t) r];
                    const int current = currentChoice (row);
                    const bool hot = hovered.row == r && hovered.option < 0;
                    const bool isOpen = expanded == r;

                    juce::String title, value;
                    if (row.stage != nullptr)
                    {
                        title = str (row.stage->name);
                        value = str (row.stage->methods[current].shortName) + "   " + str (row.stage->methods[current].fullName);
                    }
                    else
                    {
                        const auto* spec = pad::params::findSpec (str (row.modifier->param));
                        title = (spec != nullptr ? spec->shortLabel : str (row.modifier->param)) + " " + str (row.modifier->input->fullName).toUpperCase();
                        const float v = row.modifier->input->choices[(size_t) current];
                        value = v == 0.0f ? juce::String ("OFF") : juce::String (v, 0) + " " + str (row.modifier->input->unit);
                    }

                    if (r > 0)
                    {
                        g.setColour (faint);
                        g.fillRect (box.getX(), box.getY(), box.getWidth(), 1.0f);
                    }
                    if (hot && row.numChoices() > 1)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.08f));
                        g.fillRect (box.expanded (6.0f, 0.0f).withTrimmedTop (1.0f));
                    }
                    // Title over its value: stacked, never side by side
                    text (title, { box.getX(), box.getY() + 8.0f, box.getWidth(), 13.0f }, font (9.5f, true, 0.14f), soft);
                    text (value + (row.numChoices() > 1 ? (isOpen ? juce::String (juce::CharPointer_UTF8 ("  \xe2\x88\x92")) : juce::String ("  +")) : juce::String()),
                          { box.getX(), box.getY() + 23.0f, box.getWidth(), 17.0f }, font (12.5f, true, 0.02f), white);

                    if (isOpen)
                        for (int k = 0; k < row.numChoices(); ++k)
                        {
                            const auto o = layout.options[(size_t) r][(size_t) k];
                            const bool on = k == current, oh = hovered.row == r && hovered.option == k;
                            if (oh)
                            {
                                g.setColour (juce::Colours::white.withAlpha (0.10f));
                                g.fillRect (o.expanded (6.0f, 0.0f));
                            }
                            // A hard square: filled for the one in use
                            const juce::Rectangle<float> mark (o.getX() + 2.0f, o.getCentreY() - 4.0f, 8.0f, 8.0f);
                            g.setColour (white.withAlpha (on ? 1.0f : 0.55f));
                            if (on) g.fillRect (mark);
                            else    g.drawRect (mark, 1.0f);
                            juce::String label;
                            if (row.stage != nullptr)
                                label = str (row.stage->methods[k].shortName) + "   " + str (row.stage->methods[k].fullName);
                            else
                            {
                                const float v = row.modifier->input->choices[(size_t) k];
                                label = v == 0.0f ? juce::String ("OFF") : juce::String (v, 0) + " " + str (row.modifier->input->unit);
                            }
                            text (label, o.withTrimmedLeft (18.0f), font (11.5f, on, 0.02f), on || oh ? white : soft);
                        }

                    // What the details area explains: the hovered choice, else the hovered row's current one
                    if (hovered.row == r)
                    {
                        const int k = hovered.option >= 0 ? hovered.option : current;
                        if (row.stage != nullptr)
                        {
                            detail = &row.stage->methods[k];
                            detailStage = row.stage;
                        }
                        else
                        {
                            detailModifier = row.modifier->input;
                            detailValue = row.modifier->input->choices[(size_t) k];
                        }
                    }
                }
            }

            // A scroll bar when the list is longer than its area: a hard white sliver
            if (contentH > layout.list.getHeight() + 0.5f && layout.list.getHeight() > 10.0f)
            {
                const float frac = layout.list.getHeight() / contentH;
                const float barH = std::max (18.0f, layout.list.getHeight() * frac);
                const float barY = layout.list.getY() + (layout.list.getHeight() - barH) * (scrollY / std::max (1.0f, contentH - layout.list.getHeight()));
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.fillRect (panel.getRight() - 5.0f, barY, 2.0f, barH);
            }

            // Details: the registry's text for whatever is under the pointer
            if (! rows.empty() && layout.details.getHeight() > 20.0f)
            {
                const auto d = layout.details;
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
                auto heading = [&] (const juce::String& h_)
                {
                    text (h_, line.withHeight (15.0f), font (11.5f, true, 0.02f), white);
                    line.removeFromTop (17.0f);
                };
                if (detail != nullptr)
                {
                    heading (str (detail->shortName) + "   " + str (detail->fullName));
                    para (str (detail->sound), 4, white.withAlpha (0.9f));
                    para (str (detail->cost), 2, soft);
                    juce::ignoreUnused (detailStage);
                }
                else if (detailModifier != nullptr)
                {
                    heading (str (detailModifier->fullName) + (detailValue > 0.0f ? "   " + juce::String (detailValue, 0) + " " + str (detailModifier->unit) : juce::String ("   OFF")));
                    para (str (detailModifier->does), 3, white.withAlpha (0.9f));
                    para (str (detailModifier->cost), 2, soft);
                }
                else
                    para ("Hover a setting to see how it changes the sound. The host still sees every knob's raw value; "
                          "these choices are saved with the session.", 6, soft);
            }
        }

        // Straight RGBA (un-premultiplied) for GL blending
        artwork::RawTexture tex { w, h, 4, {} };
        tex.pixels.assign ((size_t) (w * h * 4), 0);
        const juce::Image::BitmapData data (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const auto c = data.getPixelColour (x, y);
                auto* dst = tex.pixels.data() + (size_t) ((y * w + x) * 4);
                dst[0] = c.getRed(); dst[1] = c.getGreen(); dst[2] = c.getBlue(); dst[3] = c.getAlpha();
            }
        return tex;
    }
}
