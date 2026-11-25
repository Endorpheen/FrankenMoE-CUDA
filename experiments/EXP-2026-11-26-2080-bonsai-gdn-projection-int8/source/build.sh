#!/usr/bin/env bash
set -euo pipefail

src=$(cd "$(dirname "$0")" && pwd)
out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2076 build directory}
gate_source=${3:?Specify EXP-2073 pipeline.cl}
cuda=${CUDA_HOME:-/usr/local/cuda}
mkdir -p "$out"

python3 - "$gate_source" "$out/component.cl" <<'PY'
from pathlib import Path
import sys

source, output = map(Path, sys.argv[1:])
text = source.read_text()
entry = 'bonsai_pq2_mmq_int8_full_n128'
start = text.index(f'__kernel void {entry}(')
end = text.index('\n}\n', start) + len('\n}\n')
original = text[start:end]
assert original.count('rows=17408,blocks=40') == 1
with output.open('w') as stream:
    stream.write(text)
    for rows in (6144, 10240):
        stream.write(original.replace(entry, f'{entry}_{rows}')
                     .replace('rows=17408,blocks=40', f'rows={rows},blocks=40'))
PY

clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/component.cl" -o "$out/component.ptx"

python3 - "$base/pipeline.ptx" "$out/component.ptx" "$out/pipeline.ptx" <<'PY'
from pathlib import Path
import sys

base, component, output = map(Path, sys.argv[1:])
target, donor = base.read_text(), component.read_text()
for rows in (6144, 10240):
    anchor = f'.visible .entry bonsai_pq2_mmq_int8_full_n128_{rows}('
    assert anchor not in target and donor.count(anchor) == 1
    start = donor.index(anchor)
    end = donor.index('\n}', start) + len('\n}')
    target += '\n\n' + donor[start:end] + '\n'
output.write_text(target)
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
