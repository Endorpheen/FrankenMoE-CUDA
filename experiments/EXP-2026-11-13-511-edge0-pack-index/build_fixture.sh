#!/bin/bash
# Build and run the Edge0 pack reader gate.
# Usage: build_fixture.sh [pack-directory]
set -euo pipefail
PACK="${1:-/home/end0/AI/LocalAI/Models/Edge0-120B-q3k}"
F=/home/end0/AI/FrankenMoE-CUDA
E=$F/experiments/EXP-2026-11-13-511-edge0-pack-index
OUT=$F/runtime/exp511/build
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose

mkdir -p "$OUT"
timeout 300 "$G" --standalone -o "$E/source/pack-gen.c" --header "$E/source/pack-gen.h" "$E/source/pack.goose" >/dev/null
timeout 300 gcc-13 -O0 -std=c11 -c "$E/source/pack-gen.c" -o "$OUT/pack-gen.o"
timeout 300 g++-13 -O2 -std=c++17 -I"$E/source" "$E/tests/pack_fixture.cpp" "$OUT/pack-gen.o" -o "$OUT/pack-fixture"
timeout 300 "$OUT/pack-fixture" "$PACK"
