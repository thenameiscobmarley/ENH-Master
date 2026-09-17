#include "PointerPoller.h"

// Xlib is kept out of every header: its macros (None, Bool, Status...) clash with JUCE.
#include <X11/Xlib.h>

namespace pad
{
    struct PointerPoller::Impl
    {
        Display* display = nullptr;
        bool failed = false;

        ~Impl()
        {
            if (display != nullptr)
                XCloseDisplay (display);
        }
    };

    PointerPoller::PointerPoller() : impl (std::make_unique<Impl>()) {}
    PointerPoller::~PointerPoller() = default;

    bool PointerPoller::query (unsigned long window, int& windowX, int& windowY) noexcept
    {
        if (window == 0 || impl->failed)
            return false;

        if (impl->display == nullptr)
        {
            impl->display = XOpenDisplay (nullptr);

            if (impl->display == nullptr)
            {
                impl->failed = true;
                return false;
            }
        }

        ::Window root = 0, child = 0;
        int rootX = 0, rootY = 0;
        unsigned int mask = 0;

        return XQueryPointer (impl->display, (::Window) window, &root, &child,
                              &rootX, &rootY, &windowX, &windowY, &mask) != 0;
    }
}
