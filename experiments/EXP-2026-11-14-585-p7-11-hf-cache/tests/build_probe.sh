#!/bin/sh
# Builds the native oracle for the HF cache naming slice (S2) into build/.
# The probe includes common/hf-cache.cpp directly; --gc-sections drops the
# network/filesystem functions that are linked but never called, so the binary
# has no unresolved httplib/json/common symbols and never opens a socket.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
up=${HF_CACHE_UPSTREAM:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}

g++ -std=c++17 -O1 -ffunction-sections -fdata-sections \
    -I"$up/common" -I"$up/include" -I"$up/ggml/include" -I"$up/vendor" -I"$up" \
    "$here/tests/probe_s2.cpp" -Wl,--gc-sections -o "$here/build/probe_s2"

"$here/build/probe_s2" > "$here/build/native_s2.out"
echo "wrote build/probe_s2 and build/native_s2.out"
