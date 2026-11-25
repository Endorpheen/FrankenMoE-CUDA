#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
base="$root/build/bonsai-mtp-head-2038"
merge="$root/build/bonsai-mtp-merge-2036"
prefill="$root/build/bonsai-mtp-prefill-2032"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
choice="$experiments/EXP-2026-11-25-2034-bonsai-mtp-device-argmax/source"
transaction="$experiments/EXP-2026-11-25-2030-bonsai-mtp-device-commit/source"
mkdir -p "$out"
python3 "$src/prepare.py" "$base/backend.c" "$merge/verify-ops.inc" "$choice" "$transaction" "$experiments" "$out"
python3 "$src/emit.py" \
    "$experiments/EXP-2026-11-25-2028-bonsai-mtp-q8-batch/source/verify-mvq.cl" \
    "$experiments/EXP-2026-11-25-2033-bonsai-mtp-attention-reuse/source/verify-attention.cl" \
    "$src/conv-four.cl" "$out/verify-four.cl"
cat "$merge/merge.cl" "$out/verify-four.cl" > "$out/verify.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/verify.cl" -o "$out/verify.ptx"
python3 "$experiments/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/verify.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/verify.ptx" -o "$out/verify.cubin" 2>"$out/ptxas.txt"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$base" -I"$prefill" \
    -I"$experiments/EXP-2026-11-25-2032-bonsai-mtp-batch-prefill/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$prefill/greedy.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/bonsai-mtp-width"
