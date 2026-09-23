# Presets

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

A preset sets the whole rack at once. Step through them with **PRESET ◀ ▶** on the black unit, or
from your DAW's program list.

| Preset | For |
|---|---|
| DEFAULT | a balanced starting point |
| COMPETITIVE FOOTSTEPS | PvP: steps and detail forward, explosions don't duck them, no reverb |
| IMMERSIVE GAMES | single-player and cinematic games: fuller low end, space and width |
| NIGHT MODE | quiet listening: quiet sounds up, sudden loud ones held down |
| BASS HEAVY, PROTECTED | big low end without pumping |
| VOICE & STREAMING | speech first: clear, even, no reverb under the voice |
| MUSIC: WARM MASTER | tape-like warmth and a touch of room, dynamics kept |
| MUSIC: WIDE & AIRY | open top, wide image, a lush hall |
| DEEP SUB: SUBMARINE | deep, dark, huge low end with a ringing steel hull |
| MASTERING: ANALOG BUS | a vintage console into tape, gently driven |
| GAME: ARENA | footsteps and callouts forward, impact without mud |
| TRANSPARENT (ALL OUT) | everything off, for comparing (the safety limiter stays) |

## Edit them without rebuilding

Presets live in a text file, which the plugin creates the first time it runs:

| System | File |
|---|---|
| Linux | `~/.config/ENH Master/presets.json` |
| Windows | `%APPDATA%\ENH Master\presets.json` |
| any | set `ENH_MASTER_PRESETS` to use another file |

- Edit it and press PRESET ◀ ▶: changes load at once.
- The file lists every setting's range at the top.
- Bad values are clamped, unknown names skipped; a broken file is ignored.
- Delete the file to get the factory presets back (they're in `Source/Parameters/FactoryPresets.h`).

Check your edited file: `EnhDspTests --presets`.
