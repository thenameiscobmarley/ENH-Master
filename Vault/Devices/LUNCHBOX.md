# LUNCHBOX

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A small side rack of three plug-in modules, in the style of a 500-series "lunchbox", on a walnut stand
to the right of the rack (full rack view; Simple view puts it away). It comes after CHARACTER and before
the OUTPUT MONITOR, so EAR GUARD and the limiter still look after everything it does. Signal runs
left to right: **CLASS-A EQ → DE-HARSH → CROSSFEED**, then an **OUTPUT** meter; the sixth slot is empty.
Everything starts OUT: old sessions and presets sound the same. Each has its own **IN** switch. When a module is
off it doesn't touch the sound at all. Switching fades over 20 ms, so it never clicks.

## CLASS-A EQ

A model of the classic British console channel EQ of the 1970s. It has stepped frequencies and
broad, musical shapes, and it's hard to make it sound bad. It works for mixing and mastering, and for
shaping a game's tone. The curves follow the analogue ones all the way up to 20 kHz, with no
digital squashing near the top.

- **HPF** (off, 50, 80, 160, 300 Hz). Removes rumble and boom with a firm 18 dB/oct slope.
- **LOW** (35, 60, 110, 220 Hz) with a **±16 dB** knob. The classic inductor shelf: boosted, it
  bumps up about 2 dB just below the chosen frequency and dips about 1.5 dB a little above it, so
  the bass gets weight without mud. Cut is the mirror image.
- **MID** (360, 700, 1.6k, 3.2k, 4.8k, 7.2k Hz) with a **±18 dB** knob. A broad bell. The more you
  boost, the more focused its peak gets (about an octave wide at full boost), like the hardware.
  Cut is the mirror image of boost.
- **HI Q**. Makes the mid bell about twice as narrow, for one exact spot.
- **HIGH** (±16 dB). A gentle 12 kHz shelf for air: it starts rising around 3 kHz and is half way
  up at 12 kHz (+8 dB at 12 kHz with the knob at +16, +11 dB at 20 kHz).
- **IRON**. Adds the input and output transformers and the class-A output stage. The transformers
  thicken the lows (mostly 3rd harmonic, more the lower and louder the note), the class-A stage adds
  a trace of 2nd harmonic everywhere. It's subtle: about 0.03 % at -18 dBFS, 0.5 % for a full-scale
  100 Hz note, 2 % at 30 Hz, well under 0.1 % in the mids and highs. The volume stays the same;
  the very lows get a tiny lift around 20 Hz and roll off below 10 Hz, the top a hair of lift near
  20 kHz. No aliasing (the class-A stage runs at twice the rate inside) and no added latency.

With every knob at 0 and IRON off, the EQ is perfectly transparent, even while it's on. Moving a
frequency switch fades over to the new setting and doesn't sweep, so nothing whooshes or clicks.

## DE-HARSH

This catches piercing upper-mid peaks that tire your ears on a headset: gunshots, breaking glass,
sibilance, screechy synths. It works like a studio de-esser: it compares one band with the rest of
the sound and turns only that band down (a dynamic bell) while it sticks out. Loud sound that's
balanced is left alone, and the rest of the mix never pumps. While it isn't cutting, it doesn't
touch the sound at all.

- **AMOUNT** (0 to 10). How deep it may cut (10 allows up to 9 dB) and how firmly it holds the band
  down.
- **FREQ** (2.5k, 4k, 6.5k Hz). The band it watches: 2.5k is edgy, 4k is piercing, 6.5k is hiss and
  sibilance.
- **SPEED** (10 to 200 ms). How quickly it lets go. It catches peaks in about 1 ms. After a long
  harsh passage it lets go a little more slowly, so it doesn't flutter.
- **CUT** lamp. Lights up as it cuts (full at 6 dB).

**Tip:** for long game sessions, try AMOUNT 5 at 4k. If you only want the worst shots tamed, use
AMOUNT 10 with a short SPEED.

## CROSSFEED

For headphones. On speakers, each ear hears both speakers. On headphones, each ear hears only one
side, which is tiring when a sound is panned hard left or right. CROSSFEED is the classic Bauer
headphone crossfeed: some of each side reaches the other ear, with the treble rolled off and about a
quarter of a millisecond later, as if it came from a speaker. Direction stays clear and listening
gets easier.

- **AMOUNT** (0 to 10). How much crosses over. 10 is the classic strong setting (the other ear 4.5
  dB down, rolled off from 700 Hz), 7 is medium (6 dB), 4 is light (9.5 dB, from 650 Hz), 1 is very
  light.

Sound in the centre (voices, most music) comes through exactly as it was, at the same level and with
the same tone. Only the sides are blended. Leave it off on speakers.

Code: `Lunchbox.h`.
