#!/usr/bin/env bash
set -euo pipefail

out=$(realpath -m -- "${1:?Usage: build-gooseinfer-apex-up-gate-images.sh OUTPUT_DIR}")
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir="$tools/../integration/goose-standalone/universal/cuda/apex-extra/up-gate"
cuda=${CUDA_HOME:-/usr/local/cuda}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$out"

bash "$tools/build-gooseinfer-bonsai-ptx.sh" "$scratch/all.ptx" mmq "$source_dir"
for variant in fused shared; do
    case "$variant" in
        fused) entry=goose_mmvq_up_gate_glu; symbol=fused_up_gate_image ;;
        shared) entry=goose_mmvq_shared_up_gate_glu; symbol=fused_shared_image ;;
    esac
    python3 - "$scratch/all.ptx" "$scratch/kernel.ptx" "$entry" <<'PY'
from pathlib import Path
import re
import sys

source, output = map(Path, sys.argv[1:3])
name = sys.argv[3]
ptx = source.read_text()
entry = re.search(rf"^\.visible \.entry {re.escape(name)}\(.*?^}}", ptx, re.M | re.S)
if entry is None:
    raise ValueError(f"Missing Apex up/gate kernel {name}")
output.write_text("\n\n".join((*ptx.split("\n\n", 2)[:2], entry[0])) + "\n")
PY
    "$cuda/bin/ptxas" -arch=sm_89 "$scratch/kernel.ptx" -o "$scratch/$variant.cubin"
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
