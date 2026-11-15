#!/bin/bash
# Differential gate driver for one phase of the P8-12 increment.
#
#   run.sh A   every native body still present (runtime switch: Goose default,
#              env selects the native arm)
#   run.sh B   the MIME and Cache-Control native bodies deleted: ON is Goose-only
#              for those two, the OFF build is the unit-isolation build and must
#              answer the documented stubs
#
# Every run is a no-server run: the gate links the shipped decision functions,
# the shipped bridge and the shipped unit, and nothing else.
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
phase="${1:-A}"
log="$root/audit/gate-$phase.log"
mkdir -p "$root/audit"
: > "$log"
fails=0
passes=0

run() { # <label> <command...>
    local label="$1"; shift
    local out rc cases div
    out=$("$@" 2>&1); rc=$?
    echo "### $label  [rc=$rc]" >> "$log"
    printf '%s\n' "$out" >> "$log"
    cases=$(printf '%s\n' "$out" | tail -1 | sed -n 's/.*cases=\([0-9]*\).*/\1/p')
    div=$(printf '%s\n' "$out" | tail -1 | sed -n 's/.*divergences=\([0-9]*\).*/\1/p')
    if [ $rc -eq 0 ]; then
        passes=$((passes + 1))
        echo "PASS  $label  (${cases:-?} cases, ${div:-?} divergences)"
    else
        fails=$((fails + 1))
        echo "FAIL  $label  (${cases:-?} cases, ${div:-?} divergences)"
        printf '%s\n' "$out" | grep -v 'OK$' | tail -5
    fi
}

run "build gate binaries" "$root/gate/build.sh"

ARGS_REF="--status ref --cors ref --mime ref --cache ref --tools ref --tools-raw ref --mime-raw ref --cache-raw ref"
ARGS_NATIVE="--status ref --cors ref --mime ref --cache ref --tools ref --native"
ARGS_OFF_A="--status unknown --cors ref --mime ref --cache ref --tools ref --tools-raw none --mime-raw none --cache-raw none"
ARGS_OFF_B="--status unknown --cors ref --mime stub --cache stub --tools ref --tools-raw none --mime-raw none --cache-raw none"

run "ON, Goose arm, explicit goose_init"      "$root/gate/gate-on" $ARGS_REF
run "ON, Goose arm, bridge lazy goose_init"   env FG_GATE_NO_EARLY=1 "$root/gate/gate-on" $ARGS_REF

if [ "$phase" = "A" ]; then
    run "ON, native arms (four env switches)" "$root/gate/gate-on" $ARGS_NATIVE
    run "OFF bridge: status stub, other native arms" "$root/gate/gate-off" $ARGS_OFF_A
else
    run "ON, native arms (Origin + tools only)" "$root/gate/gate-on" $ARGS_NATIVE
    run "OFF bridge: stubs for mime and Cache-Control" "$root/gate/gate-off" $ARGS_OFF_B
    if [ -x "$root/gate/gate-default" ]; then
        run "default configure, Goose arm"        "$root/gate/gate-default" $ARGS_REF
    fi
fi

echo "gate phase $phase: $passes passed, $fails failed; log $log"
[ $fails -eq 0 ]
