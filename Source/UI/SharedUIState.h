#pragma once

#include <juce_core/juce_core.h>
#include "Scene/PanelArtwork.h"

namespace pad
{
    /** Lock-free (or spin-locked) state shared between the message thread
        (input, text) and the OpenGL render thread. */
    struct SharedUIState
    {
        // written by message thread
        std::atomic<float> mouseNdcX { 0.0f }, mouseNdcY { 0.0f };
        std::atomic<bool>  mouseInside { false };
        std::atomic<int>   hoveredControl { -1 };
        std::atomic<int>   activeControl { -1 };
        std::atomic<int>   viewWidth { 1 }, viewHeight { 1 };

        // written by render thread
        std::atomic<float> parallaxX { 0.0f }, parallaxY { 0.0f };
        std::atomic<bool>  animating { true };

        // scope overlay hand-off
        juce::SpinLock        overlayLock;
        artwork::RawTexture   overlayPending;     // guarded by overlayLock
        juce::uint32          overlayVersion = 0; // guarded by overlayLock
    };
}
