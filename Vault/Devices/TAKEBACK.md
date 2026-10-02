# DYNAMICS RESTORER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called TAKEBACK.

Takes back what processing took out of the sound (2U, model BLONDEX, by Texas Studios). Designed in the
[Rack Unit Designer](https://thenameiscobmarley.github.io/ENH-Master/designer.html). It starts in the
[Gear locker](../UI/Gear%20locker.md) and comes after the VELVETIZER. **POWER** off: it doesn't touch
the sound at all.

- **SHARPEN** — gives back the attacks that compression and limiting squashed (9 dB at most).
- **BLUR** — the opposite: rounds the attacks off, softer and smoother.
- **COLOR** — warmth: a valve's harmonics under the sound.
- **RAW** — a short room bloom under the sound: wet and full, gone within 40 ms, never washed out.
- **SHINE** — rebuilds the air: new top octave (above 9 kHz) made from the upper mids, not a treble boost.
- **MIX** — the takeback against the sound as it came in.
- **AUTO** — measures what was lost (how squashed and how dull the sound is) and gives back that much.
  A brick-walled master gets the most SHARPEN, a dull one the most SHINE. The knobs set the most it may give;
  SHARPEN and SHINE turn on screen to show what it is giving.

The **PROCESS** display scrolls the attack shaping (up: lifted, down: rounded off) and shows AUTO's two
readings (how squashed, how dull) on the right. Meters: **IN dB+** the attacks lifted, **IN dB-** the attacks rounded off, **OUT dB+ / dB-** how much
the output is brought up or down to stay as loud as it came in. The LED ladder under each knob shows how
hard that section is working. The knobs are the designer's own "Grey Ribbed Khris", made in its knob maker.

Level-matched, click-free, no latency.

Code: `DSP/Takeback.h`, `DSP/UnitKit.h`. Tests: `EnhDspTests --designed`.
