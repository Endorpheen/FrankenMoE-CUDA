#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
previous="$root/build/bonsai-mtp-greedy-2031"
transaction="$root/build/bonsai-mtp-commit-2030"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
compiler=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
mkdir -p "$out"
python3 "$src/prepare.py" "$previous/backend.c" \
    "$experiments/EXP-2026-11-25-2031-bonsai-mtp-greedy/source/greedy.inc" "$out"
cat "$previous/greedy.goose" "$src/kv-batch.goose" > "$out/greedy.goose"
"$compiler" --standalone -o "$out/greedy.c" --header "$out/bonsai.h" "$out/greedy.goose"
gcc-13 -O2 -fPIC -ffunction-sections -fdata-sections -w -c "$out/greedy.c" -o "$out/greedy.o"
cat "$transaction/commit.cl" "$src/kv-batch.cl" > "$out/greedy.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/greedy.cl" -o "$out/greedy.ptx"
python3 "$experiments/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/greedy.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/greedy.ptx" -o "$out/greedy.cubin" 2>"$out/ptxas.txt"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$src" -I"$transaction" -I"$experiments/EXP-2026-11-25-2030-bonsai-mtp-device-commit/source" \
    -I"$experiments/EXP-2026-11-25-2029-bonsai-mtp-exact-verify/source" \
    -I"$experiments/EXP-2026-11-25-2025-bonsai-mtp-head/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$out/greedy.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/bonsai-mtp-batch"
