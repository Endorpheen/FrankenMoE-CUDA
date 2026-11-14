#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
set -e
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p gate4
"$G" --standalone -o ../source/runtime-gen.c --header ../source/runtime-gen.h ../source/runtime.goose >/dev/null
gcc-13 -O0 -std=c11 -w -c ../source/runtime-gen.c -o gate4/gen.o
g++-13 -O2 -std=c++17 -I../source -c gate4.cpp -o gate4/gate.o
g++-13 gate4/gate.o gate4/gen.o -o gate4/gate
./gate4/gate
