#!/usr/bin/env bash
set -euo pipefail
source_dir=$(cd "$(dirname "$0")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=${1:?Specify a new scratch directory}
compiler=${GOOSE_BIN:-/home/end0/AI/Fried-Goose/releases/fried-goose-v0.3.0-preview-keelwing/bin/goose}
cuda=${CUDA_HOME:-/usr/local/cuda}
main=${FRANKEN_MAIN:-/home/end0/AI/FrankenMoE-CUDA-main}
mkdir "$out"
for file in gguf-tensors.goose quant-dequant2.goose; do
    patch -s "$repo/goose/model/$file" -o "$out/$file" < "$source_dir/$file.patch"
done
sed -n '/^fn mr_name(/,/^fn mr_moe_name(/{ /^fn mr_moe_name(/d;p; }' \
    "$repo/goose/model/qwen35-model-loader.goose" > "$out/model-names.goose"
cat "$repo/goose/misc/gguf-headers.goose" "$out/gguf-tensors.goose" \
    "$repo/goose/model/gguf-reader.goose" "$out/quant-dequant2.goose" \
    "$out/model-names.goose" "$source_dir/bonsai-layout.goose" "$source_dir/bonsai-hadamard.goose" > "$out/core.goose"
"$compiler" --standalone -o "$out/core.c" --header "$out/core.h" "$out/core.goose"
gcc-13 -O2 -fPIC -ffunction-sections -fdata-sections -w -c "$out/core.c" -o "$out/core.o"
objcopy --keep-global-symbol=goose_init --keep-global-symbol=gr_parse --keep-global-symbol=qd_dequant \
    --keep-global-symbol=bonsai_signed_fwht --keep-global-symbol=bonsai_bind_layout "$out/core.o" "$out/exports.o"
gcc-13 -shared -Wl,--gc-sections,-z,defs "$out/exports.o" "$source_dir/bits.c" -lm -pthread -o "$out/core.so"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$source_dir/bonsai-pq2.cl" -o "$out/bonsai.ptx"
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/bonsai.ptx" -o "$out/bonsai.cubin"
gcc-13 -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
    "$main/integration/goose-standalone/cli/tokenize-main.c" \
    "$repo/build/checkpoint-inkling-exp1969-before-bonsai/libfranken-inference.a" \
    -lm -ldl -pthread -o "$out/frankenmoe-tokenize"
