# HardwareKit

A separate JUCE module (its own repository, checked out next to this one) holding everything about
the 3D hardware look that is **not** specific to ENH Master, so the next plugin starts from it.

```
modules/hardwarekit/
  gfx/        GLMath, GLResources (shaders, meshes, textures, RenderTarget/FBO)
  geo/        geometry primitives: swept rounded rects, boxes, pointer plates, quads
  models/     hardware: knob styles, push buttons, bat toggles, jewel lamps, LEDs,
              rack screws, chassis, faceplate edges
  anim/       KnobAnimator, ButtonAnimator, ToggleAnimator
  shaders/    material library (plastic, chrome, print, glow, value arc, magnifier lens, …)
  fx/         Loupe (zoomed view-projection)
  input/      PointerPoller, WindowVisibility  ← the only platform-specific code
```

Namespaces: `hwk::gfx`, `hwk::geo`, `hwk::models`, `hwk::anim`, `hwk::shaders`, `hwk::fx`,
`hwk::input`.

## Knob styles

`proXl` · `fluted` (Davies-1900 type) · `chickenHead` · `aluminium` · `softTouch` · `jewelCap`.

Each returns a `Model`: a list of `Part`s with a mesh, a `Role` (body / accent / metal / pointer),
whether it rotates, colour, ridge count, polish. A model also carries `footprintRadius` (picking) and,
separately, `shadowRadius` plus an optional `beakLength` - so a chicken-head knob casts a small round
shadow for its body **and** a pointer shadow that turns with it, instead of one big disc.

In ENH Master: CLARITY / ADAPT / SUB are `proXl`, the ENH masters `aluminium`, SERAPH's row `fluted`,
its masters `softTouch` with violet caps, POWER a `chickenHead` selector.

## What stays in the plugin

Layout (`DeviceLayout.h`), printed panels (`PanelArtwork`), and the two display shaders. Everything
else is library code - and the extraction was verified by screenshot: rendering came out identical.

Related: [[The loupe]], [[Rendering and performance]], [[06 Windows and other platforms]].
