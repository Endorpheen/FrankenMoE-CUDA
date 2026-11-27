#!/usr/bin/env bash
set -euo pipefail

out=$(realpath -m -- "${1:?Usage: build-gooseinfer-inkling-cuda-images.sh OUTPUT_DIR}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src="$root/integration/goose-standalone/universal/cuda/images"
shared="$root/integration/goose-standalone/universal/bonsai-frozen-cuda"
goose=${GOOSE_BIN:-goose}
cuda=${CUDA_HOME:-/usr/local/cuda}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$out"
while read -r header symbol entries prelude; do
    stem=${header%.h}
    options=(-Xclang -mlink-builtin-bitcode -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc")
    frontend=(-march=sm_80 -cl-std=CL2.0 -nocudainc -Xclang -finclude-default-header)
    case "$stem" in
        bridge-image) options=(-DI2_EXPF_TEST) ;;
        attention-image)
            "$goose" --standalone --emit-provenance "$scratch/inkling-attention.c" "$src/inkling-attention.goose"
            python3 "$shared/frankenmoe-fragment.py" "$scratch" inkling-attention
            frontend=(-Xclang -target-cpu -Xclang sm_80 -cl-std=CL1.2 -include "$shared/device-prelude.cl" -I"$scratch")
            ;;
        i2-decode-q8-image|matrix-image)
            "$goose" --standalone -o "$scratch/inkling-matrix.c" "$src/$stem.goose"
            (cd -- "$scratch" && python3 "$src/inkling-matrix-fragment.py")
            options=(-DGG_ALIGN1= -DGG_UNALIGNED= -I"$scratch")
            ;;
    esac
    if [[ -n $prelude ]]; then options+=(-include "$src/$prelude"); fi
    assembler=(-arch=sm_89)
    if [[ -n $entries && $entries != - ]]; then assembler+=(--entry "$entries"); fi
    "${CLANG:-clang-18}" -target nvptx64-nvidia-cuda "${frontend[@]}" -x cl -O3 -ffp-contract=off \
        -nocudalib "${options[@]}" \
        -S "$src/$stem.cl" -o "$scratch/$stem.ptx"
    "$cuda/bin/ptxas" "${assembler[@]}" "$scratch/$stem.ptx" -o "$scratch/$stem.cubin"
    python3 - "$scratch/$stem.cubin" "$out/$header" "$symbol" <<'PY'
from pathlib import Path
import sys

binary, header = map(Path, sys.argv[1:3])
data = binary.read_bytes()
with header.open("w") as stream:
    stream.write(f"unsigned char {sys.argv[3]}[] = {{\n")
    for offset in range(0, len(data), 24):
        stream.write(",".join(map(str, data[offset:offset + 24])) + ",\n")
    stream.write(f"}};\nunsigned int {sys.argv[3]}_len = {len(data)};\n")
PY
done < "$src/inkling-images.txt"
