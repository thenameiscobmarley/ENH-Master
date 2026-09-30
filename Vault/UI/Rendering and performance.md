# Rendering and performance

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

It's built to run on a cheap mini PC (Intel J4105 with UHD 600 graphics).

## How it stays light

- OpenGL 3.2, one simple shader per material, low-poly shapes uploaded once.
- Soft shadows are calculated, not rendered from shadow maps.
- Its own anti-aliasing, so it looks the same in every host.
- Things that never change are worked out once into textures: the wall behind the rack (a room in a house
  in the daytime: plaster, a tall window, and the view through it, painted in code - sky, clouds, hills,
  woods, a river with a stone arch bridge and a suspension bridge far off - a little out of focus), and the
  room every metal, chrome and glass surface reflects (the daylight window behind the rack, plaster walls, a
  wooden floor, a warm lamp).
- The room is finished like a real one: linen curtains in soft folds either side of the window (the day
  glowing through them), a rail, a deep sill with its shadow, the frame's bars bevelled, a framed print on
  the wall, a quartz upstand where the wall meets the shelf, warm mottled plaster. All baked, all free.
- The cables find their own shape: HardwareKit's cable model (`geo/Cable.h`) lets each one hang from its
  plugs as a real cable would - its own length, gravity, never bent tighter than its minimum radius (no
  kinks), resting on the shelf, round the case and the LUNCHBOX rather than through them, leaving each plug
  straight - and sweeps it with frames that never twist. Worked out once when the scene is built.
- The rack stands on a shelf: polished white quartz on a walnut cabinet. The quartz mirrors the wall behind
  it (the view ray off the shelf, followed to the wall, read from the wall's texture: one fetch) and the day
  falls through the window onto it in three panes of light. Measured: no cost against a plain floor.
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
