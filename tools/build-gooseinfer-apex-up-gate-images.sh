#!/usr/bin/env bash
set -euo pipefail

out=$(realpath -m -- "${1:?Usage: build-gooseinfer-apex-up-gate-images.sh OUTPUT_DIR}")
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cuda=${CUDA_HOME:-/usr/local/cuda}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$out"

GOOSE_APEX_UP_GATE_IMAGE=1 bash "$tools/build-gooseinfer-bonsai-ptx.sh" "$scratch/all.ptx" mmq
for variant in fused shared; do
    case "$variant" in
        fused) entry=goose_mmvq_up_gate_glu; symbol=fused_up_gate_image ;;
        shared) entry=goose_mmvq_shared_up_gate_glu; symbol=fused_shared_image ;;
    esac
    "$cuda/bin/ptxas" -arch=sm_89 --entry "$entry" "$scratch/all.ptx" -o "$scratch/$variant.cubin"
    python3 - "$scratch/$variant.cubin" "$out/$variant-image.h" "$symbol" <<'PY'
from pathlib import Path
import sys

binary, header = map(Path, sys.argv[1:3])
data = binary.read_bytes()
with header.open("w") as stream:
    stream.write(f"_Alignas(16) static const unsigned char {sys.argv[3]}[] = {{\n")
    for offset in range(0, len(data), 24):
        stream.write(",".join(map(str, data[offset:offset + 24])) + ",\n")
    stream.write("};\n")
PY
done
