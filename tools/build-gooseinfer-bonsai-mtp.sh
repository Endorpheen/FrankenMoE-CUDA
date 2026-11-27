#!/usr/bin/env bash
set -euo pipefail

output=$(realpath -m -- "${1:?Usage: build-gooseinfer-bonsai-mtp.sh OUTPUT}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/integration/goose-standalone/universal/bonsai-frozen-cuda"
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p -- "$(dirname -- "$output")"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT

"${CLANG:-clang-18}" -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$source_dir/mtp-mvq.cl" -o "$scratch/mvq.ptx"
python3 "$source_dir/mtp-kv-panels.py" "$scratch/mvq.ptx"
python3 "$source_dir/mtp-query-pack.py" "$scratch/mvq.ptx"
"$cuda/bin/ptxas" -arch=sm_89 "$scratch/mvq.ptx" -o "$scratch/mvq.cubin"
mv -- "$scratch/mvq.cubin" "$output"
