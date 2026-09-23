# Inside TONE and SPACE

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Code: `Source/DSP/Seraph.h/.cpp` (`SilkStage` = TONE, `HaloStage` = SPACE).

## TONE

- **SMOOTH** finds spots that are ringing *right now* and dips them there, gently.
- **AIR / WARMTH / BODY** are shelves and bells whose amount follows the sound.
- **TAPE** and the valve colour are *added* to the sound, so turning them up adds warmth instead of replacing the sound.
- **PROTECT** keeps hits sharp by letting go of the dips the instant a hit starts.
- **MATCH** keeps the volume the same.
- It all runs at double sample rate (linear-phase), so bright sounds don't turn harsh.

## SPACE

- **Early reflections** (7–37 ms) make it sound like a room, not a wash.
- **Tail** — an 8-line reverb; DECAY is the time, TONE the colour, MOD keeps it moving.
- **SHIMMER** — an octave up, fed into the tail.
- **WIDTH / BASS MONO** — mid/side with a clean crossover at 120 Hz. Out-of-phase bass is folded to the
  middle, not removed.
- **DUCK** — the room steps back while the sound is loud.

## Meters

Every effect has a left and right activity meter; that's what the screen's bars show.

Related: [TONE and SPACE](../Devices/TONE%20and%20SPACE.md).
