#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
# Rebuild the draft, the generated C object and the slice-2 gate.
set -e
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p gate2
"$G" --standalone -o ../source/runtime-gen.c --header ../source/runtime-gen.h ../source/runtime.goose
gcc-13 -O0 -std=c11 -w -c ../source/runtime-gen.c -o gate2/gen.o
g++-13 -O2 -std=c++17 -I../source -c gate2.cpp -o gate2/gate.o
g++-13 gate2/gate.o gate2/gen.o -o gate2/gate
./gate2/gate
