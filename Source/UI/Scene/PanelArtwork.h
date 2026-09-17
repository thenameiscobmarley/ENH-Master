#pragma once

#include <juce_graphics/juce_graphics.h>

/*  2D artwork rendered with juce::Graphics on the message thread, uploaded to
    the GPU as small textures:

    - Panel decal (RGBA): R = ink print, G = pink accent print,
                          B = left footstep glyph mask, A = right footstep glyph mask
    - Knob dial (R8): radial numbers + ticks for the flange face
    - Scope overlay (R8): monochrome phosphor text, re-rendered only when text changes
*/
namespace pad::artwork
{
    struct RawTexture
    {
        int width = 0, height = 0, channels = 0;
        std::vector<juce::uint8> pixels;
    };

    RawTexture renderPanelDecal (int textureWidth);
    RawTexture renderKnobDial (int size);

    struct ScopeText
    {
        juce::String title, target, lineLeft, lineMid, lineRight, focusLine, footer;

        bool operator== (const ScopeText& o) const
        {
            return title == o.title && target == o.target && lineLeft == o.lineLeft && lineMid == o.lineMid
                && lineRight == o.lineRight && focusLine == o.focusLine && footer == o.footer;
        }
        bool operator!= (const ScopeText& o) const { return ! operator== (o); }
    };

    inline constexpr int scopeOverlayWidth = 512, scopeOverlayHeight = 336;

    RawTexture renderScopeOverlay (const ScopeText&);
}
