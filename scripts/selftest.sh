#!/bin/bash
# The whole self-test in one command: every EnhDspTests mode, the router against the real sound server,
# the AudioLab check (every factory preset through every scene, hard rules + the stored baseline) and the
# panel layout audit. One line per step, logs in build/lab/selftest/, exit code 0 only when all of it holds.
#
#   scripts/selftest.sh [--build] [--quick] [--no-router] [--no-ui]
#     --build      build everything first (2 jobs: the laptop runs out of memory with more)
#     --quick      skip the slow steps (full suite, methods, fuzz, AudioLab check)
#     --no-router  skip EnhRouterTests (it moves real audio streams; needs PipeWire / PulseAudio)
#     --no-ui      skip the layout audit (it opens the standalone off screen)

cd "$(dirname "$0")/.." || exit 2
build=0; quick=0; router=1; ui=1
for a in "$@"; do
    case "$a" in
        --build) build=1 ;; --quick) quick=1 ;; --no-router) router=0 ;; --no-ui) ui=0 ;;
        *) echo "unknown option $a"; exit 2 ;;
    esac
done

logs=build/lab/selftest
mkdir -p "$logs"
T=build/EnhDspTests_artefacts/Release/EnhDspTests
R=build/EnhRouterTests_artefacts/Release/EnhRouterTests
A=build/EnhAudioLab_artefacts/Release/EnhAudioLab
S="build/EnhMaster_artefacts/Release/Standalone/ENH Master"

if [ $build = 1 ]; then
    echo "building ..."
    cmake --build build -j"$(nproc)" > "$logs/build.log" 2>&1 || { echo "BUILD FAILED (see $logs/build.log)"; exit 1; }
fi

failed=0
results=()
cr=""; [ -t 1 ] && cr="\r"   # on a terminal, the PASS / FAIL line replaces the "..." line
step() {   # step NAME COMMAND...: runs it, logs it, records PASS / FAIL and how long it took
    local name=$1; shift
    local log="$logs/$(echo "$name" | tr ' /' '__').log"
    local t0=$SECONDS
    [ -t 1 ] && printf "  %-28s ..." "$name"
    "$@" > "$log" 2>&1
    local code=$?
    local secs=$((SECONDS - t0))
    local summary
    summary=$(grep -E "ALL PASSED|FAILURES|failure|Everything holds|All router checks passed" "$log" | tail -1)
    if [ $code = 0 ] && ! grep -q "\[FAIL\]" "$log"; then
        printf "$cr  %-28s PASS  %4ds  %s\n" "$name" $secs "$summary"
        results+=("PASS  $name")
    else
        printf "$cr  %-28s FAIL  %4ds  %s   (%s)\n" "$name" $secs "$summary" "$log"
        grep -E "\[FAIL\]|RULE |BASELINE " "$log" | head -8 | sed 's/^/        /'
        results+=("FAIL  $name")
        failed=$((failed + 1))
    fi
}


echo "ENH Master self-test ($(date '+%Y-%m-%d %H:%M'))"
[ $quick = 0 ] && step "DSP full suite" "$T"
[ $quick = 0 ] && step "DSP methods" "$T" --methods
for m in --character --mastering --presets --limiter --zipper --alias --bass --units --str0; do
    step "DSP ${m#--}" "$T" $m
done
[ $quick = 0 ] && step "DSP fuzz (60 s, 2 seeds)" bash -c "'$T' --fuzz 30 1 && '$T' --fuzz 30 7"
[ $router = 1 ] && step "router (real sound server)" "$R"
[ $quick = 0 ] && step "AudioLab check" "$A" check

if [ $ui = 1 ] && [ -x "$S" ]; then
    audit() {
        rm -rf build/lab/selftest/audit && mkdir -p build/lab/selftest/audit
        PAD_UI_DUMP_ARTWORK=build/lab/selftest/audit timeout 180 "$S" > /dev/null 2>&1
        local c=build/lab/selftest/audit/clearances.txt
        [ -f "$c" ] || { echo "no audit written"; return 1; }
        local cramped overlaps
        cramped=$(grep "cramped" "$c" | awk '{s+=$1} END{print s+0}')
        overlaps=$(grep -c "clearance -" "$c")
        echo "$cramped cramped print item(s), $overlaps overlapping"
        # Nothing may overlap; the cramped count may not grow past what 3.7.13.13 left (46: 38 on the ten
        # panels 3.6.6.1 audited, 8 on the three it audits since - FOOTSTEP RADAR, POWER, LUNCHBOX)
        [ "$overlaps" = 0 ] && [ "$cramped" -le 46 ] && echo "ALL PASSED" || { echo "[FAIL] layout: $overlaps overlapping, $cramped cramped (max 46)"; grep "clearance -" "$c" | head; return 1; }
    }
    step "UI layout audit" audit
fi

echo
if [ $failed = 0 ]; then
    echo "SELF-TEST PASSED (${#results[@]} steps)"
else
    echo "SELF-TEST FAILED: $failed of ${#results[@]} steps"
fi
[ $failed = 0 ]
