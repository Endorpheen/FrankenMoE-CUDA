#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2096 build directory}
reference="$src/../../EXP-2026-11-25-2034-bonsai-mtp-device-argmax/source/argmax.cl"
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"

cat "$base/component.cl" "$reference" > "$out/component.cl"
clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/component.cl" -o "$out/component.ptx"

python3 - "$base/pipeline.ptx" "$out/component.ptx" "$out/pipeline.ptx" <<'PY'
from pathlib import Path
import sys

base, component, output = map(Path, sys.argv[1:4])
target, donor = base.read_text(), component.read_text()
for entry in ("bonsai_choice_tiles", "bonsai_choice_final"):
    anchor = f'.visible .entry {entry}('
    assert anchor not in target and donor.count(anchor) == 1
    start = donor.index(anchor)
    end = donor.index('\n}', start) + len('\n}')
    target += '\n\n' + donor[start:end] + '\n'
output.write_text(target)
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
