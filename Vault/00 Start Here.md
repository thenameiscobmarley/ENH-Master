# ENH Master - Start Here

This is an [Obsidian](https://obsidian.md) vault documenting **ENH Master**, a JUCE/VST3 plugin for
game audio: a two-unit rack that makes footsteps and detail audible without wrecking the mix, plus
**SERAPH**, a finishing processor that makes the result sound wide and beautiful.

**To open it:** Obsidian → *Open folder as vault* → pick this `Vault` folder. Every note links to the
others; press `Ctrl+G` for the graph view.

## The tutorial, in order

1. [[01 Install and build]] - get it compiled and installed
2. [[02 Your first sound]] - load it, what the two units do, what to turn
3. [[03 Tuning for footsteps]] - the part people actually came for
4. [[04 Making it sound heavenly]] - SERAPH in practice
5. [[05 Reading the panel]] - the loupe, meters and live displays
6. [[06 Windows and other platforms]] - what works where

## The two devices

- [[ENH Master unit]] - clarity, sub, footstep priority (lower, black)
- [[SERAPH unit]] - silk & halo (upper, purple)

## How it works inside

- [[Footstep detection]] - why a crate lid is not a footstep
- [[Adaptive EQ]] - a curve derived from the audio, not a preset smile
- [[Harmonics and clarity]] - generating detail instead of only boosting it
- [[Seraph stages]] - SILK and HALO, stage by stage
- [[Parameter mapping]] - how MULTIPLY and STRENGTH reach every knob

## The look

- [[HardwareKit]] - the shared 3D hardware library
- [[The loupe]] - the fisheye magnifier
- [[Rendering and performance]] - how it stays cheap

## Reference

- [[Parameters]] · [[Dev hooks]] · [[Backups and restore]]
