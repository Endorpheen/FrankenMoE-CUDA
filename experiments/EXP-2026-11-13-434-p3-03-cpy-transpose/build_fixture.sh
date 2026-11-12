#!/bin/bash
set -e
F=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108
E=$F/experiments/EXP-2026-11-13-434-p3-03-cpy-transpose
OUT=$F/runtime/exp434/build
mkdir -p "$OUT"
python3 "$E/emit_sources.py"
/usr/bin/nvcc \
  -I"$E/source" \
  -I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda \
  -I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/ggml-cuda/.. \
  -I/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/ggml/src/../include \
  -forward-unknown-to-host-compiler -ccbin=/usr/bin/g++-13 \
  -DGGML_BACKEND_BUILD -DGGML_BACKEND_SHARED -DGGML_SHARED \
  -DGGML_SCHED_MAX_COPIES=4 -D_GNU_SOURCE -D_XOPEN_SOURCE=600 -Dggml_cuda_EXPORTS \
  -O3 -DNDEBUG -std=c++17 \
  '--generate-code=arch=compute_89,code=[compute_89,sm_89]' \
  -Xcompiler=-fPIC -use_fast_math -extended-lambda \
  -x cu "$E/tests/gpu-fixture.cu" -o "$OUT/gpu-fixture"
