# FOOTSTEP RADAR

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The olive-green unit between the ADAPTIVE COMPRESSOR and TONE & SPACE: CHARACTER's sister, the same
front panel in its own finish. It finds
footsteps (any surface, any game, near or far) and makes each one easier to hear and to place.

## Knobs and buttons

- **SENSITIVITY** — how faint a step it goes after. 6 is the default; turn it up for very quiet games,
  down if it lifts things that aren't steps.
- **BOOST** — how much a found step is lifted (0–12 dB). Far, quiet steps get the most; a loud step
  right next to you gets little, since you can already hear it.
- **SPACE** — how much room it adds to far steps, so you can tell *far* from *near*. The further the
  step, the more room it gets and the later that room arrives. Near steps get none, so they stay dry and close.
- **IN** — on/off (the red switch). The COMPETITIVE FOOTSTEPS preset turns it on.
- **LISTEN** — (the switch under it) hear only what the radar adds. Good for checking what it's catching.

## Its meter

**STEP LIFT dB** — the needle kicks with every step it lifts, and how far it swings is how much it
lifted it (far steps swing further). On the ADAPTIVE ENHANCER, the **STEPS** lights in the METER
section flash with every step too.

## What makes it good at this

- It **listens to the rack's input**, before anything changes the dynamics, and it runs 2.5 ms ahead
  of what you hear, so the lift starts with the step and doesn't arrive after it.
- It lifts **only the step's own frequencies**, and only while the step lasts. Everything else is untouched.
- It knows the difference between a step and a gunshot, an explosion, a voice, music, rain, a reload
  or a UI click, by how each one starts, dies away and rings.
- It **follows walkers**: once it has heard a few steps from one place, it knows the pace, and it catches
  that walker's next steps more easily, even very faint ones. Two walkers at once are followed apart.
- Steps as faint as the background can still be found when three come in a steady walking rhythm.

How it works inside: [Footstep detection](../DSP/Footstep%20detection.md).

## Glass panel

- **DETECTION** — *STD* (standard), *SEN* (sensitive: takes more), *STR* (strict: only sure ones).
- **ROOM** — the space it adds to far steps: *ROM* (a room), *HAL* (a hall), *OPN* (open air).

## Good to know

- Delay: 2.5 ms (reported to your DAW).
- Test it: `EnhDspTests --radar table` scores it on 70 scenes; see [Dev hooks](../Reference/Dev%20hooks.md).

Code: `FootstepRadar.h/.cpp`.
