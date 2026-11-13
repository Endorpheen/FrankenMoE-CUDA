#!/usr/bin/env bash
set -euo pipefail
root=/home/end0/AI/LlamaServer/exp134-goose-cuda
export GOOSE_HOME="$root/toolchain/cache"
goose_compiler="$root/toolchain/goose-v2026.3-linux-x64/goose"
"$goose_compiler" --standalone --header "$root/build/silu_gate.h" -o "$root/build/silu_gate.c" "$root/source/silu_gate.goose"
"$goose_compiler" --standalone -O2 --header "$root/build/launch_loop.h" -o "$root/build/launch_loop.c" "$root/source/launch_loop.goose"
python3 "$root/source/emit_cuda.py"
/usr/bin/gcc-13 -O3 -fPIC -c "$root/build/launch_loop.c" -o "$root/build/launch_loop.o"
/usr/bin/nvcc -O3 -lineinfo --fmad=false -std=c++17 -arch=sm_89 -ccbin /usr/bin/g++-13 -I"$root/build" "$root/source/probe.cu" "$root/build/launch_loop.o" -lm -ldl -lpthread -o "$root/build/probe"
