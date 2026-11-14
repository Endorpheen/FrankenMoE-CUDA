#!/bin/bash
# Build the native gguf-split tool that the gate uses as its oracle.
#
# The prebuilt flint tree does not ship tools/gguf-split, so the binary is
# rebuilt from the pinned upstream source. Only the --version banner needs
# generated build-info; tests/version-shim.cpp supplies those four symbols, the
# rest of gguf-split.cpp is the unmodified native file.
#
#   build/native/gguf-split
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
L=${FG_LLAMA:-$REPO/upstream/llama.cpp}
BIN=${FG_BIN:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin}
OUT=${FG_OUT:-$SUB/build}/native
CXX=${CXX:-g++-13}

if [ -x "$BIN/gguf-split" ]; then
    printf '%s\n' "$BIN/gguf-split"
    exit 0
fi

mkdir -p "$OUT"
"$CXX" -O2 -std=c++17 -I"$L/common" -I"$L/include" -I"$L/ggml/include" \
    -c "$L/tools/gguf-split/gguf-split.cpp" -o "$OUT/gguf-split.o"
"$CXX" "$OUT/gguf-split.o" "$SUB/tests/version-shim.cpp" \
    -L"$BIN" -lllama -lggml-base -Wl,-rpath,"$BIN" -o "$OUT/gguf-split"
printf '%s\n' "$OUT/gguf-split"
