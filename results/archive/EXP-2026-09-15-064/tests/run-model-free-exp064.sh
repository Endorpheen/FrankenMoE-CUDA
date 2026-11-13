#!/bin/bash
# EXP-064 model-free checks for the depth-4 pinned expert ring (GGML_CUDA_EXPERT_RING_SLOTS = 4).
# Adapted from results/archive/EXP-2026-09-15-062/tests/run-model-free-exp062.sh (depth-3 gate).
# Same 12-check structure and the same semantics:
# unset = ON, =1 = ON, =0 = OFF, GGML_EXPERT_RING_ALLOC_FAIL = safe fallback.
# The sched outputs must reproduce the accepted sha 39505119b1...e91510 (EXP-038/040/041/043/062).
# Run from repo root. No server, no model. One run at a time.
set -u
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
T="$ROOT/results/archive/EXP-2026-09-15-064/tests"
BIN="$ROOT/build/exp064-ring4/bin"
SRC="$ROOT/work/llama.cpp-exp064-ring4"
g++ -std=c++17 -O1 -o "$T/test-exp064-ring" "$T/test-exp064-ring.cpp" \
    -I "$SRC/ggml/include" \
    -I "$SRC/ggml/src" \
    -I /usr/local/cuda-12.6/include \
    -L "$BIN" -Wl,-rpath,"$BIN" -l:libggml-cuda.so -lggml -l:libggml-base.so \
    -lcudart -L/usr/local/cuda-12.6/lib64
cd "$T"
echo "=== ring default (expect ON, 4 slots) ==="; ./test-exp064-ring ring out-ring-default.bin                2>&1 | tee log-ring-default.txt
echo "=== ring ON ===";                          GGML_EXPERT_PINNED_RING=1 ./test-exp064-ring ring out-ring-on.bin   2>&1 | tee log-ring-on.txt
echo "=== ring OFF ===";                         GGML_EXPERT_PINNED_RING=0 ./test-exp064-ring ring out-ring-off.bin  2>&1 | tee log-ring-off.txt
echo "=== ring ON + ALLOC FAIL ===";             GGML_EXPERT_RING_ALLOC_FAIL=1 ./test-exp064-ring ring out-ring-fail.bin 2>&1 | tee log-ring-fail.txt
echo "=== sched default (expect ON) ===";        ./test-exp064-ring sched out-sched-default.bin              2>&1 | tee log-sched-default.txt
echo "=== sched ON ===";                         GGML_EXPERT_PINNED_RING=1 ./test-exp064-ring sched out-sched-on.bin  2>&1 | tee log-sched-on.txt
echo "=== sched OFF ===";                        GGML_EXPERT_PINNED_RING=0 ./test-exp064-ring sched out-sched-off.bin 2>&1 | tee log-sched-off.txt
echo "=== sched ON + ALLOC FAIL ===";            GGML_EXPERT_RING_ALLOC_FAIL=1 ./test-exp064-ring sched out-sched-fail.bin 2>&1 | tee log-sched-fail.txt
echo "=== output equality ===";                  sha256sum out-sched-*.bin | tee log-sched-sha.txt
