#pragma once

#include <juce_graphics/juce_graphics.h>

/*  Dev only (PAD_UI_EXPORT_SITE=<dir>): the website's units gallery, made from the plugin's own code - every
    unit's faceplate (the painter the gear locker's previews use, UnitFace.h) as <dir>/faces/<key>.png, and each
    simulation's screen (SimScreens.h, RoomScreen.h, ColourScreens.h, from their demo states) as 48 frames at
    30 a second, <dir>/screens/<key>/f00.png ... (scripts/make-site-units.py makes the site's files from them). */
namespace pad::siteexport
{
    void write (const juce::File& dir);
}
