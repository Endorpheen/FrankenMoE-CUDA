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

for source in "$source_dir"/goose/*.goose; do
    module=$(basename -- "$source" .goose)
    if [[ -d $overlay && -f $overlay/$module.goose ]]; then source="$overlay/$module.goose"; fi
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

"${CLANG:-clang-18}" -target nvptx64-nvidia-cuda -nocudalib -O2 -x cl -cl-std=CL1.2 \
    -include "$source_dir/device-prelude.cl" -I"$scratch" -I"$standalone" -S -Xclang -target-cpu \
    -Xclang "$arch" -DGOOSE_CUDA_FROZEN_ABI "${defines[@]}" "$glue/cuda-device.cl" -o "$scratch/bonsai.ptx"
mv -- "$scratch/bonsai.ptx" "$output"
