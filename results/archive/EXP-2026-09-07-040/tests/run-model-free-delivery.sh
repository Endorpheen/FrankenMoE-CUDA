#!/bin/bash
# EXP-038 model-free checks. Run from repo root. No server, no model.
set -u
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
T="$ROOT/results/archive/EXP-2026-09-07-040/tests"
BIN="$ROOT/build/exp040-delivery/bin"
g++ -std=c++17 -O1 -o "$T/test-exp038-ring" "$T/test-exp038-ring.cpp" \
    -I "$ROOT/work/llama.cpp-exp038-pinned-ring/ggml/include" \
    -I "$ROOT/work/llama.cpp-exp038-pinned-ring/ggml/src" \
    -I /usr/local/cuda-12.6/include \
    -L "$BIN" -Wl,-rpath,"$BIN" -l:libggml-cuda.so -lggml -l:libggml-base.so \
    -lcudart -L/usr/local/cuda-12.6/lib64
cd "$T"
echo "=== ring OFF ===";           ./test-exp038-ring ring out-delivery-ring-off.bin                 2>&1 | tee log-delivery-ring-off.txt
echo "=== ring ON ===";            GGML_EXPERT_PINNED_RING=1 ./test-exp038-ring ring out-delivery-ring-on.bin   2>&1 | tee log-delivery-ring-on.txt
echo "=== ring ON + ALLOC FAIL ==="; GGML_EXPERT_PINNED_RING=1 GGML_EXPERT_RING_ALLOC_FAIL=1 ./test-exp038-ring ring out-delivery-ring-fail.bin 2>&1 | tee log-delivery-ring-fail.txt
echo "=== sched OFF ===";          ./test-exp038-ring sched out-delivery-sched-off.bin               2>&1 | tee log-delivery-sched-off.txt
echo "=== sched ON ===";           GGML_EXPERT_PINNED_RING=1 ./test-exp038-ring sched out-delivery-sched-on.bin 2>&1 | tee log-delivery-sched-on.txt
echo "=== sched ON + ALLOC FAIL ==="; GGML_EXPERT_PINNED_RING=1 GGML_EXPERT_RING_ALLOC_FAIL=1 ./test-exp038-ring sched out-delivery-sched-fail.bin 2>&1 | tee log-delivery-sched-fail.txt
echo "=== output equality ===";    sha256sum out-delivery-sched-*.bin | tee log-delivery-sched-sha.txt
