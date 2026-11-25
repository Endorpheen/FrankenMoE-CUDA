#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify a fresh candidate directory}
base=${2:?Specify EXP-2059 PTX}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir "$out"
python3 - "$base" "$out/pipeline.ptx" <<'PY'
from pathlib import Path
import sys

base, target = map(Path, sys.argv[1:])
ptx = base.read_text()
entry = "bonsai_pq2_cached_fp16_acc_k32"
start = ptx.index(f".visible .entry {entry}(")
stop = ptx.index("\n}", start) + 2
body = ptx[start:stop]
candidate = body.replace(entry, "bonsai_pq2_cached_fp16_cap80")
anchor = "\n{\n"
assert candidate.count(anchor) == 1
candidate = candidate.replace(anchor, "\n.reqntid 256, 1, 1\n.maxnreg 80\n{\n")
target.write_text(ptx + "\n\n" + candidate + "\n")
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
