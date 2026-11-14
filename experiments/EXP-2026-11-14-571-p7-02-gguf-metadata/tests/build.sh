#!/bin/bash
# Build and run the P7-02 S1 (GGUF metadata) gate.
#
#   source/s1_metadata.goose --standalone--> s1_metadata.c/.h
#   tests/probe_s1.cpp  links the native libggml-base and prints the oracle TSV
#   tests/host_s1.cpp   drives the Goose port and prints the same TSV
#
# The generated C and objects stay in build/ and are rebuilt only when the
# source changes. No generated file is written next to the sources.
set -euo pipefail

SUB=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
REPO=${FG_REPO:-/home/end0/AI/FrankenMoE-CUDA}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
L=${FG_LLAMA:-$REPO/upstream/llama.cpp}
BIN=${FG_BIN:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin}
OUT=$SUB/build
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}

mkdir -p "$OUT"
SHA=$(sha256sum "$SUB/source/s1_metadata.goose" | awk '{print $1}')

if [ ! -f "$OUT/s1_metadata.o" ] || [ "$(cat "$OUT/.gen.sha" 2>/dev/null || true)" != "$SHA" ]; then
    "$GOOSE" --standalone -o "$OUT/s1_metadata.c" --header "$OUT/s1_metadata.h" \
        "$SUB/source/s1_metadata.goose"
    "$CC" -O0 -std=c11 -c "$OUT/s1_metadata.c" -o "$OUT/s1_metadata.o"
    printf '%s\n' "$SHA" > "$OUT/.gen.sha"
fi

"$CXX" -O0 -std=c++17 -I"$OUT" -c "$SUB/tests/host_s1.cpp" -o "$OUT/host_s1.o"
"$CXX" "$OUT/host_s1.o" "$OUT/s1_metadata.o" -o "$OUT/host_s1"

"$CXX" -O0 -std=c++17 -I"$L/ggml/include" -c "$SUB/tests/probe_s1.cpp" -o "$OUT/probe_s1.o"
"$CXX" "$OUT/probe_s1.o" -L"$BIN" -lggml-base -Wl,-rpath,"$BIN" -o "$OUT/probe_s1"

"$OUT/probe_s1" > "$OUT/oracle.tsv"
"$OUT/host_s1" > "$OUT/goose.tsv"

if diff -u "$OUT/oracle.tsv" "$OUT/goose.tsv" > "$OUT/diff.txt"; then
    printf 'FG_G1 cases=%s values=%s failures=0\n' \
        "$(grep -vc '^#' "$OUT/oracle.tsv")" "$(wc -l < "$OUT/oracle.tsv")"
else
    echo "FG_G1 failure: TSV mismatch"
    head -20 "$OUT/diff.txt"
    exit 1
fi
