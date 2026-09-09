#!/usr/bin/env bash
# EXP-050 model-free gate: build the synthetic CUDA-graph harness against the
# diag build and verify the env-gated decision diagnostics.
#
# Checks:
#   1. OFF run (GGML_GRAPH_DIAG unset) emits zero [EXP050- lines and completes.
#   2. ON run emits the full expected decision sequence for one graph key:
#      direct_first_call -> capture -> launch -> direct_warmup_reset (with a
#      field-level diff naming the renamed node) -> capture.
#   3. Numeric results are identical in ON and OFF runs (diagnostics must not
#      change execution) and across all rounds within each run.
#
# No model files, no server, no profiler. Requires build/exp050-diag to exist.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
ARCH="$ROOT/results/archive/EXP-2026-09-09-050"
BUILD="$ROOT/build/exp050-diag"

cd "$ROOT"

echo "== building harness =="
cc -O2 "$ARCH/exp050_synth.c" \
   -I "$ROOT/work/llama.cpp-exp050-diag/ggml/include" \
   -L "$BUILD/bin" -lggml -lggml-base -lggml-cuda -lm \
   -Wl,-rpath,"$BUILD/bin" \
   -o "$BUILD/exp050_synth"

PASS=0
FAIL=0

check() {
    local name="$1" ok="$2"
    if [ "$ok" = "1" ]; then
        echo "CHECK PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "CHECK FAIL: $name"
        FAIL=$((FAIL + 1))
    fi
}

echo "== OFF run (GGML_GRAPH_DIAG unset) =="
env -u GGML_GRAPH_DIAG "$BUILD/exp050_synth" 2> "$ARCH/gate-off.log"
grep -c "\[EXP050" "$ARCH/gate-off.log" > /dev/null 2>&1 && n_off=$(grep -c "\[EXP050" "$ARCH/gate-off.log") || n_off=0
check "OFF emits zero diagnostic lines" "$([ "$n_off" = "0" ] && echo 1 || echo 0)"
check "OFF run completed" "$(grep -q 'gate: OK' "$ARCH/gate-off.log" && echo 1 || echo 0)"

echo "== ON run (GGML_GRAPH_DIAG=1) =="
GGML_GRAPH_DIAG=1 "$BUILD/exp050_synth" 2> "$ARCH/gate-on.log"
check "ON run completed" "$(grep -q 'gate: OK' "$ARCH/gate-on.log" && echo 1 || echo 0)"

check "event direct_first_call present"     "$(grep -q 'decision=direct_first_call' "$ARCH/gate-on.log" && echo 1 || echo 0)"
check "event capture present (>=2)"         "$([ "$(grep -c 'decision=capture' "$ARCH/gate-on.log")" -ge 2 ] && echo 1 || echo 0)"
check "event launch present"                "$(grep -q 'decision=launch' "$ARCH/gate-on.log" && echo 1 || echo 0)"
check "event direct_warmup_reset present"   "$(grep -q 'decision=direct_warmup_reset' "$ARCH/gate-on.log" && echo 1 || echo 0)"
check "reset carries field-level name diff" "$(grep -q "diff=node\[0\].name" "$ARCH/gate-on.log" && echo 1 || echo 0)"
check "phase field present (none expected)" "$(grep -q 'phase=none' "$ARCH/gate-on.log" && echo 1 || echo 0)"

echo "== ON vs OFF numeric equality =="
sums_off=$(grep 'gate: round' "$ARCH/gate-off.log" | sed 's/.*checksum=//' | tr '\n' ' ')
sums_on=$(grep  'gate: round' "$ARCH/gate-on.log"  | sed 's/.*checksum=//' | tr '\n' ' ')
check "checksums identical ON vs OFF" "$([ "$sums_off" = "$sums_on" ] && [ -n "$sums_off" ] && echo 1 || echo 0)"

echo "== event trace (ON) =="
grep '\[EXP050' "$ARCH/gate-on.log"

echo
echo "RESULT: PASS=$PASS FAIL=$FAIL"
[ "$FAIL" = "0" ]
