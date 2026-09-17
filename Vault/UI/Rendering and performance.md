# Rendering and performance

The target machine is an Intel J4105 with UHD 600 graphics - a cheap mini PC. Everything here exists
because of that.

## Shape of the renderer

- OpenGL 3.2 core, all drawing on the GL thread; the message thread only prepares textures and reads
  input.
- **One shader program per material**, with no per-fragment branching - a branch-free fragment shader
  is worth more than a clever one on this GPU.
- Procedural low-poly geometry, uploaded once.
- Analytic SDF contact shadows instead of shadow maps.
- Panel-local coordinates throughout (x across, z down, y out of the panel), which is why layout
  numbers read like a drawing.
- Frame pacing on the render thread, locked to vsync: every refresh while interacting or animating,
  every second refresh when idle.

## Pausing

Rendering **stops completely** when the editor is not visible - checked a few times a second through
JUCE's peer state and, because plugin editors are embedded in a host window, the X server as well, so
a minimised *host* is noticed too. Audio is unaffected. Measured on the standalone: 17 % of one core
visible, 8 % minimised.

## Input

The pointer is polled once per frame straight from the X server ([[HardwareKit]]), because hosts pump
the plugin's event queue at their own uneven rate. Presses are gated on the plugin window really
being **topmost under the pointer** - without that, clicking in another application that overlaps the
plugin turned its knobs, which was a real bug found by this project.

## Numbers

- DSP: 14.06 % / 15.09 % / 28.80 % of one core at 44.1 / 48 / 96 kHz, both units running.
- Render: ~1.2 ms CPU per frame, loupe open or not ([[The loupe]]).

Related: [[Dev hooks]].
