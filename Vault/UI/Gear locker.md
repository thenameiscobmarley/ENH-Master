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

## Categories and previews

The **Locker** tab is sorted into categories that fold out: click one to open it. Each has sections,
for example **Low end**: Sub bass, Simulated sub bass, Bass control. There are also Dynamics, EQ and tone,
Filters, Saturation and colour, Space, Modulation and movement, Stereo and phase, Simulated, Mastering,
Lo-fi and character, Meters and tools, and Designed units. A unit can be in more than one category.
The number on a category is how many units are in it. The **In the rack** tab stays in signal order.

Every unit shows a picture of its faceplate under its name. Point at it and it grows to the panel's width.

## Search

The search field at the top finds units and modules by name, model, what they do or the category they are in, on every tab. Searching opens every category with a match. Each
tab's count shows how many match. Esc clears it.

## 500 series

The third tab is the LUNCHBOX's own locker: its 500-series modules, in signal order. The frame has 10
slots (most modules take 1; the CLASS-A EQ, 550 EQ and BUS COMP take 2, the TUBE EQ 3) and its OUTPUT meter always stays
in. A module in the locker takes no slot and no CPU. See [LUNCHBOX](../Devices/LUNCHBOX.md).
