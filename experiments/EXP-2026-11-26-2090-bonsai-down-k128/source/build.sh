#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2086 build directory}
reference=${3:?Specify EXP-2071 pipeline.cl}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_pq2_half_unpack_half2_17408_k128
mkdir -p "$out"

python3 - "$reference" "$out/component.cl" "$entry" <<'PY'
from pathlib import Path
import sys
import textwrap

reference, output = map(Path, sys.argv[1:3])
name = 'bonsai_pq2_half_unpack_half2_17408'
text = reference.read_text()
start = text.index(f'__kernel void {name}(')
end = text.index('\n}\n', start) + len('\n}\n')
kernel = text[start:end]
kernel = kernel.replace(name, sys.argv[3], 1)
buffers = '''    __local __attribute__((aligned(16))) uint w[64][36];
    __local __attribute__((aligned(16))) uint x[128][36];'''
assert kernel.count(buffers) == 1
kernel = kernel.replace(buffers, '    __local __attribute__((aligned(16))) uint paired[2][192][36];')
setup_start = kernel.index('    uint raw[4];')
mma_start = kernel.index('        #pragma unroll\n        for (uint segment=0;', setup_start)
mma_end = kernel.index('        sync_threads();\n    }', mma_start)
mma = kernel[mma_start:mma_end].replace('&w[warp_row][0]', '&paired[half_index][warp_row][0]')
mma = mma.replace('&x[warp_token+8*i][0]', '&paired[half_index][64+warp_token+8*i][0]')
setup = '''    for (uint block=0;block<blocks;++block) {
        #pragma unroll
        for (uint half_index=0;half_index<2;++half_index) {
            for (uint packed=thread*4;packed<128*32;packed+=1024) {
                const uint token=token_base+packed/32,col=packed%32;
                const ulong source=(ulong)token*8704+(2*block+half_index)*32+col;
                bonsai_copy16(&paired[half_index][64+packed/32][col],input+source,16);
            }
        }
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row;
            const uint row=row_base+index/16, quad=index%16;
            const __global uchar *q=weights+((ulong)row*blocks+block)*34;
            const ushort bits=*(__global const ushort *)q;
            const uint2 first=bonsai_pq2_half2(bits,q[2+quad]);
            const uint2 second=bonsai_pq2_half2(bits,q[18+quad]);
            *(__local uint2 *)&paired[0][index/16][quad*2]=first;
            *(__local uint2 *)&paired[1][index/16][quad*2]=second;
        }
        bonsai_copy_complete();
        sync_threads();
        #pragma unroll
        for (uint half_index=0;half_index<2;++half_index) {
'''
kernel = kernel[:setup_start] + setup + textwrap.indent(mma, '    ') + '        }\n' + kernel[mma_end:]
output.write_text(text + kernel)
PY

clang-18 -target nvptx64-nvidia-cuda -march=sm_80 -cl-std=CL2.0 -x cl -O3 -ffp-contract=off \
    -nocudainc -nocudalib -Xclang -finclude-default-header -Xclang -mlink-builtin-bitcode \
    -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc" -S "$out/component.cl" -o "$out/component.ptx"

python3 - "$base/pipeline.ptx" "$out/component.ptx" "$out/pipeline.ptx" "$entry" <<'PY'
from pathlib import Path
import re
import sys

base, component, output = map(Path, sys.argv[1:4])
name = sys.argv[4]
anchor = f'.visible .entry {name}('
target, donor = base.read_text(), component.read_text()
assert anchor not in target and donor.count(anchor) == 1
start = donor.index(anchor)
end = donor.index('\n}', start) + len('\n}')
entry = donor[start:end]
shared = name + '_$_paired'
entry, count = re.subn(r'^\s*\.shared \.align 16 \.b8 '+re.escape(shared)+r'\[55296\];$', '',
                       entry, flags=re.M)
assert count == 1 and '.extern .shared .align 16 .b8 bonsai_flash_shared[];' in target
entry = entry.replace(shared, 'bonsai_flash_shared')
output.write_text(target + '\n\n' + entry + '\n')
PY
"$cuda/bin/ptxas" -v -arch=sm_89 "$out/pipeline.ptx" -o "$out/pipeline.cubin" 2>"$out/ptxas.txt"
