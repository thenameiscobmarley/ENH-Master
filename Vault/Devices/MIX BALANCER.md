# MIX BALANCER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Keeps the mix in balance, moment to moment. It rides six band faders (lows, 200 Hz, 500 Hz,
1.3 kHz, 3.5 kHz, highs):

- a band that suddenly jumps out is turned down;
- a band that drops away is gently turned up;
- if the whole mix gets louder, nothing moves — that's loudness, not balance.

It lets drum attacks through, ignores nearly empty bands, and gives back the loudness its cuts take
away, so the mix doesn't sound quieter.

## Knobs

| Knob | What it does |
|---|---|
| BALANCE | how much of each jump it corrects |
| SPEED | how fast it rides |
| TILT | darker (−) or brighter (+) |
| RANGE | the most any band moves |
| RESOLUTION | 6 bands, up to 28 fine bands |
| IN | on or off |

## Its screen

The spectrum before and after, the faders drawn as a curve, and the last ten seconds underneath with cuts in red.

Code: `MixBalancer.h`.
