# 07 The router app (for gamers)

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The standalone app (`ENH Master.exe` on Windows) is the rack plus a **router**. It slots the rack
between your games and your headset, and takes it out again. You never touch your sound settings.

```
 SOURCE          RACK INPUT                  LISTEN ON        LEVEL
 [Whole system]  [ENH Master rack input]     [Headphones]     [-18 LUFS]   A/B   [ INSERT RACK ]  [...]
```

## Use it

The steps are numbered in the bar, and the line under it always says what to do next. The **?**
button shows the steps again.

1. **LISTEN ON** — your headset or speakers.
2. **SOURCE** — *Whole system* (everything) or *Chosen apps* (tick the game, Discord…).
3. **INSERT RACK**. Press **REMOVE** (or close the app) and everything goes back exactly as it was.

- **LEVEL** makes games, music and calls the same loudness (*-18 LUFS* is a good choice).
- **A/B** lets you hear the sound without the rack, at the same loudness.
- **IN / OUT** meters show sound arriving and leaving.
- **DELAY** (while the rack is in) says how much later you hear everything: the rack's own 12 ms
  (look-ahead, so steps and blasts are caught in time) plus the sound card's buffers. Hover it for
  the parts. Under 30 ms nobody notices in a game; over 50 ms, lower the buffer size in
  **… > Audio settings** (128 is fast, if your computer keeps up). The app opens at 48 kHz with a
  256-sample buffer unless you chose otherwise.

## The … menu

- insert the rack when the app starts
- keep running in the tray when the window closes (on by default)
- start with the computer
- refresh devices, and the full audio settings

## Windows: one thing to set up

Install **VB-Audio Cable** (free, [vb-audio.com/Cable](https://vb-audio.com/Cable/)). Under **RACK INPUT**
it shows as **Use VB-Audio Cable (Recommended)** and is picked for you. Until it's installed, choosing
that entry opens its download page. Windows plays to the cable, the rack picks it up, and you hear the
result on LISTEN ON. (An output with nothing plugged in, like HDMI, works too.)

## Linux

Works with PipeWire or PulseAudio. The app makes its own "ENH Master rack input" and removes it again.

## If something goes wrong

If the app crashes, a small watchdog puts your audio back within half a second. After a power cut,
the next start does it. Unplug your headset and the app takes the rack out by itself.

Back to [Start here](../00%20Start%20Here.md)
