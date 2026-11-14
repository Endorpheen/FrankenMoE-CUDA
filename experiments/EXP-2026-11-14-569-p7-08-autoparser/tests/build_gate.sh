#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
# Rebuild the draft arena, the C object and the differential gate.
# -O0 is used for the generated 9.9 MB C file: -O2 exceeds the 5-minute limit.
set -e
G=/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose
mkdir -p gate
"$G" --standalone -o ../source/runtime-gen.c --header ../source/runtime-gen.h ../source/runtime.goose
gcc-13 -O0 -std=c11 -w -c ../source/runtime-gen.c -o gate/gen.o
g++-13 -O2 -std=c++17 -I../source -c gate.cpp -o gate/gate.o
g++-13 gate/gate.o gate/gen.o -o gate/gate
./gate/gate
