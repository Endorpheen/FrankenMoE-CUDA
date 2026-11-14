#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
set -e
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p gate5
"$G" --standalone -o ../source/runtime-gen.c --header ../source/runtime-gen.h ../source/runtime.goose >/dev/null
gcc-13 -O0 -std=c11 -w -c ../source/runtime-gen.c -o gate5/gen.o
g++-13 -O2 -std=c++17 -I../source -c gate5.cpp -o gate5/gate.o
g++-13 gate5/gate.o gate5/gen.o -o gate5/gate
./gate5/gate
