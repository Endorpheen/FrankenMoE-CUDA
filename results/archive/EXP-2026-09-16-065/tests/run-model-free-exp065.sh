#!/bin/bash
# EXP-065 model-free gate for the async device->host readback (GGML_SCHED_READBACK_ASYNC).
# Builds the scheduler test against build/exp065-hostser and checks:
#   prefill - output sha equals the accepted 39505119...e91510 (EXP-038/041/043/062/064)
#   decode  - the device->host path output is byte-identical with the flag off and on
# No model and no server. Run from the repo root.
set -u
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
T="$ROOT/results/archive/EXP-2026-09-16-065/tests"
BIN="$ROOT/build/exp065-hostser/bin"
SRC="$ROOT/work/llama.cpp-exp065-hostser"

g++ -std=c++17 -O1 -o "$T/test-exp065-sched" "$T/test-exp065-sched.cpp" \
    -I "$SRC/ggml/include" \
    -I "$SRC/ggml/src" \
    -I /usr/local/cuda-12.6/include \
    -L "$BIN" -Wl,-rpath,"$BIN" -l:libggml-cuda.so -lggml -l:libggml-base.so \
    -lcudart -L/usr/local/cuda-12.6/lib64 || exit 3

cd "$T"
echo "=== prefill (expect accepted sched sha 39505119...) ==="
./test-exp065-sched prefill out-prefill.bin 2>&1 | tee log-prefill.txt
echo "=== decode, flag OFF (baseline) ==="
GGML_SCHED_READBACK_ASYNC=0 ./test-exp065-sched decode out-decode-off.bin 2>&1 | tee log-decode-off.txt
echo "=== decode, flag ON (async readback) ==="
GGML_SCHED_READBACK_ASYNC=1 ./test-exp065-sched decode out-decode-on.bin 2>&1 | tee log-decode-on.txt
echo "=== output sha256 ==="
sha256sum out-prefill.bin out-decode-off.bin out-decode-on.bin | tee log-sha.txt
