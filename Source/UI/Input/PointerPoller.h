#pragma once

#include <memory>

namespace pad
{
    /** Reads the pointer position straight from the X server, on whichever thread calls it.

        Plugin hosts (e.g. Carla's VST3 bridge) pump the plugin's UI event queue at their own,
        often uneven, rate. Polling the pointer once per rendered frame on the render thread
        keeps parallax motion smooth regardless of how late mouse events are delivered.
        Uses a private X connection, so it never contends with JUCE's own.
    */
    class PointerPoller
    {
    public:
        PointerPoller();
        ~PointerPoller();

        /** window = native X11 window of the editor's peer.
            Returns false if unavailable (no X server, window gone, pointer on another screen). */
        bool query (unsigned long window, int& windowX, int& windowY) noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
