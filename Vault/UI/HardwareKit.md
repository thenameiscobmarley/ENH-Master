# HardwareKit

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

[HardwareKit](https://github.com/thenameiscobmarley/HardwareKit) is a separate library with
everything about the 3D hardware look that isn't specific to ENH Master, so future plugins can reuse it.
It lives next to this repo (`../HardwareKit`).

| Folder | What's in it |
|---|---|
| `gfx/` | OpenGL helpers: maths, shaders, meshes, textures, frame buffers |
| `geo/` | shapes: rounded boxes, plates, quads |
| `models/` | the hardware: knobs, buttons, toggles, lamps, LEDs, screws, chassis |
| `anim/` | knob, button and toggle animation |
| `shaders/` | materials: plastic, chrome, print, glow, the magnifier lens |
| `fx/` | the loupe |
| `input/` | mouse and window helpers — the only per-platform code (X11 and Windows) |

**Knob styles:** `proXl`, `fluted`, `chickenHead`, `aluminium`, `softTouch`, `jewelCap`.

**What stays in ENH Master:** the layout (`DeviceLayout.h`), the printed panels (`PanelArtwork`) and a
few display shaders.

Related: [The loupe](The%20loupe.md), [Rendering and performance](Rendering%20and%20performance.md).
