#!/usr/bin/env bash
set -euo pipefail

output=$(realpath -m -- "${1:?Usage: build-gooseinfer-bonsai-pipeline.sh OUTPUT}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/integration/goose-standalone/universal/bonsai-frozen-cuda"
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p -- "$(dirname -- "$output")"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT

for module in main flash unpack; do
    arch=sm_80
    if [[ $module == flash ]]; then arch=sm_89; fi
    "${CLANG:-clang-18}" -target nvptx64-nvidia-cuda -march="$arch" -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
        -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
        -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$source_dir/pipeline-$module.cl" -o "$scratch/$module.ptx"
done
python3 "$source_dir/mtp-kv-panels.py" "$scratch/flash.ptx"
python3 "$source_dir/pipeline-flash-shared.py" "$scratch/flash.ptx"
python3 "$source_dir/assemble-pipeline.py" "$source_dir" "$scratch"
"$cuda/bin/ptxas" -arch=sm_89 "$scratch/pipeline.ptx" -o "$scratch/pipeline.cubin"
mv -- "$scratch/pipeline.cubin" "$output"
