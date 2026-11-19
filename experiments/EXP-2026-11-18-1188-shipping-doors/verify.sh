#!/usr/bin/env bash
# The harness this directory recorded, runnable.
#
# wrap_ship.c must be compiled as C: it defines gate_dispatch_c with C linkage, and handing the file
# to g++ mangles it, which is what made this harness look like it referenced a symbol the tree had
# lost. It never had: the definition was committed here all along.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
D=$(dirname "$0")
fail=0

gcc-13 -O2 -c "$D/wrap_ship.c" -o /tmp/ship_wrap.o || { echo "compile failed: wrap_ship.c"; exit 1; }
if g++-13 -O2 -std=c++17 -w "$D/diff.cpp" /tmp/ship_wrap.o -L "$LIB" -lllama \
        -Wl,-rpath,"$LIB" -o /tmp/ship_diff 2>/tmp/ship_diff.log; then
    out=$(/tmp/ship_diff 2>&1) || fail=1
    printf '%-12s %s\n' "diff" "$(echo "$out" | grep -c 'mismatches=0')/5 arms green"
else
    echo "compile failed: diff"; head -3 /tmp/ship_diff.log; fail=1
fi

for g in gate_adaptive gate_mirostat gate_rng; do
    if ! g++-13 -O2 -std=c++17 -w "$D/$g.cpp" -L "$LIB" -lllama \
            -Wl,-rpath,"$LIB" -o "/tmp/ship_$g" 2>/tmp/ship_$g.log; then
        echo "compile failed: $g"; head -2 /tmp/ship_$g.log; fail=1; continue
    fi
    out=$(/tmp/ship_$g 2>&1) || fail=1
    printf '%-12s %s\n' "$g" "$(echo "$out" | tail -1)"
done
exit $fail
