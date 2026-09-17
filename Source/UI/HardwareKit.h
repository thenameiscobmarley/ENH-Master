#pragma once

/*  ENH Master uses the shared HardwareKit module (~/Projects/HardwareKit) for its models, materials,
    GL helpers, control animation and input. These aliases keep the plugin code short. */
#include <hardwarekit/hardwarekit.h>

namespace pad
{
    namespace gfx  = hwk::gfx;
    namespace anim = hwk::anim;
    using hwk::input::PointerPoller;
    using hwk::input::WindowVisibility;
}
