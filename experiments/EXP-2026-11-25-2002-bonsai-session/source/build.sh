#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
accepted=${1:?Specify the EXP-2000 build directory}
out=${2:?Specify a new scratch directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
cat "$accepted/mmq-k64-backend.c" "$src/session.inc" > "$out/backend.c"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -I"$accepted" -I"$src" -I"$cuda/include" -Dmain=bonsai_diagnostic_main \
    -c "$out/backend.c" -o "$out/bonsai-session.o"
"${CC:-gcc-13}" -O2 -Wall -Wextra -Werror -Wl,--gc-sections -I"$src" \
    "$src/check-session.c" "$out/bonsai-session.o" "$accepted/ffn-tail.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -l:libcuda.so.1 -lm -pthread -o "$out/check-session"
