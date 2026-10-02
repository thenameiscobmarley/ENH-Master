#!/bin/bash
# The whole self-test in one command: every EnhDspTests mode, the router against the real sound server,
# the AudioLab check (every factory preset through every scene, hard rules + the stored baseline) and the
# panel layout audit. One line per job, logs in build/lab/selftest/, exit code 0 only when all of it holds.
#
# It runs as a queue of jobs on every core, longest first (the radar's scenes, the methods, the presets and
# the character models also spread over every core inside their process; the zipper and the mastering groups
# run in shards side by side), and nothing runs twice.
#
#   scripts/selftest.sh [--full | --quick] [--build] [--no-router] [--no-ui] [--jobs N]
#     (none)       the full tests; a job whose code has not changed since it last passed is not run again
#                  (its pass is reused, and says so) - see "What a job depends on" below
#     --full       the full tests, every job run (no reuse): before a release, and after anything unusual
#     --quick      the quick tier, about 3 minutes: every kind of check on less material (two of the radar's
#                  five distances, one block size for the methods, one direction a knob for the zipper, three
#                  of the AudioLab's scenes, shorter fuzzing); passes are reused as in the default
#     --build      build everything first
#     --no-router  skip EnhRouterTests (it moves real audio streams; needs PipeWire / PulseAudio)
#     --no-ui      skip the layout audit (it opens the standalone off screen)
#     --jobs N     how many jobs at once (default: the number of cores)
#
# What a job depends on (a pass is reused only while every file it depends on, the compiler and the job's
# own command are the same as when it passed):
#   rack   (radar, methods, character, presets, core, AudioLab, cpu, limiter, bass, alias, str0): the rack -
#          everything in Source/DSP but the newer units (Source/DSP/units: in these tests they sit in the
#          locker, switched off, and never run), yet with the unit base, the LUNCHBOX's modules and the unit
#          list; Source/Parameters, Shared and Custom; the tests but the newer units' own (NewUnitsTests.h);
#          the AudioLab and its baseline; CMakeLists.txt
#   units  (mastering, zipper, units, fuzz): all of Source/DSP, Parameters, Shared, Custom, the tests, CMakeLists.txt
#   ui     (the layout audit): Source/UI, Source/DSP/units/UnitList.h, Source/Parameters, Tools/units
#   router: always run (a few seconds)

cd "$(dirname "$0")/.." || exit 2
build=0; tier=default; router=1; ui=1; jobs=$(nproc)
while [ $# -gt 0 ]; do
    case "$1" in
        --build) build=1 ;; --full) tier=full ;; --quick) tier=quick ;; --no-router) router=0 ;; --no-ui) ui=0 ;;
        --jobs) shift; jobs=$1 ;;
        *) echo "unknown option $1"; exit 2 ;;
    esac
    shift
done

logs=build/lab/selftest
cache=build/lab/selftest-cache
mkdir -p "$logs" "$cache"
rm -f "$logs"/*.log "$logs"/*.status
T=build/EnhDspTests_artefacts/Release/EnhDspTests
R=build/EnhRouterTests_artefacts/Release/EnhRouterTests
A=build/EnhAudioLab_artefacts/Release/EnhAudioLab
S="build/EnhMaster_artefacts/Release/Standalone/ENH Master"

if [ $build = 1 ]; then
    echo "building ..."
    cmake --build build -j"$(nproc)" > "$logs/build.log" 2>&1 || { echo "BUILD FAILED (see $logs/build.log)"; exit 1; }
fi

audit() {
    rm -rf build/lab/selftest/audit && mkdir -p build/lab/selftest/audit
    PAD_UI_DUMP_ARTWORK=build/lab/selftest/audit PAD_UI_DUMP_QUIT=1 timeout 180 "$S" > /dev/null 2>&1
    local c=build/lab/selftest/audit/clearances.txt
    [ -f "$c" ] || { echo "no audit written"; return 1; }
    local cramped overlaps
    cramped=$(grep "cramped" "$c" | awk '{s+=$1} END{print s+0}')
    overlaps=$(grep -c "clearance -" "$c")
    echo "$cramped cramped print item(s), $overlaps overlapping"
    # Nothing may overlap; the cramped count may not grow past what 3.7.13.13 left (46: 38 on the ten
    # panels 3.6.6.1 audited, 8 on the three it audits since - FOOTSTEP RADAR, POWER, LUNCHBOX)
    # The designed and newer units (clearances-units.txt, since 3.8.0.1): nothing overlapping, at most 12 tight
    local u=build/lab/selftest/audit/clearances-units.txt uo ut
    [ -f "$u" ] || { echo "no unit audit written"; return 1; }
    uo=$(grep -c "clearance -" "$u"); ut=$(tail -1 "$u" | awk '{print $1}')
    echo "units: $ut tight, $uo overlapping"
    [ "$overlaps" = 0 ] && [ "$cramped" -le 46 ] && [ "$uo" = 0 ] && [ "$ut" -le 12 ] && echo "ALL PASSED" || { echo "[FAIL] layout: $overlaps overlapping, $cramped cramped (max 46); units $uo overlapping, $ut tight (max 12)"; grep -h "clearance -" "$c" "$u" | head; return 1; }
}
export -f audit
export S

# --- what each kind of job depends on: a hash of those files (and the compiler) -------------------------------
compiler=$(c++ --version 2>/dev/null | head -1)
hashFiles() { { echo "$compiler"; find "$@" -type f \( -name '*.h' -o -name '*.cpp' -o -name '*.inc' -o -name '*.py' -o -name '*.json' -o -name '*.txt' \) 2>/dev/null | LC_ALL=C sort | xargs sha256sum; } | sha256sum | cut -c1-40; }
declare -A dep
dep[rack]=$(hashFiles $(find Source/DSP -maxdepth 1 -type f) Source/DSP/units/RackUnit.h Source/DSP/units/LbList.h Source/DSP/units/Lb500.h \
            Source/DSP/units/UnitList.h Source/Parameters Source/Shared Source/Custom $(find Tests -type f ! -name NewUnitsTests.h) \
            Tools/AudioLab.cpp CMakeLists.txt)
dep[units]=$(hashFiles Source/DSP Source/Parameters Source/Shared Source/Custom Tests CMakeLists.txt)
dep[ui]=$(hashFiles Source/UI Source/DSP/units/UnitList.h Source/Parameters Tools/units CMakeLists.txt)
dep[always]=$(date +%s%N)   # (never the same: always run)

# --- the jobs: "name|kind|command", longest first -----------------------------------------------------------------
F=""; LF=""; FZ="30"
if [ $tier = quick ]; then F="TEST_FAST=1 "; LF=" --fast"; FZ="10"; fi
list=()
list+=("DSP radar|rack|${F}$T --radar")
list+=("AudioLab check|rack|$A check$LF")
list+=("DSP methods|rack|${F}$T --methods")
list+=("DSP character|rack|${F}$T --character")
for k in 0 1 2; do list+=("DSP mastering $((k+1))/3|units|${F}TEST_SHARD=$k/3 $T --mastering"); done
for k in 0 1 2 3; do list+=("DSP zipper $((k+1))/4|units|${F}TEST_SHARD=$k/4 $T --zipper"); done
list+=("DSP presets|rack|${F}$T --presets")
list+=("DSP core|rack|${F}$T --core")
list+=("DSP fuzz seed 1|units|$T --fuzz $FZ 1")
[ $tier != quick ] && list+=("DSP fuzz seed 7|units|$T --fuzz $FZ 7")
[ $tier != quick ] && list+=("DSP cpu|rack|$T --cpu")
for m in --limiter --bass --alias --str0; do list+=("DSP ${m#--}|rack|${F}$T $m"); done
list+=("DSP units|units|${F}$T --units")
[ $router = 1 ] && list+=("router (real sound server)|always|$R")
[ $ui = 1 ] && [ -x "$S" ] && list+=("UI layout audit|ui|audit")

slug() { echo "$1" | tr ' /()' '____'; }
keyOf() { echo "$tier|$2|${dep[$3]}" | sha256sum | cut -c1-40; }   # keyOf NAME COMMAND KIND

run_job() {   # run_job INDEX NAME COMMAND KEY: runs it, logs it, writes its status (and remembers a pass)
    local i=$1 name=$2 cmd=$3 key=$4
    local log="$logs/$(printf '%02d' "$i")-$(slug "$name").log"
    local t0=$SECONDS
    bash -c "$cmd" > "$log" 2>&1
    local code=$?
    local secs=$((SECONDS - t0))
    echo "$code $secs $log run" > "$logs/$(printf '%02d' "$i").status"
    if [ "$code" = 0 ] && ! grep -q "\[FAIL\]" "$log"; then
        { echo "$key $secs"; cat "$log"; } > "$cache/$(slug "$name").pass"
    else
        rm -f "$cache/$(slug "$name").pass"
    fi
}

echo "ENH Master self-test ($(date '+%Y-%m-%d %H:%M')), $tier tier: ${#list[@]} jobs, $jobs at a time"
start=$SECONDS
reused=0
for i in "${!list[@]}"; do
    IFS='|' read -r name kind cmd <<< "${list[$i]}"
    key=$(keyOf "$name" "$cmd" "$kind")
    pass="$cache/$(slug "$name").pass"
    if [ $tier != full ] && [ -f "$pass" ] && [ "$(head -1 "$pass" | cut -d' ' -f1)" = "$key" ]; then
        # nothing it depends on has changed since it passed: its pass (and its log) stand
        log="$logs/$(printf '%02d' "$i")-$(slug "$name").log"
        tail -n +2 "$pass" > "$log"
        echo "0 $(head -1 "$pass" | cut -d' ' -f2) $log reused" > "$logs/$(printf '%02d' "$i").status"
        reused=$((reused + 1))
        continue
    fi
    while [ "$(jobs -rp | wc -l)" -ge "$jobs" ]; do wait -n; done
    run_job "$i" "$name" "$cmd" "$key" &
done
wait

failed=0
for i in "${!list[@]}"; do
    IFS='|' read -r name kind cmd <<< "${list[$i]}"
    read -r code secs log how < "$logs/$(printf '%02d' "$i").status"
    summary=$(grep -E "ALL PASSED|FAILURES|failure|Everything holds|All router checks passed" "$log" | tail -1)
    if [ "$how" = reused ]; then
        printf "  %-28s PASS  (unchanged since it passed in %ds: reused)\n" "$name" "$secs"
    elif [ "$code" = 0 ] && ! grep -q "\[FAIL\]" "$log"; then
        printf "  %-28s PASS  %4ds  %s\n" "$name" "$secs" "$summary"
    else
        printf "  %-28s FAIL  %4ds  %s   (%s)\n" "$name" "$secs" "$summary" "$log"
        grep -E "\[FAIL\]|RULE |BASELINE " "$log" | head -8 | sed 's/^/        /'
        failed=$((failed + 1))
    fi
done

echo
note=""; [ $reused -gt 0 ] && note=", $reused reused (unchanged)"
if [ $failed = 0 ]; then
    echo "SELF-TEST PASSED ($tier tier: ${#list[@]} jobs$note, $((SECONDS - start)) s)"
else
    echo "SELF-TEST FAILED: $failed of ${#list[@]} jobs ($tier tier$note, $((SECONDS - start)) s)"
fi
[ $failed = 0 ]
