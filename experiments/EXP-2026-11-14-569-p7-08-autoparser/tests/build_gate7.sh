#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
set -e
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p gate7
"$G" --standalone -o ../source/runtime-gen.c --header ../source/runtime-gen.h ../source/runtime.goose >/dev/null
gcc-13 -O0 -std=c11 -w -c ../source/runtime-gen.c -o gate7/gen.o
g++-13 -O2 -std=c++17 -I../source -c gate7.cpp -o gate7/gate.o
g++-13 gate7/gate.o gate7/gen.o -o gate7/gate
./gate7/gate
