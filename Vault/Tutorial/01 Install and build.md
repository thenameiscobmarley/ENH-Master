# 01 Install and build

You need three things next to each other: **JUCE 8**, **HardwareKit** (the shared UI module) and this
plugin. A C++20 compiler, CMake and Ninja do the rest.

```sh
git clone https://github.com/juce-framework/JUCE ~/JUCE          # JUCE 8
git clone <your-fork>/HardwareKit  ~/Projects/HardwareKit        # shared 3D UI module
git clone <your-fork>/ENH-Master   ~/Projects/PvPAdaptiveDynamics
cd ~/Projects/PvPAdaptiveDynamics

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j1
```

Two paths are overridable if your checkouts live elsewhere: `-DJUCE_PATH=…` (default `~/JUCE`) and
`-DHARDWAREKIT_PATH=…` (default `../HardwareKit`).

> [!tip] Why `-j1`
> JUCE translation units are large; each parallel job wants roughly a gigabyte. On a small box
> (this was developed on an Intel J4105 with 3.7 GB) `-j1` is the difference between a slow build and
> a machine that swaps itself to death. On a big machine use `-j$(nproc)`.

The build installs `~/.vst3/ENH Master.vst3` by itself and also produces a standalone app at
`build/EnhMaster_artefacts/Release/Standalone/ENH Master`, which is the quickest way to look at it.

## Check it works

```sh
build/EnhDspTests_artefacts/Release/EnhDspTests
```

This runs the offline DSP suite - detection accuracy against synthetic scenes, EQ behaviour, harmonic
generation, stability at 44.1/48/96 kHz and odd block sizes - and prints a CPU benchmark. It should
end with `ALL PASSED (0 failures)`. See [[Dev hooks]] for its other modes.

## Load it in a host

Carla: *Add Plugin → Refresh → VST3 → ENH Master*. Anything that hosts VST3 on Linux works;
see [[06 Windows and other platforms]] for other systems.

Next: [[02 Your first sound]]
