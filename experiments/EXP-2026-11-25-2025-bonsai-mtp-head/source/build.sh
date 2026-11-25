#!/usr/bin/env bash
set -euo pipefail
src=$(cd "$(dirname "$0")" && pwd)
root=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
base="$root/build/bonsai-mmq-pipeline-2019-http"
dev="$root/build/bonsai-mmq-pipeline-2019-packed"
host="$root/build/bonsai-mmq-k64-2000-default"
cuda=${CUDA_HOME:-/usr/local/cuda}
compiler=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
exp="$src/../.."
mkdir "$out"
prev="$exp/EXP-2026-11-25-2024-bonsai-mtp-stem/source"
cat "$dev/pipeline.cl" "$prev/stem.cl" "$src/head.cl" > "$out/stem.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/stem.cl" -o "$out/stem.ptx"
python3 "$exp/EXP-2026-11-25-2018-bonsai-flash-kv-panels/source/dynamic.py" "$out/stem.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/stem.ptx" -o "$out/stem.cubin" 2>"$out/ptxas.txt"
cat "$host/ffn-tail.goose" "$prev/stem.goose" "$src/head.goose" > "$out/stem.goose"
"$compiler" --standalone -o "$out/stem.c" --header "$out/bonsai.h" "$out/stem.goose"
gcc-13 -O2 -fPIC -ffunction-sections -fdata-sections -w -c "$out/stem.c" -o "$out/stem.o"
python3 "$prev/compose.py" "$base/backend.c" "$out/stem-backend.c"
python3 "$src/compose.py" "$out/stem-backend.c" "$out/backend.c"
gcc-13 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -I"$out" -I"$src" -I"$exp/EXP-2026-11-25-2002-bonsai-session/source" -I"$cuda/include" \
    "$out/backend.c" "$out/stem.o" "$exp/EXP-2026-11-25-1973-bonsai-pq2-bootstrap/source/bits.c" \
    -lm -ldl -pthread -o "$out/bonsai-mtp-head"
ln -s "$host/frozen-goose.ptx" "$out/frozen-goose.ptx"
