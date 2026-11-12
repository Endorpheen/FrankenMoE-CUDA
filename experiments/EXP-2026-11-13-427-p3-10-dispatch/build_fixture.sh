#!/bin/bash
# Build the P3-10 dispatch differential gate.
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-427-p3-10-dispatch
B=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin
OUT=$F/runtime/exp427/build
mkdir -p "$OUT"

python3 "$E/emit_sources.py"
gcc-13 -O2 -std=c11 -fPIC -I"$E/source" -c "$E/source/dispatch-unit.c" -o "$OUT/dispatch-unit.o"
g++-13 -O2 -std=c++17 -x c++ -I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/include \
  "$E/tests/dispatch_fixture.cu" -x none "$OUT/dispatch-unit.o" \
  -L"$B" -Wl,-rpath,"$B" -lggml-cuda -lggml -lggml-base -lggml-cpu \
  -o "$OUT/dispatch-fixture"
