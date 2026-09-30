# RAY ROOM

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A room around the sound, and a little of what a room full of old gear adds (3U, model RT-3). It starts in
the [Gear locker](../UI/Gear%20locker.md). **POWER** off: it doesn't touch the sound.

- **SPACE**: 0 to 10. The room's size, from a small room (4 m across) to a hall (25 m). The sound
  reaches you straight from the two speakers and off every wall: from each speaker, eight reflections
  (first and second order), each as late as its path is long, quieter with distance and with every wall
  it met, and from its own direction. Then a short tail, as long as the room is big.
- **DAMP**: 0 to 10. How much the walls soak up: bright and ringing at 0, soft and short at 10.
- **CRACKLE**: 0 to 10. Dots are thrown from the speakers into the room (more the louder the track is)
  and wander off their lines. Where one hits a wall, it plays back a few milliseconds of what was playing
  as if off a worn tape: a little slow and wavering, saturated, thin, hissing and grainy.
- **POPPING**: 0 to 10. The same, but fewer and bigger: a tenth of a second of the track, a pop.
- **MIX**: 0 to 100 %. How much of the room is added. The sound itself always stays.

Nothing plays over silence: the crackle and the pops are made from the track itself.

## Its screen

White on black: the room from above, its walls, the two speakers, and you. The rays of sound go from
each speaker into the room and bounce off the walls. There is one ray per frequency band: it's thicker
the louder its band is, and it reaches further the bigger the room and the louder the track. The stereo
balance turns the fans, and the width spreads them. The crackle (small dots) and the pops (dots with a
ring) fly across it.

Cheap: 16 reflections and 4 lines a sample, 12 tape voices at most, no latency. Its screen is only
drawn while it is on.

Code: `DSP/units/RayRoom.h`, `DSP/units/RoomScene.h`, the screen: `UI/Scene/RoomScreen.h`. Tests:
`EnhDspTests --units16`.
