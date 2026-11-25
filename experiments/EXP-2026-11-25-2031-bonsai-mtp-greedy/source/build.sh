#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a diagnostic output directory}
previous="$root/build/bonsai-mtp-commit-2030"
experiments="$src/../.."
cuda=${CUDA_HOME:-/usr/local/cuda}
compiler=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
mkdir -p "$out"
python3 "$src/prepare.py" "$previous/backend.c" "$out/backend.c"
cat "$previous/target.goose" "$src/kv-only.goose" > "$out/greedy.goose"
"$compiler" --standalone -o "$out/greedy.c" --header "$out/bonsai.h" "$out/greedy.goose"
gcc-13 -O2 -fPIC -ffunction-sections -fdata-sections -w -c "$out/greedy.c" -o "$out/greedy.o"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$src" -I"$previous" -I"$experiments/EXP-2026-11-25-2030-bonsai-mtp-device-commit/source" \
    -I"$experiments/EXP-2026-11-25-2029-bonsai-mtp-exact-verify/source" \
    -I"$experiments/EXP-2026-11-25-2025-bonsai-mtp-head/source" \
    -I"$experiments/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$out/greedy.o" \
    "$experiments/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/bonsai-mtp-greedy"
