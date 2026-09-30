# CUSTOM

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A slot for a unit you made in the [Rack Unit Designer](https://thenameiscobmarley.github.io/ENH-Master/designer.html) (2U).
Design it there - its panel and, in the Sound tab, its sound - then **Share → Copy link** (or copy the code), open
CUSTOM's glass panel in ENH Master and click **PASTE A DESIGN CODE**. Its panel, knobs and sound load in the rack.

- Its knobs and switches go where you put them (up to 16 knobs and 4 switches; knobs are drawn in one style).
- Each knob or switch does what you wired it to in the designer's Sound tab (Controls).
- The sound: the same blocks as the designer - EQ, filter, saturator, compressor, exciter, delay, reverb, width, output.
- A taller design is scaled to fit the 2U slot.
- **EMPTY THE SLOT** unloads it. The design is saved with the session.
- It starts in the [Gear locker](../UI/Gear%20locker.md). The code is checked like the designer checks it: only design data gets in.

Code: `Custom/DesignCode.h`, `DSP/units/CustomUnit.h`. Tests: `EnhDspTests --units16`.
