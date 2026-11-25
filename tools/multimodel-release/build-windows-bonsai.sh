#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a fresh Windows build directory}
base="$root/build/bonsai-mtp-warp-rows-2047"
prefix="$root/build/bonsai-ffn-prefix-2044-both"
cc=${WINDOWS_CC:-x86_64-w64-mingw32-gcc-posix}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/port-bonsai-windows.py" "$base/backend.c" "$out/bonsai-win.c"
"$cc" -O2 -ffunction-sections -fdata-sections -w -c "$prefix/prefix.c" -o "$out/bonsai-goose.o"
"$cc" -O2 -D_WIN32_WINNT=0x0a00 -ffunction-sections -fdata-sections -Wl,--gc-sections -static \
    -I"$prefix" -I"$root/build/bonsai-mtp-five-2046" -I"$root/build/bonsai-mtp-pairs-2043" \
    -I"$root/build/bonsai-mtp-head-2038" -I"$root/build/bonsai-mtp-prefill-2032" \
    -I"$root/experiments/EXP-2026-11-25-2032-bonsai-mtp-batch-prefill/source" \
    -I"$root/experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/bonsai-win.c" "$out/bonsai-goose.o" \
    "$root/experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -pthread -o "$out/bonsai-mtp.exe"
