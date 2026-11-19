#!/usr/bin/env bash
# The three gates that survive: they call the exported doors directly, so they do not depend on the
# run-local dispatcher that diff.cpp in this directory still declares. diff.cpp is kept as the record
# of what was measured at the time; it is not the harness that runs now.
set -uo pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
LIB=$R/builds/flint-acc/bin
fail=0
for g in gate_adaptive gate_mirostat gate_rng; do
    if ! g++-13 -O2 -std=c++17 -w "$(dirname "$0")/$g.cpp" -L "$LIB" -lllama \
            -Wl,-rpath,"$LIB" -o "/tmp/ship_$g" 2>/tmp/ship_$g.log; then
        echo "compile failed: $g"; head -2 /tmp/ship_$g.log; fail=1; continue
    fi
    out=$(/tmp/ship_$g 2>&1) || fail=1
    printf '%-12s %s\n' "$g" "$(echo "$out" | tail -1)"
done
exit $fail
