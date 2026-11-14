#!/usr/bin/env bash
# S3 regression: the accepted tensor/descriptor gate (EXP-2026-11-14-601)
# compiled against this experiment's generated object. The S1/S2/S3 bodies are
# carried unchanged; only their internal names are core_* with exported
# wrappers, so the whole writer path must still pass 41 cases / 307 checks.
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
OUT=${FG_OUT:-$SUB/build}
EXP601=${FG_EXP601:-/home/end0/AI/FrankenMoE-CUDA/experiments/EXP-2026-11-14-601-p7-02-gguf-tensors}

bash "$SUB/tests/build.sh" final >/dev/null

# The S3 fixture includes its own generated header name.
cp "$OUT/model_saver.h" "$OUT/gguf_tensors.h"
g++-13 -O1 -std=c++17 -I"$OUT" -I"$L/ggml/include" \
    -c "$EXP601/tests/fixture.cpp" -o "$OUT/exp601_fixture.o"
g++-13 "$OUT/exp601_fixture.o" "$OUT/model_saver.o" \
    -L"$L/build-cuda/bin" -lggml-base -Wl,-rpath,"$L/build-cuda/bin" \
    -o "$OUT/exp601_fixture" -lm
"$OUT/exp601_fixture" "$OUT/exp601_tmp.bin"
