# The loupe

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The print is small, like on real hardware. Hover anything and a **magnifying glass** appears.

![The loupe](../../docs/screenshot-hover.png)

## How it behaves

- It magnifies around your mouse, so what's under the pointer stays under the pointer.
- It's slightly see-through.
- While you drag a knob it stays on that knob, so you can read the value.
- Knobs show their name and value under the glass, and their value arc lights up.
- It zooms out as it goes, instead of blinking off.

## How it's made

The scene is drawn a second time, zoomed in, so the text is really sharp (not stretched pixels), then
shown through a fisheye lens with a bright rim. It only costs anything while it's open.

Code: `hwk::fx::Loupe` in [HardwareKit](HardwareKit.md).
