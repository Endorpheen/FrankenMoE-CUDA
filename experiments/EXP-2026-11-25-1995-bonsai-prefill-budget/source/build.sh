#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
base=${BONSAI_WIDE_BUILD:-"$repo/build/bonsai-mmq-wide-1992-default"}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
python3 "$src/instrument.py" "$base/mmq-wide-backend.c" "$out/profile-backend.c"
"${CC:-gcc-13}" -O2 -g -Wall -Wextra -Werror -Wl,--gc-sections -I"$base" -I"$cuda/include" \
    "$out/profile-backend.c" "$base/bonsai.o" \
    "$src/../../EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -L"$cuda/lib64" -Wl,-rpath,"$cuda/lib64" -lnvToolsExt -l:libcuda.so.1 -lm -pthread \
    -o "$out/bonsai-profile"
