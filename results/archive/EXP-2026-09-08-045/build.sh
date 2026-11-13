#!/bin/bash
# EXP-2026-09-08-045: build the model-free two-thread gather prototype in its own build dir.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../../..
BUILD="$ROOT/build/exp045-gather-2t"
mkdir -p "$BUILD"
nvcc -O2 -std=c++17 -Xcompiler "-Wall,-Wextra" \
    -o "$BUILD/exp045_gather2t" \
    exp045_gather2t.cpp -lcudart -lpthread
echo "built: $BUILD/exp045_gather2t"
