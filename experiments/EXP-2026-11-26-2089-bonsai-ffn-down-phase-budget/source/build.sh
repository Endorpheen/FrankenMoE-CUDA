#!/usr/bin/env bash
set -euo pipefail

out=${1:?Specify the component output directory}
base=${2:?Specify EXP-2086 build directory}
reference=${3:?Specify EXP-2071 pipeline.cl}
cuda=${CUDA_HOME:-/usr/local/cuda}
entry=bonsai_pq2_half_unpack_half2_17408_clock
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
for old, new in (
    (name, sys.argv[3]),
    ('uint rows,uint tokens)', 'uint rows,uint tokens,__global ulong *counts)'),
    ('    for (uint step=0;step<2*blocks;++step) {', '''    const bool sample=thread==0 && work_group()%8==0 && flash_grid_y()%2==0;
    const uint selected=2*((7+3*(work_group()/8+10*(flash_grid_y()/2)))%136);
    for (uint step=0;step<2*blocks;++step) {
        const bool measure=sample && (step&~1u)==selected;
        ulong t0=0,t1=0,t2=0,t3=0,t4=0,t5=0;
        if (measure) t0=bonsai_down_clock();'''),
    ('        for (uint packed=thread*4;', '''        if (measure) t1=bonsai_down_clock();
        for (uint packed=thread*4;'''),
    ('''        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;''',
     '''        if (measure) t2=bonsai_down_clock();
        #pragma unroll
        for (uint lane_row=0;lane_row<4;++lane_row) {
            const uint index=thread+256*lane_row, quad=index%16;'''),
    ('''        bonsai_copy_complete();
        sync_threads();''',
     '''        if (measure) t3=bonsai_down_clock();
        bonsai_copy_complete();
        sync_threads();
        if (measure) t4=bonsai_down_clock();'''),
    ('''        sync_threads();
    }
    const uint row=row_base''',
     '''        if (measure) t5=bonsai_down_clock();
        sync_threads();
        if (measure) {
            const ulong t6=bonsai_down_clock();
            const uint cta=flash_grid_y()*(rows/64)+work_group();
            __global ulong *result=counts+((ulong)cta*2+(step&1))*6;
            result[0]=t1-t0;
            result[1]=t2-t1;
            result[2]=t3-t2;
            result[3]=t4-t3;
            result[4]=t5-t4;
            result[5]=t6-t5;
        }
    }
    const uint row=row_base'''),
):
    assert kernel.count(old) == 1, old
    kernel = kernel.replace(old, new)
clock = '''
static ulong bonsai_down_clock(void) {
    ulong clock_value;
    __asm__ volatile("mov.u64 %0, %%clock64;" : "=l"(clock_value) :: "memory");
    return clock_value;
}
'''
output.write_text(text + clock + kernel)
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
