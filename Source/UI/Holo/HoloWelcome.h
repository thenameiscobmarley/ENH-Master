#pragma once

#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <vector>
#include "VectorBeam.h"

/*  The hologram: ENH Master's welcome screen, and the look the glass panel takes in its HOLOGRAM style -
    the PHOSPHOR scope's green light, projected. Built to be read first and admired second: solid dark
    ground, bright green type at a readable size, no text over the moving trace.

    The card is drawn once into an image (both views show it: the 3D one as a texture with a scanline
    shader over it, the 2D one as is); what moves - the scope trace in its window - is drawn over it each
    frame from lissajous(). */
namespace pad::holo
{
    inline const juce::Colour phosphor    { 0xff7dffa6 };   // the scope's green, a touch brighter to read
    inline const juce::Colour phosphorDim { 0xff3fa86a };
    inline const juce::Colour ground      { 0xf2030b07 };   // near black, a green cast

    /** The welcome card's layout (logical px, about the card's top left) and what was clicked. */
    struct Welcome
    {
        static constexpr float cardW = 640.0f, cardH = 420.0f;
        bool showAtStart = true, holoPanel = false;
        juce::String version;

        juce::Rectangle<float> enter() const { return { cardW - 32.0f - 190.0f, cardH - 32.0f - 44.0f, 190.0f, 44.0f }; }
        juce::Rectangle<float> startBox() const { return { 32.0f, cardH - 70.0f, 200.0f, 20.0f }; }
        juce::Rectangle<float> holoBox() const { return { 32.0f, cardH - 44.0f, 220.0f, 20.0f }; }
        juce::Rectangle<float> scope() const { return { cardW - 32.0f - 190.0f, 104.0f, 190.0f, 150.0f }; }
        /** 1 enter, 2 "show at start", 3 "hologram panel", 0 none (p about the card). */
        int hit (juce::Point<float> p) const
        {
            if (enter().expanded (4.0f).contains (p)) return 1;
            if (startBox().contains (p)) return 2;
            if (holoBox().contains (p)) return 3;
            return 0;
        }

        /** Where the card sits in a view of w x h (centred, never bigger than the view). */
        static juce::Rectangle<float> placeIn (float w, float h)
        {
            const float s = std::min (1.0f, std::min ((w - 32.0f) / cardW, (h - 32.0f) / cardH));
            return juce::Rectangle<float> (cardW * s, cardH * s).withCentre ({ 0.5f * w, 0.5f * h });
        }

        static constexpr int frames = 4;   // jitter frames baked for the 3D view (stacked top to bottom)

        /** The card at `scale` (pixels a logical px), as the beam draws it this frame (`seed`: which jitter):
            ground, the bezel's brackets, every word in the single-stroke vector font, the scope's graticule,
            the switches. Everything is beam: no filled type anywhere. */
        juce::Image render (float scale, int seed = 0) const
        {
            juce::Image img (juce::Image::ARGB, juce::roundToInt (cardW * scale), juce::roundToInt (cardH * scale), true);
            juce::Graphics g (img);
            g.addTransform (juce::AffineTransform::scale (scale));
            const float px = 1.0f, jit = 0.55f;   // (logical px; the image is drawn at `scale`)
            namespace V = vec;
            g.setColour (ground);
            g.fillRoundedRectangle (0.0f, 0.0f, cardW, cardH, 10.0f);

            // The bezel: corner brackets and a faint outline, traced
            {
                juce::Path frame;
                frame.addRoundedRectangle (3.0f, 3.0f, cardW - 6.0f, cardH - 6.0f, 9.0f);
                V::beam (g, frame, phosphor.withMultipliedAlpha (0.45f), px * 0.8f, seed + 1, jit);
                juce::Path br;
                for (int cx = 0; cx < 2; ++cx) for (int cy = 0; cy < 2; ++cy)
                {
                    const float x = cx ? cardW - 14.0f : 14.0f, y = cy ? cardH - 14.0f : 14.0f, dx = cx ? -20.0f : 20.0f, dy = cy ? -20.0f : 20.0f;
                    br.startNewSubPath (x + dx, y); br.lineTo (x, y); br.lineTo (x, y + dy);
                }
                V::beam (g, br, phosphor, px * 1.3f, seed + 2, jit);
            }

            // The name, what version, where to start
            V::text (g, "ENH MASTER", { 32.0f, 28.0f, 360.0f, 40.0f }, 34.0f, phosphor, juce::Justification::centredLeft, px * 1.5f, seed + 3, jit * 1.2f);
            if (version.isNotEmpty())
                V::text (g, "VERSION " + version, { 33.0f, 74.0f, 300.0f, 14.0f }, 11.0f, phosphorDim, juce::Justification::centredLeft, px, seed + 4, jit);
            struct Tip { const char* head; const char* body; };
            const Tip tips[] {
                { "CLICK A UNIT", "ITS SETTINGS OPEN ON THE GLASS PANEL." },
                { "RIGHT-CLICK THE RACK", "SIMPLE OR FULL RACK, AND THE GEAR LOCKER." },
                { "PRESETS", "PREV AND NEXT ON THE ADAPTIVE ENHANCER." },
                { "NEW IN 3.8", "53 NEW UNITS, 35 OF THEM SIMULATIONS." },
            };
            float y = 106.0f;
            int k = 10;
            for (const auto& t : tips)
            {
                juce::Path dot; dot.addEllipse (34.0f, y + 4.0f, 4.0f, 4.0f);
                V::beam (g, dot, phosphor, px, seed + k++, 0.2f);
                V::text (g, t.head, { 48.0f, y, 340.0f, 14.0f }, 13.0f, phosphor, juce::Justification::centredLeft, px * 1.15f, seed + k++, jit);
                V::text (g, t.body, { 48.0f, y + 20.0f, 340.0f, 12.0f }, 10.5f, phosphor.withMultipliedAlpha (0.85f), juce::Justification::centredLeft, px, seed + k++, jit);
                y += 48.0f;
            }

            // The scope's window: its graticule, traced (the moving trace is drawn live over it)
            const auto sc = scope();
            g.setColour (juce::Colour (0xff010603));
            g.fillRoundedRectangle (sc, 6.0f);
            {
                juce::Path grid;
                for (int i = 1; i < 8; ++i) { const float x = sc.getX() + sc.getWidth() * (float) i / 8.0f; grid.startNewSubPath (x, sc.getY() + 4.0f); grid.lineTo (x, sc.getBottom() - 4.0f); }
                for (int i = 1; i < 6; ++i) { const float yy = sc.getY() + sc.getHeight() * (float) i / 6.0f; grid.startNewSubPath (sc.getX() + 4.0f, yy); grid.lineTo (sc.getRight() - 4.0f, yy); }
                V::beam (g, grid, phosphor.withMultipliedAlpha (0.16f), px * 0.7f, seed + 40, 0.3f);
                juce::Path box; box.addRoundedRectangle (sc, 6.0f);
                V::beam (g, box, phosphor.withMultipliedAlpha (0.55f), px, seed + 41, jit);
            }
            V::text (g, "OSCILLOSCOPE", sc.withY (sc.getBottom() + 6.0f).withHeight (11.0f), 9.0f, phosphorDim, juce::Justification::centred, px, seed + 42, jit);

            // The switches, and the way in
            auto check = [&] (juce::Rectangle<float> r, bool on, const char* label, int sd)
            {
                const auto box = r.withWidth (16.0f).reduced (1.0f);
                juce::Path p; p.addRectangle (box);
                if (on) { p.startNewSubPath (box.getX() + 3.0f, box.getCentreY()); p.lineTo (box.getCentreX() - 1.0f, box.getBottom() - 3.0f); p.lineTo (box.getRight() - 3.0f, box.getY() + 3.0f); }
                V::beam (g, p, phosphor, px, seed + sd, jit);
                V::text (g, label, r.withTrimmedLeft (26.0f), 10.5f, phosphor.withMultipliedAlpha (0.9f), juce::Justification::centredLeft, px, seed + sd + 1, jit);
            };
            check (startBox(), showAtStart, "SHOW THIS AT START", 50);
            check (holoBox(), holoPanel, "HOLOGRAM SETTINGS PANEL", 52);
            const auto b = enter();
            g.setColour (phosphor.withAlpha (0.08f)); g.fillRoundedRectangle (b, 8.0f);
            { juce::Path p; p.addRoundedRectangle (b, 8.0f); V::beam (g, p, phosphor, px * 1.3f, seed + 54, jit); }
            V::text (g, "ENTER THE RACK", b.reduced (14.0f, 0.0f), 14.0f, phosphor, juce::Justification::centred, px * 1.3f, seed + 55, jit);
            return img;
        }

        /** The jitter frames stacked top to bottom in one image (the 3D view's texture). */
        juce::Image renderFrames (float scale) const
        {
            const auto one = render (scale, 0);
            juce::Image all (juce::Image::ARGB, one.getWidth(), one.getHeight() * frames, true);
            juce::Graphics g (all);
            g.drawImageAt (one, 0, 0);
            for (int f = 1; f < frames; ++f)
                g.drawImageAt (render (scale, f * 101), 0, f * one.getHeight());
            return all;
        }

        /** The scope trace at time t: points in the scope window, -1 .. 1 each way (a slowly turning 3:2
            Lissajous figure with a little life in it). */
        static void lissajous (double t, std::vector<juce::Point<float>>& pts, int n = 220)
        {
            pts.resize ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const double s = 6.283185307 * (double) i / (double) (n - 1);
                const float x = (float) (0.82 * std::sin (3.0 * s + 0.6 * t) * (0.92 + 0.08 * std::sin (0.9 * t)));
                const float y = (float) (0.78 * std::sin (2.0 * s + 0.35 * t));
                pts[(size_t) i] = { x, y };
            }
        }
    };
}
