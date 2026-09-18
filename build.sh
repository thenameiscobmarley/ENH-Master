#!/usr/bin/env bash
# ==============================================================================
#  ENH Master builder - builds the plugin from this checkout, for people who want
#  to use it or edit it.
#
#  Run it and answer the questions:     ./build.sh
#  Or answer them up front with flags:  ./build.sh --jobs all --mode replace
#
#    --jobs 1|all|N     how many CPU cores the compiler may use
#    --mode replace     install over ~/.vst3/ENH Master.vst3 (the one your DAW loads)
#    --mode copy        put a separate copy in dist/, leave the installed plugin alone
#    --tests            run the offline DSP tests after building
#    --clean            delete the build folder first (a from-scratch build)
#    -y, --yes          don't ask; use the defaults (all cores, replace) for anything not given
#    -h, --help         this text
#
#  Environment: JUCE_PATH (default ~/JUCE), HARDWAREKIT_PATH (default ../HardwareKit).
# ==============================================================================
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$here"

bold=$'\e[1m'; dim=$'\e[2m'; red=$'\e[31m'; green=$'\e[32m'; yellow=$'\e[33m'; off=$'\e[0m'
[ -t 1 ] || { bold=""; dim=""; red=""; green=""; yellow=""; off=""; }

say()  { printf '%s\n' "$*"; }
warn() { printf '%s\n' "${yellow}$*${off}" >&2; }
die()  { printf '%s\n' "${red}$*${off}" >&2; exit 1; }

usage() { sed -n '3,17p' "$0" | sed -E 's/^# ? ?//'; exit 0; }

jobs_arg="" mode="" run_tests=0 clean=0 assume_yes=0
while [ $# -gt 0 ]; do
    case "$1" in
        --jobs)  jobs_arg="${2:-}"; shift ;;
        --jobs=*) jobs_arg="${1#*=}" ;;
        --mode)  mode="${2:-}"; shift ;;
        --mode=*) mode="${1#*=}" ;;
        --tests) run_tests=1 ;;
        --clean) clean=1 ;;
        -y|--yes) assume_yes=1 ;;
        -h|--help) usage ;;
        *) die "unknown option: $1 (see ./build.sh --help)" ;;
    esac
    shift
done

# Questions go to the terminal; with no terminal (or --yes) the defaults are used
interactive=0
if [ "$assume_yes" -eq 0 ] && [ -r /dev/tty ] && [ -t 0 ]; then interactive=1; fi

ask() {   # ask "question" default -> answer
    local answer=""
    if [ "$interactive" -eq 1 ]; then
        read -r -p "$1 [$2]: " answer < /dev/tty || true
    fi
    printf '%s' "${answer:-$2}"
}

confirm() {   # confirm "question" Y|N -> 0 for yes
    local a
    a="$(ask "$1 (y/n)" "$2")"
    case "$a" in [Yy]*) return 0 ;; *) return 1 ;; esac
}

version="$(sed -n 's/^project(EnhMaster VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
cores="$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)"

say ""
say "${bold}ENH Master ${version} - builder${off}"
say "${dim}$(pwd)${off}"
say ""

# ------------------------------------------------------------------------------
# Tools and libraries
# ------------------------------------------------------------------------------
missing=()
command -v cmake >/dev/null || missing+=("cmake")
command -v git   >/dev/null || missing+=("git")
if ! command -v c++ >/dev/null && ! command -v g++ >/dev/null && ! command -v clang++ >/dev/null; then
    missing+=("a C++ compiler (g++ or clang++)")
fi
if [ ${#missing[@]} -gt 0 ]; then
    die "Missing: ${missing[*]}
Debian / Ubuntu: sudo apt install build-essential cmake git ninja-build
Arch / CachyOS:  sudo pacman -S base-devel cmake git ninja"
fi

if command -v cmake >/dev/null; then
    cmake_version="$(cmake --version | sed -n 's/^cmake version \([0-9]*\.[0-9]*\).*/\1/p')"
    if [ "$(printf '%s\n' 3.22 "$cmake_version" | sort -V | head -1)" != "3.22" ]; then
        die "CMake $cmake_version is too old: 3.22 or newer is needed."
    fi
fi

if command -v pkg-config >/dev/null; then
    libs_missing=()
    for lib in x11 xrandr xinerama xcursor freetype2 alsa gl; do
        pkg-config --exists "$lib" 2>/dev/null || libs_missing+=("$lib")
    done
    if [ ${#libs_missing[@]} -gt 0 ]; then
        warn "These development libraries were not found: ${libs_missing[*]}"
        warn "  Debian / Ubuntu: sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \\"
        warn "                   libxext-dev libxcomposite-dev libfreetype6-dev libasound2-dev libglu1-mesa-dev"
        warn "  Arch / CachyOS:  sudo pacman -S libx11 libxrandr libxinerama libxcursor freetype2 alsa-lib mesa"
        confirm "Try to build anyway?" Y || exit 1
    fi
fi

generator=()
if [ -f build/CMakeCache.txt ]; then
    : # keep whatever generator the existing build folder was made with
elif command -v ninja >/dev/null; then
    generator=(-G Ninja)
fi

# ------------------------------------------------------------------------------
# JUCE and HardwareKit
# ------------------------------------------------------------------------------
juce="${JUCE_PATH:-$HOME/JUCE}"
hwk="${HARDWAREKIT_PATH:-$here/../HardwareKit}"

if [ ! -f "$juce/CMakeLists.txt" ]; then
    say "JUCE (the framework the plugin is built on) was not found at $juce."
    if confirm "Download it there now?" Y; then
        git clone --depth 1 https://github.com/juce-framework/JUCE.git "$juce"
    else
        die "Set JUCE_PATH to your JUCE folder and run again."
    fi
fi

if [ ! -d "$hwk/modules/hardwarekit" ]; then
    say "HardwareKit (the 3D hardware UI library) was not found at $hwk."
    if confirm "Download it there now?" Y; then
        git clone --depth 1 https://github.com/thenameiscobmarley/HardwareKit.git "$hwk"
    else
        die "Set HARDWAREKIT_PATH to your HardwareKit folder and run again."
    fi
fi
hwk="$(cd "$hwk" && pwd)"

# ------------------------------------------------------------------------------
# How many cores
# ------------------------------------------------------------------------------
if [ -z "$jobs_arg" ]; then
    say "${bold}How many CPU cores should the build use?${off} This computer has ${cores}."
    say "  1) 1 core           slowest, lightest on memory"
    say "  2) all ${cores} cores      fastest"
    say "  3) a specific number"
    case "$(ask "Choose" 2)" in
        1) jobs_arg=1 ;;
        3) jobs_arg="$(ask "How many (1-${cores})" "$cores")" ;;
        *) jobs_arg=all ;;
    esac
    say ""
fi
case "$jobs_arg" in
    all|ALL|max) jobs="$cores" ;;
    ''|*[!0-9]*) die "--jobs must be 1, all, or a number (got '$jobs_arg')" ;;
    *) jobs="$jobs_arg" ;;
esac
[ "$jobs" -ge 1 ] || jobs=1
if [ "$jobs" -gt "$cores" ]; then
    warn "Only ${cores} cores here; using ${cores}."
    jobs="$cores"
fi

# Each compiler job can take up to ~1 GB on the big JUCE files. Say so if memory + swap looks tight.
if [ "$jobs" -gt 1 ] && [ -r /proc/meminfo ]; then
    mem_mb=$(( ( $(awk '/^MemTotal/ {print $2}' /proc/meminfo) + $(awk '/^SwapTotal/ {print $2}' /proc/meminfo) ) / 1024 ))
    if [ "$mem_mb" -lt $(( jobs * 1100 )) ]; then
        warn "${jobs} jobs may run short of memory (${mem_mb} MB RAM + swap). If the build fails, run again with fewer cores."
    fi
fi

# ------------------------------------------------------------------------------
# Replace or copy
# ------------------------------------------------------------------------------
case "$(uname -s)" in
    Darwin) vst3_dir="$HOME/Library/Audio/Plug-Ins/VST3" ;;
    *)      vst3_dir="$HOME/.vst3" ;;
esac

if [ -z "$mode" ]; then
    say "${bold}Install it as a replacement or as a copy?${off}"
    say "  1) replace   install over ${vst3_dir}/ENH Master.vst3 (the one your DAW loads)"
    say "  2) copy      put a separate copy in dist/ and leave the installed plugin alone"
    case "$(ask "Choose" 1)" in
        2|copy) mode=copy ;;
        *) mode=replace ;;
    esac
    say ""
fi
case "$mode" in
    replace|copy) ;;
    *) die "--mode must be replace or copy (got '$mode')" ;;
esac

if [ "$clean" -eq 1 ] && [ -d build ]; then
    if [ "$interactive" -eq 0 ] || confirm "Delete the build folder and build from scratch?" Y; then
        rm -rf build
        generator=()
        command -v ninja >/dev/null && generator=(-G Ninja)
    fi
fi

# ------------------------------------------------------------------------------
# Build
# ------------------------------------------------------------------------------
copy_flag=ON
[ "$mode" = copy ] && copy_flag=OFF

say "${bold}Building${off} with ${jobs} core$([ "$jobs" -eq 1 ] || echo s), mode: ${mode}"
say "${dim}JUCE: ${juce}   HardwareKit: ${hwk}${off}"
say ""

mkdir -p build
log=build/builder.log
: > "$log"
cmake -S . -B build ${generator[@]+"${generator[@]}"} -DCMAKE_BUILD_TYPE=Release \
      -DJUCE_PATH="$juce" -DHARDWAREKIT_PATH="$hwk" -DENH_COPY_PLUGIN="$copy_flag" > "$log" 2>&1 \
    || { tail -30 "$log" >&2; die "Configuring failed; the full log is in $log"; }

start=$(date +%s)
build_once() {
    cmake --build build --config Release --parallel "$1" 2>&1 | tee -a "$log"
    return "${PIPESTATUS[0]}"
}
if ! build_once "$jobs"; then
    if [ "$jobs" -gt 1 ] && grep -qE "Killed signal|out of memory|cannot allocate memory" "$log"; then
        fewer=$(( jobs / 2 ))
        warn "The compiler ran out of memory with ${jobs} jobs."
        if confirm "Try again with ${fewer}?" Y; then
            build_once "$fewer" || die "The build failed; see $log"
        else
            die "The build failed; see $log"
        fi
    else
        die "The build failed; see $log"
    fi
fi
elapsed=$(( $(date +%s) - start ))

bundle="build/EnhMaster_artefacts/Release/VST3/ENH Master.vst3"
standalone="build/EnhMaster_artefacts/Release/Standalone/ENH Master"
[ -d "$bundle" ] || die "The build finished but there is no VST3 at $bundle"

say ""
say "${green}${bold}Built ENH Master ${version} in $((elapsed / 60)) min $((elapsed % 60)) s.${off}"
if [ "$mode" = replace ]; then
    say "  VST3 installed:  ${vst3_dir}/ENH Master.vst3  (rescan plugins in your DAW)"
else
    dest="dist/ENH-Master-${version}-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$dest"
    cp -r "$bundle" "$dest/"
    [ -f "$standalone" ] && cp "$standalone" "$dest/"
    say "  Copy:            ${here}/${dest}/"
    say "  The installed plugin was not touched. The copy has the same plugin ID, so to try it in a DAW"
    say "  move it into ${vst3_dir} in place of the installed one (not next to it)."
fi
[ -f "$standalone" ] && say "  Standalone app:  ${here}/${standalone}"

if [ "$run_tests" -eq 1 ]; then
    say ""
    say "${bold}Running the offline DSP tests${off}"
    build/EnhDspTests_artefacts/Release/EnhDspTests
fi
