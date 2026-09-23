# Glass panel

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Click a unit and a frosted glass panel opens on the right, joined to the unit by a thin white line.

![The glass panel](../../docs/screenshot-panel.png)

## What's in it

Settings listed one under another (scroll with the wheel), in folding groups:

- **PROCESSING** — how the unit works. Each setting has two or three named choices.
- **KNOBS** — a dropdown per knob with **SMOOTHING**, **CURVE** and **RANGE**.
- **STEREO** — L/R, only the middle, or only the sides (on six units).
- **OUTPUT / DISPLAY** — on the OUTPUT MONITOR.
- **RESET TO DEFAULTS** — puts the whole unit back.

Hover any setting and the bottom of the panel explains what it does to the sound and what it costs.
A small white square marks settings you've changed. The full list: [Methods](../Reference/Methods.md).

## Good to know

- The first choice is always the original sound, so old sessions sound the same.
- Switching never clicks, and never changes the plugin's delay.
- Settings are saved with your session; presets leave them alone.

## Clicks

| Where | What happens |
|---|---|
| a unit's front | opens its panel (again = close, another = switch) |
| off the rack | closes it |
| the wheel over the panel | scrolls it |

## How it's drawn

The glass is the rack behind it, blurred at a quarter size, only while the panel is open. White text,
square corners, a soft shadow, no tint. It costs nothing measurable on a small laptop GPU.

Code: `GlassPanel.h/.cpp`, `MethodRegistry.h`.
