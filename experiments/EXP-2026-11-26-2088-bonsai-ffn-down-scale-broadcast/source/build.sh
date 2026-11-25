#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2086 build directory}
reference=${3:?Specify EXP-2071 pipeline.cl}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_pq2_half_unpack_half2_17408_broadcast
mkdir -p "$out"

python3 - "$reference" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
name = 'bonsai_pq2_half_unpack_half2_17408'
text = reference.read_text()
start = text.index(f'__kernel void {name}(')
end = text.index('\n}\n', start) + len('\n}\n')
kernel = text[start:end]
original = '                const uint bits=*(__global const ushort *)q;'
candidate = """                uint local_bits=0, bits;
                if (quad==0) local_bits=*(__global const ushort *)q;
                const uint row_lane=lane&16u;
                __asm__("shfl.sync.idx.b32 %0, %1, %2, 31, -1;"
                        : "=r"(bits) : "r"(local_bits), "r"(row_lane));"""
assert kernel.count(original) == 1
output.write_text(text + kernel.replace(name, sys.argv[3], 1).replace(original, candidate))
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
