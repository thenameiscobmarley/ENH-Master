# The loupe

Panel print is small, as it is on real hardware. Rather than enlarging the text (which would ruin the
layout) or showing a flat tooltip, hovering opens a **glass magnifier**.

## How it is made

1. The scene is rendered a **second time**, zoomed, into an off-screen framebuffer
   (`gfx::RenderTarget`) - so the magnified print is genuinely re-rasterised at that size, not
   stretched pixels. The zoomed view-projection comes from `hwk::fx::Loupe::zoomedViewProj`.
2. That texture is drawn through the `magnifierLens` material: barrel (fisheye) distortion, chromatic
   fringing at the edges, a darker inner rim, a bright bevel ring and a specular glint.
3. A ring-shaped backing and drop shadow sit behind it - a ring, not a disc, because the glass is
   translucent.

The target is allocated once at its maximum size, so opening the loupe never reallocates.

## Behaviour

- **Magnifies about the cursor.** What is under the pointer stays under the pointer, only bigger, so
  you can read one label and move straight on to the next through the glass.
- **Slightly transparent**, so the panel underneath stays visible.
- **Locks while you adjust.** During a drag it anchors to the control being moved, so the value pill
  stays readable instead of chasing whatever print passes under the mouse.
- **Zooms out to leave.** On unhover the magnification collapses quickly and the fade trails it, so
  it reads as zooming out and then being gone, rather than blinking off.
- Controls also get a name + value pill under the glass and a lit value arc on the knob.

## Cost

Measured with `PAD_UI_TEST_STATS=1`: `avgCpuRender` 1.22 ms without the loupe, 1.21 ms with it - the
second pass is GPU work, and it only runs while the loupe is open.

Related: [[HardwareKit]], [[Rendering and performance]], [[05 Reading the panel]].
