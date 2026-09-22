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
- The scene is drawn into the plugin's own 4x multisampled target and filtered to the screen, so
  anti-aliasing never depends on the host's window. `renderScale` (0 = auto = 1x) can supersample on
  GPUs with room to spare; on the UHD 600 1.25x already costs ~13 fps.
- Panel-local coordinates throughout (x across, z down, y out of the panel), which is why layout
  numbers read like a drawing.
- Frame pacing on the render thread, locked to vsync: every refresh while interacting or animating,
  every second refresh when idle.

## Pausing

Rendering **stops completely** when the editor is not visible - checked a few times a second through
JUCE's peer state and, because plugin editors are embedded in a host window, the X server as well, so
a minimised *host* is noticed too. Audio is unaffected.

## Input

The pointer is polled once per frame straight from the X server ([[HardwareKit]]), because hosts pump
the plugin's event queue at their own uneven rate. Presses are gated on the plugin window really
being **topmost under the pointer** - without that, clicking in another application that overlaps the
plugin turned its knobs, which was a real bug found by this project.

## Numbers

- DSP: about 18 % of one J4105 core at 48 kHz for the whole rack (`EnhDspTests --cpu`,
  `CPU_BREAKDOWN=1` for per-stage numbers).
- Render: the whole rack at ~57-60 fps on the UHD 600; close-ups are limited by pixel fill.

Related: [[Dev hooks]].
