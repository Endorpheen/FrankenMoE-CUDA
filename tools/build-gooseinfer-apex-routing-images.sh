#!/usr/bin/env bash
set -euo pipefail

out=$(realpath -m -- "${1:?Usage: build-gooseinfer-apex-routing-images.sh OUTPUT_DIR}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source_dir="$root/integration/goose-standalone/universal/cuda/apex-routing"
goose=${APEX_ROUTING_GOOSE:?Set APEX_ROUTING_GOOSE to the Goose 2026.3 compiler}
ptxas=${APEX_PTXAS:?Set APEX_PTXAS to CUDA 12.4 ptxas}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$out"

python3 "$root/tools/emit-gooseinfer-cuda-ptx-primitives.py" \
    "$root/integration/goose-standalone/cuda-ptx-primitives.json" "$scratch"
bash "$root/tools/build-gooseinfer-cuda-memory.sh" "$scratch"
printf '#include "cuda-device-prelude.h"\n#include "typed-memory.h"\n#pragma OPENCL FP_CONTRACT ON\n#include "typed-memory-bridge.h"\n' > "$scratch/typed-routing-memory.h"

for abi in "$root/integration/goose-standalone/"apex-*-entries.json; do
    name=${abi##*/}
    python3 "$root/tools/emit-gooseinfer-cuda-entries.py" "$abi" "$scratch/${name%.json}.h"
done
for module in route softmax mailbox; do
    goose_module=$module
    assembler=$ptxas
    alignment=16
    case "$module" in
        route) fragment=device-goose.h; header=compact-route-image.h; symbol=compact_route_image ;;
        softmax) fragment=softmax-device.h; header=softmax-image.h; symbol=router_softmax_image ;;
        mailbox)
            fragment=device-goose.h; header=route-mailbox-image.h; symbol=route_mailbox_image
            goose_module=route
            alignment=0
            assembler="${CUDA_HOME:-/usr/local/cuda}/bin/ptxas"
            ;;
    esac
    cat "$source_dir/warp-shuffle.goose" "$source_dir/$goose_module.goose" > "$scratch/$module.goose"
    if [[ $module == mailbox ]]; then cat "$source_dir/mailbox.goose" >> "$scratch/$module.goose"; fi
    "$goose" -o "$scratch/$module.c" --standalone --header "$scratch/$module.h" "$scratch/$module.goose"
    python3 "$source_dir/extract-device.py" "$scratch/$module.c" "$scratch/$fragment"
    "${CLANG:-clang-18}" -target nvptx64-nvidia-cuda -nocudalib -O2 -x cl -cl-std=CL1.2 \
        -I"$scratch" -include "$scratch/typed-routing-memory.h" -Xclang -target-cpu -Xclang sm_80 -S "$scratch/apex-$module.cl" -o "$scratch/$module.ptx"
    "$assembler" -arch=sm_89 "$scratch/$module.ptx" -o "$scratch/$module.cubin"
    python3 - "$scratch/$module.cubin" "$out/$header" "$symbol" "$alignment" <<'PY'
from pathlib import Path
import sys

binary, header = map(Path, sys.argv[1:3])
data = binary.read_bytes()
with header.open("w") as stream:
    alignment = f"_Alignas({sys.argv[4]}) " if sys.argv[4] != "0" else ""
    stream.write(f"{alignment}static const unsigned char {sys.argv[3]}[] = {{\n")
    for offset in range(0, len(data), 24):
        stream.write(",".join(map(str, data[offset:offset + 24])) + ",\n")
    stream.write("};\n")
PY
done

"$goose" -o "$scratch/prediction.c" --standalone --header "$scratch/prediction.h" "$source_dir/prediction.goose"
python3 "$source_dir/extract-device.py" "$scratch/prediction.c" "$scratch/prediction-device.h"
"$goose" -o "$scratch/prediction.c" --standalone --header "$scratch/prediction.h" "$source_dir/prediction.goose"
python3 "$source_dir/extract-device.py" "$scratch/prediction.c" "$scratch/prediction-device.h"
"${CLANG:-clang-18}" -target nvptx64-nvidia-cuda -nocudalib -O2 -ffp-contract=on \
    -x cl -cl-std=CL1.2 -I"$scratch" -include "$scratch/typed-routing-memory.h" -S -Xclang -target-cpu -Xclang sm_89 \
    "$scratch/apex-prediction.cl" -o "$scratch/prediction.ptx"
"$ptxas" -arch=sm_89 "$scratch/prediction.ptx" -o "$scratch/prediction.cubin"
python3 - "$scratch/prediction.cubin" "$out/pred-image.h" <<'PY'
from pathlib import Path
import sys

binary, header = map(Path, sys.argv[1:])
data = binary.read_bytes()
with header.open("w") as stream:
    stream.write("_Alignas(16) static const unsigned char pred_image[] = {\n")
    for offset in range(0, len(data), 24):
        stream.write(",".join(map(str, data[offset:offset + 24])) + ",\n")
    stream.write("};\n")
PY
