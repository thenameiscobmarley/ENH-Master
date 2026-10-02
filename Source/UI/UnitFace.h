#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Scene/DeviceLayout.h"
#include "Scene/PanelArtwork.h"

/*  A unit's faceplate painted flat, in software: its paint, its print, its windows and its meters' faces
    (and, asked, its controls drawn simply). The 2D rack is made of these; THE GEAR LOCKER shows small ones
    as each unit's preview. */
namespace pad::face
{
    /** The renderer's plate colours are linear light; a flat picture is sRGB. */
    juce::Colour fromLinear (float r, float g, float b);
    struct Plate { float r, g, b; };
    Plate plateOf (int unit);
    juce::Colour knobColour (layout::KnobStyle);
    /** A meter's face: the cream card, its print (channel 0) and its red zone (channel 1). */
    juce::Image meterFace (const artwork::RawTexture&);
    /** A texture from the artwork as an image: one channel (print) becomes the ink colour, with that alpha. */
    juce::Image toImage (const artwork::RawTexture&, juce::Colour ink);
    /** The faceplate at `scale` pixels a panel unit. */
    juce::Image render (int unit, float scale, bool withControls = false);
}
