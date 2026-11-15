#!/bin/bash
# Build the standalone FrankenMoE runtime: the composed Goose unit, the host
# adapter and nothing else.
#
# This is the direct path P11 asks for: the shipped executable must be the Goose
# runtime itself, not llama.cpp with Goose bolted on. The three inputs are
#
#   build/goose-main/main.{c,h}   the composed unit (tools/build-main-runtime.sh)
#   integration/m1/host/goose_host.c  the host primitives (fp16, bit ops, JSON
#                                  helpers, the one pthread_once)
#   integration/goose-standalone/  the per-stage header shims and the smoke host
#
# The link needs libc, libm and pthread only: no ggml, no llama, no libstdc++.
# `ldd` on the result shows three entries, and `nm` shows no C++ symbols.
#
# Generated C is not committed; the unit is built here. The shims are committed.
#
# Usage: bash tools/build-standalone-runtime.sh [--check]
#   --check  build, run the smoke host and fail unless it reports init_runs=1
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=$ROOT/builds/goose-standalone
CC=${CC:-gcc-13}
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CHECK=0
[ "${1:-}" = "--check" ] && CHECK=1

mkdir -p "$OUT/inc"

echo "[1/5] compose the unit"
if [ ! -f "$ROOT/build/goose-main/main.c" ]; then
    bash "$ROOT/tools/build-main-runtime.sh" "$ROOT/build/goose-main"
fi

echo "[2/5] stage the unit header and the per-stage shims"
cp -f "$ROOT/build/goose-main/main.h" "$OUT/inc/main.h"
for h in goose-unit config-slice gguf-stack json-schema-grammar json \
         metrics-traces models quant-stack session spec-decide \
         stream-policy tokenizer-decisions; do
    printf '#pragma once\n// One composed unit, so every per-stage header is a forward to it.\n#include "main.h"\n' \
        > "$OUT/inc/$h.h"
done

echo "[3/5] compile the unit and the adapter"
$CC -O1 -std=c11 -w -c "$ROOT/build/goose-main/main.c" -o "$OUT/main.o"
$CC -O1 -std=c11 -w -I "$ROOT/integration/m1/host" -I "$OUT/inc" \
    -c "$ROOT/integration/m1/host/goose_host.c" -o "$OUT/host.o"

echo "[4/5] compile and link the smoke host"
$CC -O1 -std=c11 -w -I "$OUT/inc" \
    -c "$ROOT/integration/goose-standalone/smoke.c" -o "$OUT/smoke.o"
$CC -O1 "$ROOT/build/goose-main/main.o" "$OUT/host.o" "$OUT/smoke.o" \
    -lm -pthread -o "$OUT/frankenmoe-standalone"

echo "[4b/5] v4: metadata, tensor directory and payload"
# One host covers what v2 and v3 did separately; the superseded copies are gone,
# because a superseded adapter is pure debt (AGENTS.md).
if [ -d "$SUB" ] && [ -f "$ROOT/integration/goose-standalone/host_v4.c" ]; then
    $CC -O1 -std=c11 -w -I "$OUT/inc" -I "$ROOT/upstream/llama.cpp/ggml/include" \
        -I "$ROOT/integration/goose-standalone" \
        -c "$ROOT/integration/goose-standalone/host_v4.c" -o "$OUT/host_v4.o"
    $CXX -O1 -std=c++17 -w -I "$ROOT/upstream/llama.cpp/ggml/include" \
        -I "$ROOT/integration/goose-standalone" \
        -c "$ROOT/integration/goose-standalone/tensor-data.cpp" -o "$OUT/tensor-data.o"
    $CXX -O1 -std=c++17 -w -I "$ROOT/upstream/llama.cpp/ggml/include" \
        -c "$ROOT/integration/goose-standalone/graph-bindings.cpp" -o "$OUT/graph-bindings.o"
    $CXX -O1 -fopenmp "$ROOT/build/goose-main/main.o" "$OUT/host.o" "$OUT/host_v4.o" \
        "$OUT/tensor-data.o" "$OUT/graph-bindings.o" \
        -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread -lstdc++ \
        -o "$OUT/frankenmoe-v4"
    printf '  v4:     %s (%s B)\n' "$OUT/frankenmoe-v4" "$(stat -c%s "$OUT/frankenmoe-v4")"
fi

echo "[5/5] result"
printf '  binary: %s (%s B)\n' "$OUT/frankenmoe-standalone" \
    "$(stat -c%s "$OUT/frankenmoe-standalone")"
printf '  deps:   %s\n' "$(ldd "$OUT/frankenmoe-standalone" | awk '{print $1}' | tr '\n' ' ')"

if [ "$CHECK" -eq 1 ]; then
    line=$("$OUT/frankenmoe-standalone")
    printf '  run:    %s\n' "$line"
    case "$line" in
        *init_runs=1*) ;;
        *) echo "standalone runtime: smoke host did not report init_runs=1" >&2; exit 1 ;;
    esac
    # A C++ runtime or a ggml symbol here would mean the old runtime leaked in.
    if ldd "$OUT/frankenmoe-standalone" | grep -qE "libstdc\+\+|libggml|libllama"; then
        echo "standalone runtime: the binary links a C++ or ggml library" >&2
        exit 1
    fi
    echo "standalone runtime: ok (single Goose runtime, libc/libm/pthread only)"
fi
