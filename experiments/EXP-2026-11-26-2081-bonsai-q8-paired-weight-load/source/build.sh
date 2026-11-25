#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2076 build directory}
reference=${3:?Specify the EXP-2073 pipeline.cl}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_pq2_mmq_int8_full_n128_pairload
mkdir -p "$out"

python3 - "$reference" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
entry = sys.argv[3]
name = 'bonsai_pq2_mmq_int8_full_n128'
text = reference.read_text()
start = text.index(f'__kernel void {name}(')
end = text.index('\n}\n', start) + len('\n}\n')
original = text[start:end]
scalar = """        for (uint i=thread;i<64*32;i+=256) {
            const uint r=row_base+i/32;
            const uint code=weights[((ulong)r*blocks+block)*34+2+i%32];
            w[i/32][i%32]=bonsai_expand_pq2_byte(code);
        }"""
paired = """        for (uint i=2*thread;i<64*32;i+=512) {
            const uint r=row_base+i/32;
            const __global ushort *codes=(const __global ushort *)(
                weights+((ulong)r*blocks+block)*34+2+i%32);
            const uint pair=*codes;
            const uint2 unpacked=(uint2)(bonsai_expand_pq2_byte(pair&255u),
                                         bonsai_expand_pq2_byte(pair>>8));
            *(__local uint2 *)&w[i/32][i%32]=unpacked;
        }"""
assert original.count(scalar) == 1
output.write_text(text + original.replace(name, entry, 1).replace(scalar, paired))
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
