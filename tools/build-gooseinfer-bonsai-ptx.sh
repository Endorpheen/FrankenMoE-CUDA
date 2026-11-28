#!/usr/bin/env bash
set -euo pipefail

output=$(realpath -m -- "${1:?Usage: build-gooseinfer-bonsai-ptx.sh OUTPUT}")
mode=${2:-mmq}
case "$mode" in
    mmq) arch=sm_80; defines=(-DGOOSE_CUDA_MMQ_IMAGE) ;;
    plain) arch=sm_70; defines=() ;;
    *) printf 'Unknown CUDA PTX mode: %s\n' "$mode" >&2; exit 2 ;;
esac
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
standalone="$root/integration/goose-standalone"
source_dir="$standalone/universal/bonsai-frozen-cuda"
goose=${GOOSE:?Set GOOSE to the Fried Goose KeelWing compiler}
mkdir -p -- "$(dirname -- "$output")"
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT

overlay=${3:-}
glue="$scratch/glue"
cp -R -- "$source_dir/glue" "$glue"
cp -- "$standalone/cuda-device.cl" "$glue/cuda-device.cl"

for source in "$source_dir"/goose/*.goose "$root/goose/cuda/standalone-matmul.goose"; do
    module=$(basename -- "$source" .goose)
    module=${module#standalone-}
    if [[ -d $overlay && -f $overlay/$module.goose ]]; then source="$overlay/$module.goose"; fi
    case "$module" in
        rope)
            cat "$source" "$root/goose/cuda/standalone-rope-device.goose" > "$scratch/$module.goose"
            source="$scratch/$module.goose"
            ;;
        q4-attention)
            cat "$source" "$root/goose/cuda/standalone-q4-attention-entry.goose" > "$scratch/$module.goose"
            source="$scratch/$module.goose"
            ;;
        mmq|mmq-quant|dense-mma|fwht|mmvq|mmvq-pack|ssm-conv|gdn|gdn-mma|q4-set-rows|flash-mma|flash-vec)
            launcher=$module
            if [[ "$module" == mmq-quant ]]; then launcher=mmq-pack; fi
            if [[ "$module" == ssm-conv ]]; then launcher=ssm; fi
            if [[ "$module" == gdn ]]; then
                cat "$source" "$root/goose/cuda/standalone-gdn-packet.goose" \
                    "$root/goose/cuda/standalone-gdn-launch.goose" > "$scratch/$module.goose"
            else
                cat "$source" "$root/goose/cuda/standalone-$launcher-launch.goose" > "$scratch/$module.goose"
            fi
            source="$scratch/$module.goose"
            ;;
    esac
    if [[ ${GOOSE_APEX_UP_GATE_IMAGE:-0} == 1 && "$module" == mmvq-q4 ]]; then
        cat "$source" "$root/goose/cuda/apex-up-gate.goose" > "$scratch/$module.goose"
        source="$scratch/$module.goose"
    fi
    if [[ ${overlay##*/} == moe-epilogue && "$module" == vector ]]; then
        cat "$source" "$overlay/launch.goose" > "$scratch/$module.goose"
        source="$scratch/$module.goose"
    fi
    "$goose" --standalone --emit-provenance "$scratch/$module.c" "$source"
    python3 "$source_dir/frankenmoe-fragment.py" "$scratch" "$module"
done

if [[ -d $overlay ]]; then
    for patch_file in "$overlay"/*.cl.patch; do
        [[ -f $patch_file ]] || continue
        name=${patch_file##*/}
        target=${name%.patch}
        if [[ ! -f $glue/$target ]]; then
            cp -- "$standalone/$target" "$glue/$target"
        fi
        patch --batch --forward --silent "$glue/$target" < "$patch_file"
    done
elif [[ -n $overlay ]]; then
    patch --batch --forward --silent "$scratch/flash-vec-device.h" < "$overlay"
fi

if [[ ${overlay##*/} == moe-epilogue ]]; then defines+=(-DGOOSE_APEX_MOE_EPILOGUE_IMAGE); fi
if [[ ${overlay##*/} == fwht-fixed ]]; then defines+=(-DGOOSE_APEX_FWHT_FIXED_IMAGE); fi
if [[ ${overlay##*/} == rms-onevec ]]; then defines+=(-DGOOSE_APEX_RMS_ONEVEC_IMAGE); fi
if [[ ${GOOSE_APEX_UP_GATE_IMAGE:-0} == 1 ]]; then defines+=(-DGOOSE_APEX_UP_GATE_IMAGE); fi

python3 "$root/tools/emit-gooseinfer-cuda-entries.py" \
    "$standalone/cuda-attention-entries.json" "$scratch/cuda-attention-entries.h"
python3 "$root/tools/emit-gooseinfer-cuda-entries.py" \
    "$standalone/cuda-mmq-entries.json" "$scratch/cuda-mmq-entries.h"
python3 "$root/tools/emit-gooseinfer-cuda-entries.py" \
    "$standalone/cuda-activation-entries.json" "$scratch/cuda-activation-entries.h"
python3 "$root/tools/emit-gooseinfer-cuda-entries.py" \
    "$standalone/cuda-intrinsic-aliases.json" "$scratch/cuda-intrinsic-aliases.h"
python3 "$root/tools/emit-gooseinfer-cuda-ptx-primitives.py" "$standalone/cuda-ptx-primitives.json" "$scratch"
bash "$root/tools/build-gooseinfer-cuda-memory.sh" "$scratch"
for abi in "$standalone/"cuda-{dense-mma,flash-mma,gdn-mma,flash-vec,mmvq,rms-reference,fwht,up-gate,apex-epilogue}-entries.json; do
    name=${abi##*/}
    python3 "$root/tools/emit-gooseinfer-cuda-entries.py" "$abi" "$scratch/${name%.json}.h"
done

for abi in "$standalone/"cuda-*-bonsai-entries.json; do
    name=${abi##*/}
    python3 "$root/tools/emit-gooseinfer-cuda-entries.py" "$abi" "$scratch/${name%.json}.h"
done
"${CLANG:-clang-18}" -target nvptx64-nvidia-cuda -nocudalib -O2 -x cl -cl-std=CL1.2 \
    -include "$source_dir/device-prelude.cl" -I"$scratch" -I"$standalone" -S -Xclang -target-cpu \
    -Xclang "$arch" -DGOOSE_CUDA_FROZEN_ABI "${defines[@]}" "$glue/cuda-device.cl" -o "$scratch/bonsai.ptx"
mv -- "$scratch/bonsai.ptx" "$output"
