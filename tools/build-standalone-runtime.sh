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
#   integration/goose-standalone/  per-stage header shims, the v4 host, the bindings
#
# The link needs libc, libm and pthread only: no ggml, no llama, no libstdc++.
# `ldd` on the result shows three entries, and `nm` shows no C++ symbols.
#
# Generated C is not committed; the unit is built here. The shims are committed.
#
# Usage: bash tools/build-standalone-runtime.sh [--check]
#   --check  build, run the graph gate and v4, fail on a mismatch
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=$ROOT/builds/goose-standalone
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
# OFF ggml libraries: pure substrate, no carrier bridges, so exactly one
# goose_init can exist in the process.
SUB=${FG_SUBSTRATE:-/tmp/opencode/sub-661/tree/build-off/llama.cpp/ggml/src}
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

echo "[4b/5] v4: metadata, tensor directory and payload"
# One host covers what v2 and v3 did separately; the superseded copies are gone,
# because a superseded adapter is pure debt (AGENTS.md).
if [ -d "$SUB" ] && [ -f "$ROOT/integration/goose-standalone/host_v4.c" ]; then
    $CC -O1 -std=c11 -w -I "$OUT/inc" -I "$ROOT/upstream/llama.cpp/ggml/include" \
        -I "$ROOT/integration/goose-standalone" \
        -c "$ROOT/integration/goose-standalone/host_v4.c" -o "$OUT/host_v4.o"
    $CC -O1 -std=c11 -w -I "$ROOT/upstream/llama.cpp/ggml/include" \
        -I "$ROOT/integration/goose-standalone" \
        -c "$ROOT/integration/goose-standalone/tensor-data.c" -o "$OUT/tensor-data.o"
    $CC -O1 -std=c11 -w -I "$ROOT/upstream/llama.cpp/ggml/include" \
        -c "$ROOT/integration/goose-standalone/graph-bindings.c" -o "$OUT/graph-bindings.o"
    # Link with $CXX, and the reason matters: libggml-base.a itself contains six C++ objects
    # (ggml.cpp, ggml-backend.cpp, ggml-backend-meta.cpp, ggml-opt.cpp, ggml-threading.cpp) and
    # ggml_critical_section_start references std::__throw_system_error. So libstdc++ is a
    # SUBSTRATE dependency, not ours. Our own sources are now all C - graph-bindings and
    # tensor-data were converted - which is the part we control.
    $CXX -O1 -fopenmp "$ROOT/build/goose-main/main.o" "$OUT/host.o" "$OUT/host_v4.o" \
        "$OUT/tensor-data.o" "$OUT/graph-bindings.o" \
        -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread \
        -o "$OUT/frankenmoe-v4"
    printf '  v4:     %s (%s B)\n' "$OUT/frankenmoe-v4" "$(stat -c%s "$OUT/frankenmoe-v4")"

    # The graph-execution gate is a test, not an adapter: it proves that a node the
    # Goose builder creates runs on the CPU backend (EXP681).
    $CC -O1 -std=c11 -w -I "$ROOT/upstream/llama.cpp/ggml/include" \
        -c "$ROOT/integration/goose-standalone/tests/graph_exec.c" -o "$OUT/graph_exec.o"
    $CXX -O1 -fopenmp "$ROOT/build/goose-main/main.o" "$OUT/host.o" \
        "$OUT/graph-bindings.o" "$OUT/graph_exec.o" \
        -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread \
        -o "$OUT/graph_exec"
    printf '  gate:   %s\n' "$OUT/graph_exec"
fi

echo "[5/5] result"
printf '  runtime: %s (%s B)\n' "$OUT/frankenmoe-v4" \
    "$(stat -c%s "$OUT/frankenmoe-v4")"
printf '  deps:    %s\n' "$(ldd "$OUT/frankenmoe-v4" | awk '{print $1}' | tr '\n' ' ')"

if [ "$CHECK" -eq 1 ]; then
    if ldd "$OUT/frankenmoe-v4" | grep -q "libllama"; then
        echo "standalone runtime: the binary links the old llama runtime" >&2
        exit 1
    fi

    # v4 and the graph gate, when the substrate is present.
    if [ -x "$OUT/graph_exec" ]; then
        g=$("$OUT/graph_exec" 2>&1)
        printf '  gate:   %s\n' "$(printf '%s' "$g" | tr '\n' '|')"
        case "$g" in
            *FAIL*) echo "standalone runtime: graph execution gate failed" >&2; exit 1 ;;
        esac
    fi
    if [ -x "$OUT/frankenmoe-v4" ] && [ -f "$ROOT/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid/tiny-hybrid.gguf" ]; then
        v=$("$OUT/frankenmoe-v4" "$ROOT/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid/tiny-hybrid.gguf" 2>&1 | head -1)
        printf '  v4:     %s\n' "$v"
        case "$v" in
            *mismatches=0*) ;;
            *) echo "standalone runtime: tensor layout mismatch in v4" >&2; exit 1 ;;
        esac
    fi
fi
