#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
previous="$root/build/bonsai-mtp-head-2025"
target="$root/build/bonsai-mtp-target-2027"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"
cat "$previous/stem.cl" "$src/verify-mvq.cl" > "$out/verify.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/verify.cl" -o "$out/verify.ptx"
python3 "$experiments/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/verify.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/verify.ptx" -o "$out/verify.cubin" 2>"$out/ptxas.txt"
python3 "$src/prepare.py" "$target/backend.c" "$out/backend.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$previous" -I"$experiments/EXP-2026-11-25-2027-bonsai-mtp-target-verify/source" \
    -I"$experiments/EXP-2026-11-25-2025-bonsai-mtp-head/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$previous/stem.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/target-q8"
