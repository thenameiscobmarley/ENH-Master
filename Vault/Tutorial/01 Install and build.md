# 01 Install and build

> 🔎 **[Searchbar](../../Searchbar.md)** — find any doc, setting, function or GitHub page (Ctrl+F)

## Just install it

Download the zip for your system from
[Releases](https://github.com/thenameiscobmarley/ENH-Master/releases/latest), then copy
`ENH Master.vst3` into your VST3 folder:

- Windows: `C:\Program Files\Common Files\VST3`
- Linux: `~/.vst3`

Rescan plugins in your DAW. In Carla: *Add Plugin → Refresh → VST3 → ENH Master*.

## Build it (Linux)

```sh
git clone https://github.com/thenameiscobmarley/ENH-Master
cd ENH-Master
./build.sh
```

`build.sh` checks your tools, downloads [JUCE](https://github.com/juce-framework/JUCE) and
[HardwareKit](https://github.com/thenameiscobmarley/HardwareKit) if needed, then asks:

- **how many CPU cores** (fewer uses less memory), and
- **replace** the installed plugin, or build a separate **copy** into `dist/`.

Skip the questions with `./build.sh --jobs all --mode replace`. See `./build.sh --help`.

<details><summary>By hand</summary>

```sh
git clone https://github.com/juce-framework/JUCE ~/JUCE
git clone https://github.com/thenameiscobmarley/HardwareKit ../HardwareKit
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Other paths: `-DJUCE_PATH=…`, `-DHARDWAREKIT_PATH=…`. Build without installing: `-DENH_COPY_PLUGIN=OFF`.
</details>

## Build it (Windows)

Easiest: download `convert-to-windows.bat` from the repo and double-click it
([Windows and other platforms](06%20Windows%20and%20other%20platforms.md)). By hand, with Visual
Studio 2022, CMake and Git:

```bat
git clone https://github.com/juce-framework/JUCE ..\JUCE
git clone https://github.com/thenameiscobmarley/HardwareKit ..\HardwareKit
cmake -S . -B build -A x64 -DJUCE_PATH=..\JUCE -DENH_COPY_PLUGIN=OFF
cmake --build build --config Release --parallel
```

> [!tip] Low on memory?
> Each build job can use about 1 GB. On a small machine use 1–2 cores.

## Check it works

```sh
scripts/selftest.sh
```

It runs every test and ends with `SELF-TEST PASSED`. More in [Testing and tools](../Reference/Audio%20lab.md).

Next: [Your first sound](02%20Your%20first%20sound.md)
