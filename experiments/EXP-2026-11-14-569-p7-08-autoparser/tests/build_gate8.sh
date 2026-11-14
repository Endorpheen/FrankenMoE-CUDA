#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
set -e
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p gate8
"$G" --standalone -o ../source/runtime-gen.c --header ../source/runtime-gen.h ../source/runtime.goose >/dev/null
gcc-13 -O0 -std=c11 -w -c ../source/runtime-gen.c -o gate8/gen.o
g++-13 -O2 -std=c++17 -I../source -c gate8.cpp -o gate8/gate.o
g++-13 gate8/gate.o gate8/gen.o -o gate8/gate
./gate8/gate
