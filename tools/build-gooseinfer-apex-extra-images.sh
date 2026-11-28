#!/usr/bin/env bash
set -euo pipefail

out=$(realpath -m -- "${1:?Usage: build-gooseinfer-apex-extra-images.sh OUTPUT_DIR}")
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
standalone="$tools/../integration/goose-standalone/universal"
src="$standalone/cuda/apex-extra"
cuda=${CUDA_HOME:-/usr/local/cuda}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$out"

for variant in fwht-fixed rms-onevec flash-interior moe-epilogue; do
    case "$variant" in
        fwht-fixed) entries=goose_kv_fwht_64,goose_kv_fwht_256,goose_fwht_q4_64,goose_fwht_q4_256 ;;
        rms-onevec) entries=goose_rms_reference_one,goose_rms_reference_one_warp4 ;;
        flash-interior) entries=goose_flash_causal_mma ;;
        moe-epilogue) entries=goose_moe_shared_residual_prefill ;;
    esac
    bash "$tools/build-gooseinfer-bonsai-ptx.sh" "$scratch/module.ptx" mmq "$src/$variant"
    "$cuda/bin/ptxas" -arch=sm_89 --entry "$entries" "$scratch/module.ptx" -o "$scratch/module.cubin"
    python3 - "$scratch/module.cubin" "$out/$variant-image.h" "${variant//-/_}_image" <<'PY'
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
