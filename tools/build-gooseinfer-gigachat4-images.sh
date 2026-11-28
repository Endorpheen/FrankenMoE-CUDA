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

for source in "$src"/goose/*.goose "$root/goose/cuda/standalone-matmul.goose"; do
    module=$(basename -- "$source" .goose)
    module=${module#standalone-}
    case "$module" in
        mmq|mmq-quant|mmvq|mmvq-pack|ssm-conv|gdn|gdn-mma|q4-set-rows|flash-mma|flash-vec)
            launcher=$module
            if [[ "$module" == mmq-quant ]]; then launcher=mmq-pack; fi
            if [[ "$module" == ssm-conv ]]; then launcher=ssm; fi
            cat "$source" "$root/goose/cuda/standalone-$launcher-launch.goose" > "$scratch/$module.goose"
            source="$scratch/$module.goose"
            ;;
    esac
    "$goose" --standalone -o "$scratch/$module.c" --header "$scratch/$module.h" "$source"
    python3 "$src/extract-device.py" "$scratch" "$module"
done
compile_device() {
    "${CLANG:-clang-18}" -target nvptx64-nvidia-cuda -nocudalib -O2 -DGOOSE_CUDA_MMQ_IMAGE -x cl -cl-std=CL1.2 \
        -I"$scratch" -I"$standalone" -S -Xclang -target-cpu -Xclang sm_80 \
        "$1" -o "$scratch/device.ptx"
}

cat "$src/dsa-mla.goose" "$root/goose/cuda/"standalone-{q4-attention-entry,mla-launch}.goose > "$scratch/q4-attention.goose"
"$goose" --standalone -o "$scratch/q4-attention.c" --header "$scratch/q4-attention.h" "$scratch/q4-attention.goose"
python3 "$src/extract-device.py" "$scratch" q4-attention
sed -i -E 's/static void (giga_mla_mma_pv_(tile|indexed)_g\()/static __attribute__((always_inline)) void \1/' \
    "$scratch/q4-attention-device.h"
python3 "$root/tools/emit-gooseinfer-cuda-entries.py" \
    "$standalone/cuda-attention-entries.json" "$scratch/cuda-attention-entries.h"
python3 "$root/tools/emit-gooseinfer-cuda-entries.py" \
    "$standalone/cuda-mmq-entries.json" "$scratch/cuda-mmq-entries.h"
python3 "$root/tools/emit-gooseinfer-cuda-entries.py" \
    "$standalone/gigachat4/cuda-dsa-entries.json" "$scratch/cuda-dsa-entries.h"
compile_device "$standalone/cuda-device.cl"
for variant in dense vector dsa mla certified-dsa; do
    case "$variant" in
        dense) entries=goose_dense_mma ;;
        vector) entries=goose_vector_ops,goose_vector_flat,goose_vector_flat4 ;;
        dsa)
            entries=goose_dsa_score,goose_giga_dsa_score_shared_q,goose_giga_dsa_top_wide,goose_giga_mask_clear,goose_giga_mask_scatter
            ;;
        mla)
            entries=goose_mla_attention_tiles,goose_mla_attention_merge,goose_mla_attention_pair,goose_mla_attention_batch_pair,goose_mla_attention_batch_merge
            entries+=,goose_giga_mla_mma_pv_tile,goose_giga_mla_mma_pv_visible_tile,goose_giga_mla_visibility
            entries+=,goose_giga_mla_compact_mask,goose_giga_mla_mma_pv_indexed,goose_giga_mla_indexed_merge,goose_giga_mla_mma_tile
            ;;
        certified-dsa)
            "$goose" --standalone -o "$scratch/dsa-certified.c" --header "$scratch/dsa-certified.h" "$standalone/gigachat4/dsa-certified-device.goose"
            python3 "$src/extract-device.py" "$scratch" dsa-certified
            cp "$standalone/cuda-device.cl" "$scratch/certified-device.cl"
            printf '\n#include "gigachat4/cuda-dsa-certified.cl"\n' >> "$scratch/certified-device.cl"
            compile_device "$scratch/certified-device.cl"
            entries=goose_dsa_pack_power2,goose_dsa_bound_metadata,goose_dsa_bound_query,goose_dsa_integer_score,goose_dsa_score_cutoff,goose_dsa_compact,goose_dsa_replay,goose_dsa_finalize,goose_dsa_fallback_score,goose_dsa_fallback_top
            ;;
    esac
    "$ptxas" -arch=sm_89 --entry "$entries" "$scratch/device.ptx" -o "$scratch/$variant.cubin"
    python3 - "$scratch/$variant.cubin" "$out/giga-$variant-image.h" "giga_${variant//-/_}_image" <<'PY'
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
