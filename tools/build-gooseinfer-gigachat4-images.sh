#!/usr/bin/env bash
set -euo pipefail

out=$(realpath -m -- "${1:?Usage: build-gooseinfer-gigachat4-images.sh OUTPUT_DIR}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
standalone="$root/integration/goose-standalone"
src="$standalone/universal/cuda/gigachat4-images"
goose=${APEX_ROUTING_GOOSE:?Set APEX_ROUTING_GOOSE to the Goose 2026.3 compiler}
ptxas=${APEX_PTXAS:?Set APEX_PTXAS to CUDA 12.4 ptxas}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$out"

for source in "$src"/goose/*.goose; do
    module=$(basename -- "$source" .goose)
    "$goose" --standalone -o "$scratch/$module.c" --header "$scratch/$module.h" "$source"
    python3 "$src/extract-device.py" "$scratch" "$module"
done
compile_device() {
    "${CLANG:-clang-18}" -target nvptx64-nvidia-cuda -nocudalib -O2 -DGOOSE_CUDA_MMQ_IMAGE -x cl -cl-std=CL1.2 \
        -I"$scratch" -I"$standalone" -S -Xclang -target-cpu -Xclang sm_80 \
        "$1" -o "$scratch/device.ptx"
}

sed -i 's/\ba16_f32\b/mla_a16_f32/g' "$scratch/q4-attention-device.h"
compile_device "$src/cuda-device.cl"
for variant in dense vector dsa mla; do
    case "$variant" in
        dense) entries=goose_dense_mma ;;
        vector) entries=goose_vector_ops,goose_vector_flat,goose_vector_flat4 ;;
        dsa)
            "$goose" --standalone -o "$scratch/q4-attention.c" --header "$scratch/q4-attention.h" "$src/dsa-mla.goose"
            python3 "$src/extract-device.py" "$scratch" q4-attention
            sed -i 's/\ba16_f32\b/mla_a16_f32/g' "$scratch/q4-attention-device.h"
            sed -i -E 's/static void (giga_mla_mma_pv_(tile|indexed)_g\()/static __attribute__((always_inline)) void \1/' \
                "$scratch/q4-attention-device.h"
            compile_device "$standalone/cuda-device.cl"
            entries=goose_dsa_score,goose_giga_dsa_score_shared_q,goose_giga_dsa_top_wide,goose_giga_mask_clear,goose_giga_mask_scatter
            ;;
        mla)
            entries=goose_mla_attention_tiles,goose_mla_attention_merge,goose_mla_attention_pair,goose_mla_attention_batch_pair,goose_mla_attention_batch_merge
            entries+=,goose_giga_mla_mma_pv_tile,goose_giga_mla_mma_pv_visible_tile,goose_giga_mla_visibility
            entries+=,goose_giga_mla_compact_mask,goose_giga_mla_mma_pv_indexed,goose_giga_mla_indexed_merge,goose_giga_mla_mma_tile
            ;;
    esac
    "$ptxas" -arch=sm_89 --entry "$entries" "$scratch/device.ptx" -o "$scratch/$variant.cubin"
    python3 - "$scratch/$variant.cubin" "$out/giga-$variant-image.h" "giga_${variant}_image" <<'PY'
from pathlib import Path
import sys

binary, header = map(Path, sys.argv[1:3])
data = binary.read_bytes()
with header.open("w") as stream:
    stream.write(f"static const unsigned char {sys.argv[3]}[] = {{\n")
    for offset in range(0, len(data), 24):
        stream.write(",".join(map(str, data[offset:offset + 24])) + ",\n")
    stream.write("};\n")
PY
done
