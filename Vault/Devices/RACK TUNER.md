# AUTOMATIC RACK TUNER

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

Formerly called RACK TUNER.

Tunes the whole rack from a few words (3U, model RT-60K). It starts in the [Gear locker](../UI/Gear%20locker.md),
under Meters and tools. Its own sound is untouched.

Click it to open its glass panel. Type what you want, like "warm punchy hip-hop", "huge dreamy space" or "clean
podcast voice", or click the words under the box. Then press Enter, click **Tune the rack**, or flip **TUNE**
on the faceplate.

It knows about 480 words and phrases: plain ones (warm, crisp, muddy, huge), genres (hip-hop, shoegaze, drill,
bossa), instruments, and uses (podcast, gaming, movie, car). Each word is a mix of twelve tags: warm, bright, deep,
punch, loud, space, wide, dirty, smooth, vintage, motion and clarity. "very", "a little" and "no" change the word
after them. The panel says what it understood ("more warm, more punch, more deep").

It holds 60,000 settings for every unit that changes the sound: the rack's own units (their switches and choices
too, like TONE & SPACE's AUTO and MODE, or CHARACTER's models), FOOTSTEP RADAR, PRO X4, VELVETIZER, TAKEBACK,
the newer units and all of the LUNCHBOX's modules. Only the screens (PHOSPHOR SCOPE, HYPERCUBE), LEVEL CONTROL and the
playback checks are left out. What each unit and
knob does was measured, not guessed: the AudioLab played music, a steady chord, a sine and silence through the
real rack, turned every knob, and measured the change in brightness, lows, width, tail, loudness, punch,
distortion and movement. Each setting is one unit's answer to a mix of words, its knobs moved the way they were
measured to go. A tune picks the setting for every unit that fits your words best. No AI is used and nothing
leaves your computer.

The same words give a different rack each time, as far as VARIETY allows. At 0 it gives the best fit every time.

The screen shows the 60,000 as small cubes in a turning cloud, one cluster per unit. A tune sweeps through
them, and the chosen ones light up and send a beam down to the rack.

- **VARIETY**: 0 to 10 (3 to start). At 0 it picks the best fit every time. Higher, it picks among the settings
  nearly as good (up to 40 % less good at 10) and lets up to 7 units play.
- **AMOUNT**: 0 to 10. How far the knobs move from their defaults. 7 is as the settings were made.
- **CHANGE KNOBS**: on, it may move knobs.
- **UNITS ON/OFF**: on, it may switch units on and off.
- **SWAP UNITS**: on, it may bring units in from the locker and put unused ones away (if they fit).
- **LEVEL MATCH**: on, the rack stays as loud after a tune as before it, so you hear the change, not "louder".
  It works while the unit is on.
- **TUNE**: tunes to the words in the panel.
- **UNDO**: back to the rack before the last tune. It keeps 32.
- **A/B**: flips between the rack before the last tune and after it.

The knobs glide to their new places over a second. A big unit like TONE & SPACE or PRO X4 can have several knobs
moved at once. The rack's own units are never put away, and only switched off if the words go against them (and
UNITS ON/OFF is on).

Code: `Tuner/TunerCore.h` (the words, the bank, the tune), `Tuner/TunerWords.inc` (the words),
`Tuner/TunerMeasured.inc` (made by `EnhAudioLab tunerbank`), `UI/RackTuner.cpp`, `DSP/TunerMatch.h` (the level
match). `EnhAudioLab tunerverify` plays 16 of each unit's settings and records how far each unit can be trusted;
`EnhAudioLab tunercheck` tunes a list of words, plays the rack and checks it went the way they asked.

It works best with room in the rack: a full rack leaves it only the units already in. With SWAP UNITS on and
no room, it says so; store a unit or two to let it bring the right ones in.
