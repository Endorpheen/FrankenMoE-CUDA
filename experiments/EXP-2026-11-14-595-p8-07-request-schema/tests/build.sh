#!/usr/bin/env bash
# Differential gate for P8-07 S1 (request-schema fields), no server and no
# sockets: the native probe includes tools/server/server-schema.cpp, the Goose
# side is the port plus a generated driver, both emit the same 181 lines.
#
# Usage: bash tests/build.sh
# FG_LLAMA, FG_GOOSE, FG_CXX, FG_OUT override the defaults.
set -euo pipefail
E="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
L=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CXX=${FG_CXX:-g++-13}
OUT=${FG_OUT:-$E/build}
mkdir -p "$OUT"

python3 "$E/tests/gen_schema.py"

"$CXX" -O1 -std=c++17 -ffunction-sections -fdata-sections \
    -I"$L/tools/server" -I"$L/common" -I"$L/include" -I"$L/src" \
    -I"$L/ggml/include" -I"$L/tools/mtmd" -I"$L/vendor" \
    -c "$E/tests/probe_schema.cpp" -o "$OUT/probe_schema.o"
"$CXX" -Wl,--gc-sections "$OUT/probe_schema.o" \
    -L"$L/build-cuda/bin" -lllama-common -lllama -lmtmd -lggml -lggml-base \
    -Wl,-rpath,"$L/build-cuda/bin" -o "$OUT/probe_schema"

timeout 300 "$OUT/probe_schema" > "$OUT/native_schema.out" 2> "$OUT/native_schema.err"
timeout 300 "$G" run "$OUT/gate_schema.goose" > "$OUT/goose_schema.out" 2> "$OUT/goose_schema.err"

if diff -u "$OUT/native_schema.out" "$OUT/goose_schema.out" > "$OUT/diff.txt"; then
    echo "gate: $(wc -l < "$OUT/native_schema.out") native lines, $(wc -l < "$OUT/goose_schema.out") Goose lines, diff = 0"
else
    echo "gate FAILED: see build/diff.txt"
    exit 1
fi
