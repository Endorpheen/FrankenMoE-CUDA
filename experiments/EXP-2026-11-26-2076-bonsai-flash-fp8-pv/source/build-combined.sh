#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a fresh candidate directory}
base=${2:?Specify EXP-2073 build directory}
component=${3:?Specify EXP-2076 component build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
python3 - "$base/pipeline.ptx" "$component/pipeline.ptx" "$out/pipeline.ptx" <<'PY'
from pathlib import Path
import sys
base, component, out = map(Path, sys.argv[1:])
target = base.read_text()
donor = component.read_text()
for entry in ("bonsai_flash_stage_v_fp8", "bonsai_flash_fp8_qkpv"):
    anchor = f".visible .entry {entry}("
    assert anchor not in target and donor.count(anchor) == 1
    start = donor.index(anchor)
    end = donor.index("\n}", start) + 2
    target += "\n\n" + donor[start:end] + "\n"
out.write_text(target)
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
python3 "$src/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin" > "$out/device-code.json"
bash "$src/build-http.sh" "$out" "$base"
