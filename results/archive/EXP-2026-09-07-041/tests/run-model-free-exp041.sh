#!/bin/bash
# EXP-041 model-free checks for the default-on pinned expert ring. Run from repo root.
# No server, no model. Same 12-check structure as EXP-038 with default-on semantics:
# unset = ON, =1 = ON, =0 = OFF, GGML_EXPERT_RING_ALLOC_FAIL = safe fallback.
set -u
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
T="$ROOT/results/archive/EXP-2026-09-07-041/tests"
BIN="$ROOT/build/exp041-default-runtime/bin"
SRC="$ROOT/work/llama.cpp-exp041"
g++ -std=c++17 -O1 -o "$T/test-exp041-ring" "$T/test-exp041-ring.cpp" \
    -I "$SRC/ggml/include" \
    -I "$SRC/ggml/src" \
    -I /usr/local/cuda-12.6/include \
    -L "$BIN" -Wl,-rpath,"$BIN" -l:libggml-cuda.so -lggml -l:libggml-base.so \
    -lcudart -L/usr/local/cuda-12.6/lib64
cd "$T"
echo "=== ring default (expect ON) ===";     ./test-exp041-ring ring out-ring-default.bin                2>&1 | tee log-ring-default.txt
echo "=== ring ON ===";                     GGML_EXPERT_PINNED_RING=1 ./test-exp041-ring ring out-ring-on.bin   2>&1 | tee log-ring-on.txt
echo "=== ring OFF ===";                     GGML_EXPERT_PINNED_RING=0 ./test-exp041-ring ring out-ring-off.bin  2>&1 | tee log-ring-off.txt
echo "=== ring ON + ALLOC FAIL ===";         GGML_EXPERT_RING_ALLOC_FAIL=1 ./test-exp041-ring ring out-ring-fail.bin 2>&1 | tee log-ring-fail.txt
echo "=== sched default (expect ON) ===";    ./test-exp041-ring sched out-sched-default.bin              2>&1 | tee log-sched-default.txt
echo "=== sched ON ===";                     GGML_EXPERT_PINNED_RING=1 ./test-exp041-ring sched out-sched-on.bin  2>&1 | tee log-sched-on.txt
echo "=== sched OFF ===";                    GGML_EXPERT_PINNED_RING=0 ./test-exp041-ring sched out-sched-off.bin 2>&1 | tee log-sched-off.txt
echo "=== sched ON + ALLOC FAIL ===";         GGML_EXPERT_RING_ALLOC_FAIL=1 ./test-exp041-ring sched out-sched-fail.bin 2>&1 | tee log-sched-fail.txt
echo "=== output equality ===";              sha256sum out-sched-*.bin | tee log-sched-sha.txt
