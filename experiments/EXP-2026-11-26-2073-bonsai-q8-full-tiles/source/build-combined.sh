#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify a fresh candidate directory}
base=${2:?Specify EXP-2071 build directory}
component=${3:?Specify the component build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
python3 - "$base/pipeline.ptx" "$component/pipeline.ptx" "$out/pipeline.ptx" <<'PY'
from pathlib import Path
import sys
base, component, out = map(Path, sys.argv[1:])
target = base.read_text()
donor = component.read_text()
anchor = ".visible .entry bonsai_pq2_mmq_int8_full_n128("
assert anchor not in target and donor.count(anchor) == 1
start = donor.index(anchor)
end = donor.index("\n}", start) + 2
out.write_text(target + "\n\n" + donor[start:end] + "\n")
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
bash "$src/build-http.sh" "$out" "$base"
