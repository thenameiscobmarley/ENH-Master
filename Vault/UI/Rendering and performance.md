# Rendering and performance

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

It's built to run on a cheap mini PC (Intel J4105 with UHD 600 graphics).

## How it stays light

- OpenGL 3.2, one simple shader per material, low-poly shapes uploaded once.
- Soft shadows are calculated, not rendered from shadow maps.
- Its own anti-aliasing, so it looks the same in every host.
- Things that never change are worked out once into textures: the studio wall, and the room every
  metal, chrome and glass surface reflects (a window up to the left, a warm floor lamp, walnut walls).
  One texture read each, instead of maths per pixel.
- The clear coat (the wet look): a second, sharp reflection of that room on top of each material, weighted
  by Fresnel (strong at grazing angles), plus a tight glint of the key light. How wet each surface is:
  `coatFor` in `PluginMaterials.h`; HardwareKit's `uCoat` / `uCoatLod` (0 = off, the default for other
  plugins). Its smudges are baked into a small tiling texture, and the room map's coordinates are worked
  out once per pixel. It costs 2 - 4 ms a frame on an integrated GPU: `"wetCoat": 0` in `ui-config.json`
  turns it off (0.5 halves it); `PAD_UI_TEST_COAT=0` for A/B.
- The reflections are sharp, like still water: every coat mirrors the room at full detail (a 2048 x 1024
  room map); only brushed metal and powder coat soften it a little, from their grain. Very little haze.
- Light from the whole room: the room map is turned once into nine numbers (spherical harmonics), so a
  face turned to the window gets the window's light and a flank turned to the walnut gets warm light.
  Worked out per corner of each shape, not per pixel: free.
- Shadows and ambient occlusion are ray traced once, when the rack is built. Every knob, switch and
  button stands in as a sphere; each panel's face is traced against them (exact occlusion, and the key
  light's soft shadow) and against the case's walnut cheeks, into one small light map. Drawing reads it
  once per pixel: about 1 ms a frame on a UHD 600, nothing on a normal GPU. Traced again when SIMPLE /
  FULL view moves the units. `PAD_UI_TEST_TRACE=0` turns it off (A/B).
- Drawing follows the screen's refresh while you interact, half as often when idle, **10 times a
  second when another program (like a game) is in front**, and **not at all** when the window is hidden
  or minimised. Audio never stops.

## Input

The mouse is read once per frame, straight from the system. Clicks only count when the plugin
window is really on top — otherwise clicking another app over it could turn its knobs.

## Numbers

- Sound: about a quarter of one J4105 core for the whole rack (`EnhDspTests --cpu`).
- Picture: about 57–60 frames a second for the whole rack.

Related: [HardwareKit](HardwareKit.md), [Dev hooks](../Reference/Dev%20hooks.md).
