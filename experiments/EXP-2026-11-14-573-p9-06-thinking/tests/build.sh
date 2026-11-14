#!/bin/bash
# Differential gate build for P9-06 S3 (thinking control).
#
#   tests/build.sh
#
# Native oracle source (runtime/core/src/engine/thinking_control.cpp) is
# compiled into the fixture; the renderer (common_chat_templates_apply) comes
# from llama-common. The Goose port supplies only the pure decision.
set -euo pipefail

S=$(cd "$(dirname "$0")/.." && pwd)
R=${REPO_ROOT:-/home/end0/AI/FrankenMoE-CUDA}
L=${LLAMA_DIR:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${GOOSE_BIN:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
OPT=${OPT:--O1}
SRC=${GOOSE_SRC:-$S/source/think.goose}
OUT=${OUT_DIR:-$S/build}
BIN=$OUT/fg577-think

mkdir -p "$OUT"

need_gen=0
if [ ! -f "$OUT/gen.o" ] || [ ! -f "$OUT/runtime-gen.h" ]; then
    need_gen=1
elif [ "$SRC" -nt "$OUT/gen.o" ]; then
    need_gen=1
elif [ "$(cat "$OUT/.opt" 2>/dev/null || true)" != "$OPT" ]; then
    need_gen=1
fi

if [ "$need_gen" = 1 ]; then
    echo "[fg577] goose --standalone"
    "$G" --standalone -o "$OUT/runtime-gen.c" --header "$OUT/runtime-gen.h" "$SRC"
    echo "[fg577] gcc -O0"
    "$CC" "$OPT" -std=c11 -c "$OUT/runtime-gen.c" -o "$OUT/gen.o"
    echo "$OPT" > "$OUT/.opt"
else
    echo "[fg577] gen.o cached"
fi

echo "[fg577] g++ fixture + native thinking_control.cpp"
"$CXX" "$OPT" -std=c++17 \
    -I"$OUT" -I"$R/runtime/core/include" -I"$R/runtime/core/src/engine" \
    -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$S/tests/fixture.cpp" -o "$OUT/fixture.o"
"$CXX" "$OPT" -std=c++17 \
    -I"$R/runtime/core/include" -I"$R/runtime/core/src/engine" \
    -I"$L/common" -I"$L/include" -I"$L/src" -I"$L/ggml/include" \
    -c "$S/tests/native_thinking_control.cpp" -o "$OUT/thinking_control.o"

echo "[fg577] link"
"$CXX" "$OUT/fixture.o" "$OUT/thinking_control.o" "$OUT/gen.o" \
    -L"$L/build-cuda/bin" -lllama-common -lggml-base \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$BIN"

echo "[fg577] run"
"$BIN" "$L"
