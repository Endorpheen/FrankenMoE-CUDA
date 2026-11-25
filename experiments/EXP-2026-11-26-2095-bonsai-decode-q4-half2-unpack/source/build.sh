#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2094 build directory}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_decode_direct_q4_mma_half2
mkdir -p "$out"

python3 - "$base/component.cl" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys

reference, output = map(Path, sys.argv[1:3])
name = 'bonsai_decode_direct_q4_mma'
text = reference.read_text()
start = text.index(f'__kernel void {name}(')
end = text.index('\n}\n', start) + len('\n}\n')
kernel = text[start:end]
assert kernel.count('local_flash_pack_q4_pair(') == 2
unpack = '''
static inline uint decode_pack_q4_half2(__local const uchar *row, uint pair) {
    const uint block=pair/16, index=pair%16;
    const __local ushort *data=(__local const ushort *)(row+18*block);
    const uint bits=data[0];
    const uint bytes=(uint)data[1+(index%8)];
    const uint q=(bytes>>(index>=8 ? 4 : 0))&0x0f0fu;
    const uint biased=0x64006400u|(q&15u)|((q&0x0f00u)<<8);
    const uint scale=bits|(bits<<16);
    uint values, result;
    __asm__("sub.rn.f16x2 %0, %1, %2;" : "=r"(values) : "r"(biased), "r"(0x64086408u));
    __asm__("mul.rn.f16x2 %0, %1, %2;" : "=r"(result) : "r"(values), "r"(scale));
    return result;
}
'''
output.write_text(text + unpack + kernel.replace(name, sys.argv[3], 1)
                  .replace('local_flash_pack_q4_pair(', 'decode_pack_q4_half2('))
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
