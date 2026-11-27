#!/usr/bin/env bash
set -euo pipefail

out=$(realpath -m -- "${1:?Usage: build-gooseinfer-apex-attention-images.sh OUTPUT_DIR}")
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
source_dir="$tools/../integration/goose-standalone/universal/cuda/apex-attention"
cuda=${CUDA_HOME:-/usr/local/cuda}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$out"

for variant in tail first-row unroll; do
    case "$variant" in
        tail) header=attention-cubin.h; symbol=attention_key_bound_image ;;
        first-row) header=attention-first-row-image.h; symbol=attention_first_row_image ;;
        unroll) header=attention-unroll-image.h; symbol=attention_unroll_image ;;
    esac
    bash "$tools/build-gooseinfer-bonsai-ptx.sh" "$scratch/all.ptx" mmq "$source_dir/$variant.patch"
    python3 - "$scratch/all.ptx" "$scratch/attention.ptx" <<'PY'
from pathlib import Path
import re
import sys

source, output = map(Path, sys.argv[1:])
ptx = source.read_text()
header = ptx.split("\n\n", 2)[:2]
entry = re.search(r"^\.visible \.entry goose_flash_vec_q4_packed_quad\(.*?^}", ptx, re.M | re.S)
if entry is None:
    raise ValueError("Missing Apex Q4 packed attention entry")
output.write_text("\n\n".join((*header, entry[0])) + "\n")
PY
    "$cuda/bin/ptxas" -arch=sm_89 "$scratch/attention.ptx" -o "$scratch/attention.cubin"
    python3 - "$scratch/attention.cubin" "$out/$header" "$symbol" <<'PY'
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
