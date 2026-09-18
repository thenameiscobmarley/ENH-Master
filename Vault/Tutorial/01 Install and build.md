# 01 Install and build

You need three things next to each other: **JUCE 8**, **HardwareKit** (the shared UI module) and this
plugin. A C++20 compiler, CMake and Ninja do the rest.

The quickest way is the builder script, run from the plugin's folder. It checks your tools and offers
to download JUCE and HardwareKit if they're missing. Then it asks how many CPU cores to use (1, all,
or a number) and whether to **replace** the installed plugin or build a separate **copy** into `dist/`:

```sh
git clone https://github.com/thenameiscobmarley/ENH-Master ~/Projects/PvPAdaptiveDynamics
cd ~/Projects/PvPAdaptiveDynamics
./build.sh                       # or: ./build.sh --jobs all --mode replace
```

By hand:

```sh
git clone https://github.com/juce-framework/JUCE ~/JUCE          # JUCE 8
git clone https://github.com/thenameiscobmarley/HardwareKit ~/Projects/HardwareKit
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Two paths are overridable if your checkouts live elsewhere: `-DJUCE_PATH=…` (default `~/JUCE`) and
`-DHARDWAREKIT_PATH=…` (default `../HardwareKit`). `-DENH_COPY_PLUGIN=OFF` builds without installing.

> [!tip] Cores and memory
> JUCE translation units are large; each parallel job can want about a gigabyte. With enough swap,
> all cores is fine even on a small machine (this was developed on an Intel J4105 with 3.7 GB RAM
> and 16 GB of swap). If a job gets killed for memory, use fewer cores; `build.sh` offers to retry
> with half as many.

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
