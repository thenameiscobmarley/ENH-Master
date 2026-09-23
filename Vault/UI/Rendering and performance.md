# Rendering and performance

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

It's built to run on a cheap mini PC (Intel J4105 with UHD 600 graphics).

## How it stays light

- OpenGL 3.2, one simple shader per material, low-poly shapes uploaded once.
- Soft shadows are calculated, not rendered from shadow maps.
- Its own anti-aliasing, so it looks the same in every host.
- Drawing follows the screen's refresh while you interact, half as often when idle, **10 times a
  second when another program (like a game) is in front**, and **not at all** when the window is hidden
  or minimised. Audio never stops.

## Input

The mouse is read once per frame, straight from the system. Clicks only count when the plugin
window is really on top — otherwise clicking another app over it could turn its knobs.

## Numbers

- Sound: about a quarter of one J4105 core for the whole rack (`EnhDspTests --cpu`).
- Picture: about 57–60 frames a second for the whole rack.

Related: [HardwareKit](HardwareKit.md), [Dev hooks](../Reference/Dev%20hooks.md).
