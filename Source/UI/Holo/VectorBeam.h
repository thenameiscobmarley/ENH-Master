#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>
#include <cmath>
#include <unordered_map>

/*  Text and lines as an oscilloscope draws them: one beam, moving. A single-stroke vector font (the kind
    vector displays and scope readouts used - capitals, figures, a little punctuation, each letter a few
    strokes on a 4 x 6 grid) and a beam that wanders a little off every stroke and glows round it, so the
    hologram reads as drawn by a CRT's electron beam, not typeset.

      vectorPath (text, height)  the strokes, left to right from (0, 0) (y down, the cap height `height`)
      width (text, height)       how wide that is
      beam (g, path, colour, px, seed, jitter)  draws it: a wide faint halo, a softer glow, the bright core;
                                 every point nudged by a hash of its place and `seed` (change the seed and it
                                 jiggles), by up to `jitter` pixels. */
namespace pad::holo::vec
{
    namespace detail
    {
        /** Each glyph: polylines on a 4 x 6 grid (x right, y up from the baseline), '|' lifts the pen. */
        inline const char* strokes (juce::juce_wchar c) noexcept
        {
            switch (c)
            {
                case 'A': return "0,0 0,4 2,6 4,4 4,0|0,3 4,3";
                case 'B': return "0,0 0,6 3,6 4,5 4,4 3,3 0,3|3,3 4,2 4,1 3,0 0,0";
                case 'C': return "4,6 1,6 0,5 0,1 1,0 4,0";
                case 'D': return "0,0 0,6 3,6 4,5 4,1 3,0 0,0";
                case 'E': return "4,6 0,6 0,0 4,0|0,3 3,3";
                case 'F': return "4,6 0,6 0,0|0,3 3,3";
                case 'G': return "4,5 3,6 1,6 0,5 0,1 1,0 3,0 4,1 4,3 2,3";
                case 'H': return "0,0 0,6|4,0 4,6|0,3 4,3";
                case 'I': return "1,6 3,6|2,6 2,0|1,0 3,0";
                case 'J': return "4,6 4,1 3,0 1,0 0,1";
                case 'K': return "0,0 0,6|4,6 0,2|1,3 4,0";
                case 'L': return "0,6 0,0 4,0";
                case 'M': return "0,0 0,6 2,3 4,6 4,0";
                case 'N': return "0,0 0,6 4,0 4,6";
                case 'O': return "1,0 0,1 0,5 1,6 3,6 4,5 4,1 3,0 1,0";
                case 'P': return "0,0 0,6 3,6 4,5 4,4 3,3 0,3";
                case 'Q': return "1,0 0,1 0,5 1,6 3,6 4,5 4,1 3,0 1,0|2,2 4,0";
                case 'R': return "0,0 0,6 3,6 4,5 4,4 3,3 0,3|2,3 4,0";
                case 'S': return "4,5 3,6 1,6 0,5 0,4 1,3 3,3 4,2 4,1 3,0 1,0 0,1";
                case 'T': return "0,6 4,6|2,6 2,0";
                case 'U': return "0,6 0,1 1,0 3,0 4,1 4,6";
                case 'V': return "0,6 2,0 4,6";
                case 'W': return "0,6 1,0 2,3 3,0 4,6";
                case 'X': return "0,0 4,6|0,6 4,0";
                case 'Y': return "0,6 2,3 4,6|2,3 2,0";
                case 'Z': return "0,6 4,6 0,0 4,0";
                case '0': return "1,0 0,1 0,5 1,6 3,6 4,5 4,1 3,0 1,0|0,1 4,5";
                case '1': return "1,5 2,6 2,0|1,0 3,0";
                case '2': return "0,5 1,6 3,6 4,5 4,4 0,0 4,0";
                case '3': return "0,5 1,6 3,6 4,5 4,4 3,3 1,3|3,3 4,2 4,1 3,0 1,0 0,1";
                case '4': return "3,0 3,6 0,2 4,2";
                case '5': return "4,6 0,6 0,3 3,3 4,2 4,1 3,0 0,0";
                case '6': return "4,5 3,6 1,6 0,5 0,1 1,0 3,0 4,1 4,2 3,3 0,3";
                case '7': return "0,6 4,6 1,0";
                case '8': return "1,3 0,4 0,5 1,6 3,6 4,5 4,4 3,3 1,3 0,2 0,1 1,0 3,0 4,1 4,2 3,3";
                case '9': return "0,1 1,0 3,0 4,1 4,5 3,6 1,6 0,5 0,4 1,3 4,3";
                case '.': return "1.7,0 2.3,0.5";
                case ',': return "2.2,0.6 1.4,-0.9";
                case ':': return "1.7,1 2.3,1.5|1.7,4 2.3,4.5";
                case ';': return "1.7,4 2.3,4.5|2.2,1 1.4,-0.6";
                case '-': return "1,3 3,3";
                case '+': return "1,3 3,3|2,2 2,4";
                case '/': return "0,0 4,6";
                case '(': return "3,6 2,5 2,1 3,0";
                case ')': return "1,6 2,5 2,1 1,0";
                case '!': return "2,6 2,2|1.7,0 2.3,0.5";
                case '?': return "0,5 1,6 3,6 4,5 4,4 2,3 2,2|1.7,0 2.3,0.5";
                case '&': return "4,0 1,4 1,5 2,6 3,5 0,2 0,1 1,0 2,0 4,3";
                case '%': return "0,0 4,6|0,5 0,6 1,6 1,5 0,5|3,0 3,1 4,1 4,0 3,0";
                case '\'': return "2,6 2,4.8";
                case '"': return "1.5,6 1.5,5|2.5,6 2.5,5";
                case '=': return "1,2 3,2|1,4 3,4";
                case '<': return "4,5 0,3 4,1";
                case '>': return "0,5 4,3 0,1";
                case '_': return "0,0 4,0";
                case '*': return "2,1 2,5|0.5,2 3.5,4|0.5,4 3.5,2";
                case '#': return "1,0 1,6|3,0 3,6|0,2 4,2|0,4 4,4";
                case 0x00b7: return "1.7,3 2.3,3.4";   // a middle dot
                default: return "";
            }
        }
    }

    inline constexpr float advance = 6.0f, capGrid = 6.0f;   // grid units a letter takes, and its height

    inline float width (const juce::String& text, float height) noexcept
    {
        return std::max (0.0f, (float) text.length() * advance - 2.0f) * height / capGrid;
    }

    /** The strokes of `text` (capitals only: small letters are drawn as capitals), its top left at (0, 0). */
    inline juce::Path vectorPath (const juce::String& text, float height)
    {
        juce::Path p;
        const float s = height / capGrid;
        float x0 = 0.0f;
        for (auto c : text.toUpperCase())
        {
            juce::StringArray lines = juce::StringArray::fromTokens (detail::strokes (c), "|", {});
            for (const auto& line : lines)
            {
                const auto pts = juce::StringArray::fromTokens (line, " ", {});
                for (int i = 0; i < pts.size(); ++i)
                {
                    const float gx = pts[i].upToFirstOccurrenceOf (",", false, false).getFloatValue();
                    const float gy = pts[i].fromFirstOccurrenceOf (",", false, false).getFloatValue();
                    const juce::Point<float> q (x0 + gx * s, height - gy * s);
                    if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
                }
            }
            x0 += advance * s;
        }
        return p;
    }

    /** The beam over `path`: three passes (halo, glow, core), every point pulled off its line a little. */
    inline void beam (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float px, int seed, float jitter)
    {
        auto hash = [] (float a, float b, int s) { const float h = std::sin (a * 12.9898f + b * 78.233f + (float) s * 37.719f) * 43758.5453f; return h - std::floor (h) - 0.5f; };
        juce::Path wobbly;
        juce::PathFlatteningIterator it (path, {}, 0.5f);
        bool first = true;
        while (it.next())
        {
            // each segment subdivided so the wander follows the stroke, not just its ends
            const juce::Point<float> a (it.x1, it.y1), b (it.x2, it.y2);
            const int n = std::max (1, (int) (a.getDistanceFrom (b) / std::max (1.0f, 3.0f * px)));
            for (int k = (it.subPathIndex == 0 || first) ? 0 : 1; k <= n; ++k)
            {
                const auto q = a + (b - a) * ((float) k / (float) n);
                const juce::Point<float> d (hash (q.x, q.y, seed) * jitter, hash (q.y, q.x, seed + 7) * jitter);
                if (k == 0 && it.subPathIndex == 0) wobbly.startNewSubPath (q + d);
                else wobbly.lineTo (q + d);
            }
            first = false;
        }
        const juce::PathStrokeType::JointStyle j = juce::PathStrokeType::curved;
        const auto cap = juce::PathStrokeType::rounded;
        g.setColour (colour.withAlpha (0.10f)); g.strokePath (wobbly, juce::PathStrokeType (4.2f * px, j, cap));
        g.setColour (colour.withAlpha (0.28f)); g.strokePath (wobbly, juce::PathStrokeType (2.2f * px, j, cap));
        g.setColour (colour.withMultipliedBrightness (1.25f).withAlpha (0.95f)); g.strokePath (wobbly, juce::PathStrokeType (1.05f * px, j, cap));
    }

    /** Text drawn by the beam in `r` (one line; shrunk to fit its width), justified left, centred or right. */
    inline void text (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float height, juce::Colour c, juce::Justification just,
                      float px, int seed, float jitter)
    {
        float h = std::min (height, r.getHeight());
        if (width (t, h) > r.getWidth() && t.isNotEmpty()) h *= r.getWidth() / width (t, h);
        const float w = width (t, h);
        const float x = just.testFlags (juce::Justification::horizontallyCentred) ? r.getCentreX() - 0.5f * w
                      : just.testFlags (juce::Justification::right) ? r.getRight() - w : r.getX();
        const float y = r.getCentreY() - 0.5f * h;
        auto p = vectorPath (t, h);
        p.applyTransform (juce::AffineTransform::translation (x, y));
        beam (g, p, c, px, seed, jitter);
    }

    /** Words wrapped into lines no wider than `maxW`, each drawn by the beam; returns the height used. */
    inline float paragraph (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float height, float lineGap, juce::Colour c,
                            float px, int seed, float jitter, int maxLines = 6)
    {
        juce::StringArray words; words.addTokens (t, " ", {});
        juce::String line; float y = r.getY(); int lines = 0;
        auto flush = [&]
        {
            if (line.isEmpty() || lines >= maxLines) return;
            auto p = vectorPath (line, height); p.applyTransform (juce::AffineTransform::translation (r.getX(), y));
            beam (g, p, c, px, seed + lines * 13, jitter);
            y += height + lineGap; ++lines; line.clear();
        };
        for (const auto& w : words)
        {
            const auto next = line.isEmpty() ? w : line + " " + w;
            if (width (next, height) > r.getWidth() && line.isNotEmpty()) flush();
            line = line.isEmpty() ? w : line + " " + w;
        }
        flush();
        return y - r.getY();
    }
}
