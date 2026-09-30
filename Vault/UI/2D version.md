# ENH Master 2D

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The same plugin as ENH Master (the same sound, presets and settings) with a flat rack instead of the 3D
one. It's a separate download on the [Releases](https://github.com/thenameiscobmarley/ENH-Master/releases)
page (`ENH-Master-2D-...zip`) and installs alongside ENH Master as its own plugin, **ENH Master 2D**.

Use it on older or weaker graphics: it draws with JUCE's own 2D drawing, with no OpenGL and no 3D. Each
faceplate is drawn once for the window's width. After that, a frame only copies those pictures and draws the
knobs and meter needles over them, and only when something moved.

## Using it

- **The wheel**, or a **drag** on a faceplate, slides the rack up and down.
- **Drag a knob** up or down to turn it (**Shift**: finely). **Double-click** a knob for its default.
- **Click** a selector to step it, and a switch to flip it. Hover a control to see its value.
- **Right-click** a unit for its settings (the [Glass panel](Glass%20panel.md)), or the
  [Gear locker](Gear%20locker.md).

Every unit you have installed is shown, top to bottom as in the 3D rack, then the LUNCHBOX with its
modules. The displays (the analyser, the loudness screen) are dark glass here; the meters' needles move.

Code: `Source/UI/Flat/FlatRackView.cpp`. Built by the `EnhMaster2D` target (`ENH_BUILD_2D`, on by default).
