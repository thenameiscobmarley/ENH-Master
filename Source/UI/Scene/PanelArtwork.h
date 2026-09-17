#pragma once

#include <juce_graphics/juce_graphics.h>

/*  2D artwork rendered with juce::Graphics on the message thread and uploaded
    as small textures:

    - Faceplate decal (RGBA): R = dark ink, G = pink accent ink,
                              B = left footprint glyph mask, A = right footprint glyph mask
    - Knob dial (R8): numbers + ticks printed on the rotating skirt
    - Display overlay (R8): phosphor text, re-rendered only when the text changes
*/
namespace pad::artwork
{
    struct RawTexture
    {
        int width = 0, height = 0, channels = 0;
        std::vector<juce::uint8> pixels;
    };

    RawTexture renderFaceplateDecal (int textureWidth);
    RawTexture renderKnobDial (int size);

    struct DisplayText
    {
        juce::String title, tag, lineLeft, lineRight, focusLine;

        bool operator== (const DisplayText& o) const
        {
            return title == o.title && tag == o.tag && lineLeft == o.lineLeft
                && lineRight == o.lineRight && focusLine == o.focusLine;
        }
    };

    inline constexpr int displayOverlayWidth = 512, displayOverlayHeight = 342;

    RawTexture renderDisplayOverlay (const DisplayText&);
}
