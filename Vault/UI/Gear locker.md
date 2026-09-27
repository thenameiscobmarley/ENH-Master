# Gear locker

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

The rack holds **22U**, like a real studio's rack. Units you aren't using go into the gear locker, and
units from the locker go into the rack if there's room. Right-click the rack → **Gear locker...**

- **STORE** — the unit leaves the rack and stops processing (no CPU). Its settings are kept.
- **INSTALL** — the unit goes back into the rack, at its place in the signal chain. If it doesn't fit,
  it says how much room it needs: store another unit first.
- The POWER strip, the OUTPUT MONITOR and the LUNCHBOX always stay.
- New units start in the locker, so your rack doesn't change until you install them.
- What's in the locker is saved with the session.

Simple view is different: it only hides units, and they keep working.

Code: `UI/GlassPanel.cpp` (the locker page), `UI/Scene/DeviceLayout.h` (`storedUnits`, `rackCapacityU`),
`DSP/ParameterMapping.h` (`withLocker`).
