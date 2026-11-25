#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2081 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_pq2_mmq_int8_full_m128n128
mkdir -p "$out"

python3 - "$base/component.cl" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
name = 'bonsai_pq2_mmq_int8_full_n128_pairload'
text = reference.read_text()
start = text.index(f'__kernel void {name}(')
end = text.index('\n}\n', start) + len('\n}\n')
kernel = text[start:end]
for old, new in (
    (name, sys.argv[3]),
    ('w[64][36]', 'w[128][36]'),
    ('weight_scale[64]', 'weight_scale[128]'),
    ('row_base=64*work_group()', 'row_base=128*work_group()'),
    ('packed+=1024', 'packed+=2048'),
    ('if (thread<64)', 'if (thread<128)'),
    ('i<64*32;i+=512', 'i<128*32;i+=1024'),
):
    assert kernel.count(old) == 1, old
    kernel = kernel.replace(old, new)
output.write_text(text + kernel)
PY

clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/component.cl" -o "$out/component.ptx"

python3 - "$base/pipeline.ptx" "$out/component.ptx" "$out/pipeline.ptx" "$entry" <<'PY'
from pathlib import Path
import sys

base, component, output = map(Path, sys.argv[1:4])
anchor = f'.visible .entry {sys.argv[4]}('
target, donor = base.read_text(), component.read_text()
assert anchor not in target and donor.count(anchor) == 1
start = donor.index(anchor)
end = donor.index('\n}', start) + len('\n}')
output.write_text(target + '\n\n' + donor[start:end] + '\n')
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
