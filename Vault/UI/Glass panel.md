# Glass panel

Click a unit and a frosted glass panel opens at the right of the window, joined to the unit by a thin
white line. Back to [[00 Start Here]].

## What it shows

The unit's name, then its settings **one under another** in a column that scrolls with the wheel,
never side by side:

- each **stage** of the unit's processing (how it measures, how it calculates, how it smooths) and
  its method, from `Source/DSP/MethodRegistry.h`;
- each **knob modifier** the unit has (the pilot: RESPONSE smoothing);
- at the bottom, a few lines about whatever is under the pointer: how it changes the sound, what it
  costs.

Click a setting to list its choices under it; click a choice to use it. The full list is [[Methods]].
Only the ADAPTIVE COMPRESSOR has stages so far (the pilot); the other units show their name.

## Clicks

| Where | What happens |
|---|---|
| the panel | the panel's own (never a knob behind it) |
| a control | works as always, panel open or not |
| a unit's faceplate | opens its panel; another unit switches; the same one closes it |
| off the rack | closes the panel; with none open, steps back to the whole rack |
| the wheel | over the panel, scrolls it; elsewhere, walks up to the unit under the pointer |

The panel is hit-tested first on both input paths: `HardwareView::mouseDown` (message thread) and
`HardwareRenderer::handleInteraction`, which ignores presses inside `SharedUIState::pointInPanel`. The
loupe and the hover outlines are off while the pointer is over the glass.

## How it is drawn

- **Print:** `GlassPanel::render` draws the white text and hairlines into an RGBA image with JUCE, only
  when something shown changes (hover, a choice, a value changed by the host), at twice the screen's
  resolution. It is handed to the render thread like the loupe's pill.
- **Glass:** one quad, `glassPanel` material. The scene behind the panel, blurred, lifted a little toward
  white, with square corners, a one-pixel edge that catches the light (brightest along the top), a soft
  shadow falling under it, and the print on top. No tint and no coloured light.
- **Blur:** `blurBehindPanel` takes the finished frame down to a quarter of the resolution and runs three
  pairs of Gaussian passes (1x, 2x, 4x wide), each scissored to the panel's rectangle. It only runs
  while the panel is open. Without the plugin's own frame buffer the glass is a plain frost.
- **Line:** from a small square on the unit's right edge, out level with it, then diagonally to the
  panel's header. It draws out along its length as the panel opens, and folds back when it closes.
- **Hover outlines:** the hovered control's own mesh again, 6 % larger, with front faces culled (an
  inverted hull: only its rim shows); a unit gets one quad over its faceplate with a crisp edge line
  (`outlineFrame`). The open panel's unit keeps a faint white outline.

## Cost

On the J4105 / UHD 600 at 1340x720, `build/stats.sh` gave the same frame times, within run-to-run
noise, with the panel open, blurred and a control outlined as without them (rack about 27 ms,
close-up about 38 ms a frame under load).

Related: [[The loupe]], [[Rendering and performance]], [[Methods]].
