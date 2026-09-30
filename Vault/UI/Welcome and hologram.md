# Welcome screen and hologram

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

When ENH Master opens, a **welcome screen** appears as a hologram in the PHOSPHOR scope's green: the
name and version, where to start (click a unit, right-click the rack, the presets), what's new, and a live
scope trace. **Enter the rack** (or a click anywhere outside it) closes it.

- **Show this at start**: untick it and it stays away. It still comes back once after an update.
- **Hologram settings panel**: the [Glass panel](Glass%20panel.md) drawn as a hologram too, in the same
  green on a dark light box with faint scanlines. The same layout, and just as easy to read.

Both are also in the rack's right-click menu (**Welcome screen...**, **Hologram settings panel**), in the
3D and the [2D version](2D%20version.md). The choices are saved in `ui-config.json` (`showWelcome`,
`holoPanel`).

It costs next to nothing: the card is drawn once as a picture; each frame adds scanlines over it and about
200 short strokes for the trace.

Code: `Source/UI/Holo/HoloWelcome.h`, the renderer's `drawWelcome`, the `holoCard` material.
